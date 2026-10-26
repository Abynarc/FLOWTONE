#pragma once
#include "TrackAnalysis.h"
#include <condition_variable>
#include <thread>

namespace flowtone
{
struct FileAnalysisSnapshot
{
    AnalysisSnapshot result;
    std::array<float,512> waveform {};
    juce::String name, path, error;
    double duration=0, sampleRate=0;
    int channels=0;
    float progress=0;
    bool busy=false, available=false;
};

class FileAnalysis
{
public:
    explicit FileAnalysis(std::function<void()> stateChanged={});
    ~FileAnalysis();
    void stop();
    void load(const juce::File&, std::shared_ptr<void> lease={}, std::shared_ptr<juce::InputStream> stream={});
    void repeat();
    void restore(FileAnalysisSnapshot);
    FileAnalysisSnapshot snapshot() const;
private:
    struct Request { juce::File file; std::shared_ptr<void> lease; unsigned revision=0; std::shared_ptr<juce::InputStream> stream; };
    void run();
    mutable std::mutex mutex;
    std::condition_variable ready;
    std::atomic<bool> exiting {false};
    std::atomic<unsigned> revision {0};
    bool pending=false;
    Request request;
    FileAnalysisSnapshot current;
    const std::function<void()> stateChanged;
    std::thread worker;
};
}
