#pragma once
#include <JuceHeader.h>

namespace flowtone
{
class PitchEngine
{
public:
    PitchEngine();
    ~PitchEngine();
    void prepare(double sampleRate, int channels);
    void reset();
    void process(juce::AudioBuffer<float>&, float semitones, bool preserveFormants,
                 bool highQuality, bool after, bool bypass, float gainDb);
    int latency() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
