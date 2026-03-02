#include "PluginEditor.h"

static const juce::Colour bgColour       (0xFF1A1A2E);
static const juce::Colour accentColour   (0xFF0F3460);
static const juce::Colour recColour      (0xFFCC2222);
static const juce::Colour recActiveColour(0xFFFF4444);
static const juce::Colour playColour     (0xFF22AA44);
static const juce::Colour textColour     (0xFFEEEEEE);

//==============================================================================
FFTFreezerEditor::FFTFreezerEditor (FFTFreezerProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (420, 370);

    // REC button
    recButton.setColour (juce::TextButton::buttonColourId, recColour);
    recButton.setColour (juce::TextButton::textColourOffId, textColour);
    recButton.onClick = [this] { proc.startRecording(); };
    addAndMakeVisible (recButton);

    // Write to disk button
    writeButton.setColour (juce::TextButton::buttonColourId, accentColour);
    writeButton.setColour (juce::TextButton::textColourOffId, textColour);
    writeButton.onClick = [this] { proc.writeToDisk(); };
    writeButton.setEnabled (false);
    addAndMakeVisible (writeButton);

    // Progress bar
    progressBar.setColour (juce::ProgressBar::foregroundColourId, recActiveColour);
    addAndMakeVisible (progressBar);

    // Threshold slider
    threshSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    threshSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    threshSlider.setRange (0.0, 0.01, 0.0001);
    threshSlider.setValue (proc.threshParam->get());
    threshSlider.onValueChange = [this] { *proc.threshParam = (float)threshSlider.getValue(); };
    threshSlider.setColour (juce::Slider::rotarySliderFillColourId, recColour);
    threshSlider.setColour (juce::Slider::thumbColourId, recColour);
    addAndMakeVisible (threshSlider);
    threshLabel.setText ("Threshold", juce::dontSendNotification);
    threshLabel.setJustificationType (juce::Justification::centred);
    threshLabel.setColour (juce::Label::textColourId, textColour);
    addAndMakeVisible (threshLabel);

    // Mix slider
    mixSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    mixSlider.setRange (0.0, 1.0, 0.01);
    mixSlider.setValue (proc.mixParam->get());
    mixSlider.onValueChange = [this] { *proc.mixParam = (float)mixSlider.getValue(); };
    mixSlider.setColour (juce::Slider::rotarySliderFillColourId, recColour);
    mixSlider.setColour (juce::Slider::thumbColourId, recColour);
    addAndMakeVisible (mixSlider);
    mixLabel.setText ("Mix", juce::dontSendNotification);
    mixLabel.setJustificationType (juce::Justification::centred);
    mixLabel.setColour (juce::Label::textColourId, textColour);
    addAndMakeVisible (mixLabel);

    // Record length slider
    recLenSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    recLenSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    recLenSlider.setRange (0.1, 10.0, 0.1);
    recLenSlider.setValue (proc.recLenParam->get());
    recLenSlider.setTextValueSuffix (" s");
    recLenSlider.onValueChange = [this] { *proc.recLenParam = (float)recLenSlider.getValue(); };
    recLenSlider.setColour (juce::Slider::rotarySliderFillColourId, recColour);
    recLenSlider.setColour (juce::Slider::thumbColourId, recColour);
    addAndMakeVisible (recLenSlider);
    recLenLabel.setText ("Rec Length", juce::dontSendNotification);
    recLenLabel.setJustificationType (juce::Justification::centred);
    recLenLabel.setColour (juce::Label::textColourId, textColour);
    addAndMakeVisible (recLenLabel);

    // Status label
    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setColour (juce::Label::textColourId, textColour.withAlpha (0.7f));
    addAndMakeVisible (statusLabel);

    startTimerHz (30);
}

FFTFreezerEditor::~FFTFreezerEditor() {}

//==============================================================================
void FFTFreezerEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);

    g.setColour (textColour);
    g.setFont (juce::FontOptions (22.0f));
    g.drawText ("FFT FREEZER", getLocalBounds().removeFromTop (40), juce::Justification::centred);
}

void FFTFreezerEditor::resized()
{
    auto area = getLocalBounds().reduced (15);
    area.removeFromTop (35);

    // Buttons row
    auto buttonRow = area.removeFromTop (45);
    recButton.setBounds (buttonRow.removeFromLeft (buttonRow.getWidth() / 2).reduced (4));
    writeButton.setBounds (buttonRow.reduced (4));

    // Progress bar
    area.removeFromTop (4);
    progressBar.setBounds (area.removeFromTop (14).reduced (4, 0));

    area.removeFromTop (6);

    // Knobs row
    auto knobRow = area.removeFromTop (130);
    int knobW = knobRow.getWidth() / 3;

    auto col1 = knobRow.removeFromLeft (knobW);
    threshLabel.setBounds (col1.removeFromTop (20));
    threshSlider.setBounds (col1);

    auto col2 = knobRow.removeFromLeft (knobW);
    mixLabel.setBounds (col2.removeFromTop (20));
    mixSlider.setBounds (col2);

    auto col3 = knobRow;
    recLenLabel.setBounds (col3.removeFromTop (20));
    recLenSlider.setBounds (col3);

    area.removeFromTop (5);
    statusLabel.setBounds (area.removeFromTop (25));
}

//==============================================================================
void FFTFreezerEditor::timerCallback()
{
    auto st = proc.getState();

    switch (st)
    {
        case FFTFreezerProcessor::Idle:
            recButton.setButtonText ("REC");
            recButton.setColour (juce::TextButton::buttonColourId, recColour);
            recButton.setEnabled (true);
            writeButton.setEnabled (false);
            progressValue = 0.0;
            statusLabel.setText ("Press REC to capture audio", juce::dontSendNotification);
            break;

        case FFTFreezerProcessor::Recording:
            recButton.setButtonText ("RECORDING...");
            recButton.setColour (juce::TextButton::buttonColourId, recActiveColour);
            recButton.setEnabled (false);
            writeButton.setEnabled (false);
            progressValue = (double)proc.getRecordProgress();
            statusLabel.setText ("Recording...", juce::dontSendNotification);
            break;

        case FFTFreezerProcessor::Freezing:
            recButton.setButtonText ("FREEZING...");
            recButton.setEnabled (false);
            writeButton.setEnabled (false);
            progressValue = -1.0; // indeterminate
            statusLabel.setText ("Computing FFT freeze...", juce::dontSendNotification);
            break;

        case FFTFreezerProcessor::Playing:
        {
            recButton.setButtonText ("REC");
            recButton.setColour (juce::TextButton::buttonColourId, playColour);
            recButton.setEnabled (true);
            writeButton.setEnabled (true);
            progressValue = 0.0;
            int len = proc.getLoopLength();
            double secs = (double)len / proc.getSampleRate();
            statusLabel.setText ("Playing frozen loop: " + juce::String (len) + " samples ("
                                + juce::String (secs, 2) + "s)", juce::dontSendNotification);
            break;
        }
    }

    repaint();
}
