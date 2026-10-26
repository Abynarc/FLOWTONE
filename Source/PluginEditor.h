#pragma once
#include "PluginProcessor.h"
#include "NativeRegionDrop.h"

class ToneLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
    void drawButtonText(juce::Graphics&,juce::TextButton&,bool,bool) override;
    void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override;
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
};
class FLOWTONEAudioProcessorEditor final : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit FLOWTONEAudioProcessorEditor(FLOWTONEAudioProcessor&);
    ~FLOWTONEAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
private:
    void timerCallback() override;
    void applyPreset(int);
    void showHelp();
    void chooseFile();
    juce::Rectangle<int> area(int,int,int,int) const;
    FLOWTONEAudioProcessor& processor;
    ToneLookAndFeel look;
    juce::TooltipWindow tooltips {this,500};
    juce::Slider shift,gain;
    juce::ToggleButton formants;
    juce::ComboBox quality,presets;
    juce::TextButton down,up,resetShift,half,doubleTempo,repeat,before,after,bypass,profileA,profileB,settings,previous,next;
    juce::TextButton liveSource,fileSource,openFile;
    flowtone::NativeRegionDrop nativeDrop;
    std::unique_ptr<juce::FileChooser> chooser;
    bool dropHover=false,receivingFile=false;
    unsigned dropTicket=0;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shiftAttachment,gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> formantAttachment,bypassAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FLOWTONEAudioProcessorEditor)
};
