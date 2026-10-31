#include "PitchEngine.h"
#include "signalsmith-stretch.h"
#include <cmath>

namespace flowtone
{
struct PitchEngine::Impl
{
    std::array<signalsmith::stretch::SignalsmithStretch<float>, 2> stretch;
    juce::AudioBuffer<float> input, wet[2], dryDelay, wetDelay[2];
    juce::SmoothedValue<float> pitch, quality, mix, gain;
    std::array<int, 2> engineLatency {}, delayPosition {}, warmRemaining {};
    std::array<bool, 2> running {};
    int warmupSamples = 0;
    int channels = 0, commonLatency = 0, dryPosition = 0;
    double rate = 48000.0;
    bool primed = false;
    static constexpr int quantum = 128;
};
PitchEngine::PitchEngine() : impl(std::make_unique<Impl>()) {}
PitchEngine::~PitchEngine() = default;
void PitchEngine::prepare(double sampleRate, int channels)
{
    auto& p = *impl; p.channels = channels; p.rate = sampleRate;
    p.stretch[0].presetCheaper(channels, sampleRate, true);
    const int highBlock=juce::nextPowerOfTwo(juce::jmax(1024,juce::roundToInt(sampleRate*0.16)));
    p.stretch[1].configure(channels, highBlock, highBlock/16, true);
    for (int i = 0; i < 2; ++i)
    {
        p.engineLatency[static_cast<size_t>(i)] = p.stretch[static_cast<size_t>(i)].inputLatency() + p.stretch[static_cast<size_t>(i)].outputLatency();
        p.stretch[static_cast<size_t>(i)].setFormantBase(static_cast<float>(180.0 / sampleRate));
    }
    p.commonLatency = juce::jmax(p.engineLatency[0], p.engineLatency[1]);
    p.warmupSamples = p.commonLatency + highBlock;
    p.input.setSize(channels, Impl::quantum);
    p.dryDelay.setSize(channels, p.commonLatency + 1);
    for (int i = 0; i < 2; ++i)
    {
        p.wet[i].setSize(channels, Impl::quantum);
        p.wetDelay[i].setSize(channels, p.commonLatency - p.engineLatency[static_cast<size_t>(i)] + 1);
    }
    p.pitch.reset(sampleRate, 0.025); p.quality.reset(sampleRate, 0.04);
    p.mix.reset(sampleRate, 0.025); p.gain.reset(sampleRate, 0.02);
    reset();
}
void PitchEngine::reset()
{
    auto& p = *impl;
    for (auto& s : p.stretch) s.reset();
    p.dryDelay.clear(); for (auto& d : p.wetDelay) d.clear();
    p.dryPosition = 0; p.delayPosition = {}; p.running = {}; p.warmRemaining = {}; p.primed = false;
}
int PitchEngine::latency() const { return impl->commonLatency; }
void PitchEngine::process(juce::AudioBuffer<float>& buffer, float semitones, bool formants,
                         bool highQuality, bool after, bool bypass, float gainDb)
{
    auto& p = *impl;
    if (p.channels == 0) return;
    const float targetPitch = juce::jlimit(-12.0f, 12.0f, std::round(semitones * 2.0f) * 0.5f);
    const float targetMix = !bypass && after && std::abs(targetPitch) > 0.001f ? 1.0f : 0.0f;
    const float targetGain = bypass ? 1.0f : juce::Decibels::decibelsToGain(gainDb);
    if (!p.primed)
    {
        p.pitch.setCurrentAndTargetValue(targetPitch); p.mix.setCurrentAndTargetValue(targetMix);
        p.gain.setCurrentAndTargetValue(targetGain); p.quality.setCurrentAndTargetValue(highQuality ? 1.0f : 0.0f); p.running[highQuality ? 1u : 0u] = true; p.primed = true;
    }
    p.pitch.setTargetValue(targetPitch); p.mix.setTargetValue(targetMix);
    p.gain.setTargetValue(targetGain);
    for (int offset = 0; offset < buffer.getNumSamples(); offset += Impl::quantum)
    {
        const int count = juce::jmin(Impl::quantum, buffer.getNumSamples() - offset);
        for (int c = 0; c < p.channels; ++c)
            for (int n = 0; n < count; ++n)
            {
                const auto sample = buffer.getSample(c, offset + n);
                p.input.setSample(c, n, std::isfinite(sample) ? sample : 0.0f);
            }
        const auto currentPitch = p.pitch.skip(count);
        const int wanted = highQuality ? 1 : 0;
        for (int i = 0; i < 2; ++i)
        {
            const auto index = static_cast<size_t>(i);
            const float q = p.quality.getCurrentValue();
            const bool contributes = i == 0 ? q < 1.0f : q > 0.0f;
            if (i != wanted && !contributes)
            {
                p.running[index] = false;
                p.wet[i].clear();
                continue;
            }
            auto& s = p.stretch[index];
            if (!p.running[index])
            {
                s.reset(); p.wetDelay[i].clear(); p.delayPosition[index] = 0;
                p.warmRemaining[index] = p.warmupSamples;
                p.running[index] = true;
            }
            s.setTransposeSemitones(currentPitch);
            s.setFormantFactor(1.0f, formants);
            s.process(p.input.getArrayOfReadPointers(), count, p.wet[i].getArrayOfWritePointers(), count);
            p.warmRemaining[index] = juce::jmax(0, p.warmRemaining[index] - count);
        }
        // Keep the current engine audible until the requested one has valid
        // history. Only the transition runs both; host latency stays constant.
        if (p.warmRemaining[static_cast<size_t>(wanted)] == 0)
            p.quality.setTargetValue(highQuality ? 1.0f : 0.0f);
        for (int n = 0; n < count; ++n)
        {
            const float q = p.quality.getNextValue(), mix = p.mix.getNextValue(), gain = p.gain.getNextValue();
            for (int c = 0; c < p.channels; ++c)
            {
                const float dry = p.dryDelay.getSample(c, p.dryPosition);
                p.dryDelay.setSample(c, p.dryPosition, p.input.getSample(c, n));
                float values[2] {};
                for (int i = 0; i < 2; ++i)
                {
                    const int delay = p.commonLatency - p.engineLatency[static_cast<size_t>(i)];
                    const int pos = p.delayPosition[static_cast<size_t>(i)];
                    values[i] = delay == 0 ? p.wet[i].getSample(c, n) : p.wetDelay[i].getSample(c, pos);
                    p.wetDelay[i].setSample(c, pos, p.wet[i].getSample(c, n));
                }
                const float wet = values[0] + q * (values[1] - values[0]);
                const float value = (dry + mix * (wet - dry)) * gain;
                buffer.setSample(c, offset + n, std::isfinite(value) ? value : 0.0f);
            }
            // Read-before-write rings have exactly length samples of delay.
            if (++p.dryPosition >= p.commonLatency) p.dryPosition = 0;
            for (int i = 0; i < 2; ++i)
                if (++p.delayPosition[static_cast<size_t>(i)] >= juce::jmax(1, p.commonLatency - p.engineLatency[static_cast<size_t>(i)]))
                    p.delayPosition[static_cast<size_t>(i)] = 0;
        }
    }
}
}
