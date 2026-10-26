#pragma once
#include <JuceHeader.h>

namespace flowtone
{
struct AnalysisSnapshot
{
    int key = -1;
    bool minor = false, complete = false;
    double bpm = 0.0, seconds = 0.0;
    std::array<double,12> pitchClasses {}, bassPitchClasses {};
};
class TrackAnalysis
{
public:
    TrackAnalysis();
    ~TrackAnalysis();
    void prepare(double sourceRate);
    void stop();
    void push(const juce::AudioBuffer<float>&);
    void requestReset();
    AnalysisSnapshot snapshot() const;
    int pendingSamples() const;
    static juce::String keyName(int key, bool minor);
    // Non-realtime: bounded-memory decoding of the entire file on the caller's worker.
    static AnalysisSnapshot analyseFile(juce::AudioFormatReader&, std::array<float,512>& waveform,
        const std::function<bool(double)>& progress);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
