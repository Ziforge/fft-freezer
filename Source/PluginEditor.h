#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class FFTFreezerEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    FFTFreezerEditor (FFTFreezerProcessor&);
    ~FFTFreezerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    FFTFreezerProcessor& proc;

    juce::TextButton freezeButton { "SAMPLE" };
    juce::TextButton writeButton  { "WRITE TO DISK" };

    juce::Slider threshSlider;
    juce::Slider mixSlider;
    juce::Slider recLenSlider;

    juce::Label threshLabel, mixLabel, recLenLabel, statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FFTFreezerEditor)
};
