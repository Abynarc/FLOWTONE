#pragma once
#include <JuceHeader.h>
#include "PitchEngine.h"
#include "TrackAnalysis.h"
#include "FileAnalysis.h"
#include <mutex>

class FLOWTONEAudioProcessor final : public juce::AudioProcessor
{
public:
    FLOWTONEAudioProcessor();
    ~FLOWTONEAudioProcessor() override;
    void prepareToPlay(double,int) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "FLOWTONE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return getSampleRate()>0 ? (engine.latency()+4096)/getSampleRate() : 0.3; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return parameters.getParameter("bypass"); }

    juce::AudioProcessorValueTreeState parameters;
    float value(const char* id) const { return parameters.getRawParameterValue(id)->load(); }
    void setValue(const char* id,float value);
    flowtone::TrackAnalysis analysis;
    flowtone::FileAnalysis fileAnalysis;
    bool usesFileAnalysis() const { return fileSource.load(); }
    void selectAnalysisSource(bool file);
    void loadAnalysisFile(const juce::File&,std::shared_ptr<void> lease={},std::shared_ptr<juce::InputStream> stream={});
    flowtone::AnalysisSnapshot analysisSnapshot() const;
    float inputLevel() const { return inputPeak.load(); }
    std::array<float,512> waveform() const;
    void selectProfile(int);
    int activeProfile() const { return currentProfile.load(); }
    juce::String resultKey() const;
    void resetAnalysis();
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void process(juce::AudioBuffer<float>&, bool forcedBypass);
    flowtone::PitchEngine engine;
    std::array<std::atomic<float>,512> history {};
    std::atomic<unsigned> historyWrite {0};
    std::atomic<float> inputPeak {0};
    int waveCount=0, waveInterval=1125;
    float wavePeak=0;
    std::array<std::array<float,4>,2> profiles {{{0,1,1,0},{0,1,1,0}}};
    std::atomic<int> currentProfile {0};
    std::recursive_mutex stateMutex;
    std::atomic<float> *rawShift=nullptr,*rawFormants=nullptr,*rawQuality=nullptr,*rawAfter=nullptr,*rawBypass=nullptr,*rawGain=nullptr;
    std::atomic<bool> resetRequested {false};
    std::atomic<bool> fileSource {false};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FLOWTONEAudioProcessor)
};
