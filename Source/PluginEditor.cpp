#include "PluginEditor.h"

static const juce::Colour bgColour       (0xFF1A1A2E);
static const juce::Colour panelColour    (0xFF16213E);
static const juce::Colour accentColour   (0xFF0F3460);
static const juce::Colour highlightColour(0xFFE94560);
static const juce::Colour textColour     (0xFFEEEEEE);

//==============================================================================
FFTFreezerEditor::FFTFreezerEditor (FFTFreezerProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (420, 340);

    // Freeze / Sample button
    freezeButton.setColour (juce::TextButton::buttonColourId, highlightColour);
    freezeButton.setColour (juce::TextButton::textColourOffId, textColour);
    freezeButton.onClick = [this] { proc.freezeRequested.store (true); };
    addAndMakeVisible (freezeButton);

    // Write to disk button
    writeButton.setColour (juce::TextButton::buttonColourId, accentColour);
    writeButton.setColour (juce::TextButton::textColourOffId, textColour);
    writeButton.onClick = [this] { proc.writeToDisk(); };
    addAndMakeVisible (writeButton);

    // Threshold slider
    threshSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    threshSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    threshSlider.setRange (0.0, 0.01, 0.0001);
    threshSlider.setValue (proc.threshParam->get());
    threshSlider.onValueChange = [this] { *proc.threshParam = (float)threshSlider.getValue(); };
    threshSlider.setColour (juce::Slider::rotarySliderFillColourId, highlightColour);
    threshSlider.setColour (juce::Slider::thumbColourId, highlightColour);
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
    mixSlider.setColour (juce::Slider::rotarySliderFillColourId, highlightColour);
    mixSlider.setColour (juce::Slider::thumbColourId, highlightColour);
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
    recLenSlider.setColour (juce::Slider::rotarySliderFillColourId, highlightColour);
    recLenSlider.setColour (juce::Slider::thumbColourId, highlightColour);
    addAndMakeVisible (recLenSlider);
    recLenLabel.setText ("Rec Length", juce::dontSendNotification);
    recLenLabel.setJustificationType (juce::Justification::centred);
    recLenLabel.setColour (juce::Label::textColourId, textColour);
    addAndMakeVisible (recLenLabel);

    // Status label
    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setColour (juce::Label::textColourId, textColour.withAlpha (0.6f));
    addAndMakeVisible (statusLabel);

    startTimerHz (15);
}

FFTFreezerEditor::~FFTFreezerEditor() {}

//==============================================================================
void FFTFreezerEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);

    // Title
    g.setColour (textColour);
    g.setFont (juce::Font (22.0f));
    g.drawText ("FFT FREEZER", getLocalBounds().removeFromTop (40), juce::Justification::centred);
}

void FFTFreezerEditor::resized()
{
    auto area = getLocalBounds().reduced (15);
    area.removeFromTop (35); // title space

    // Buttons row
    auto buttonRow = area.removeFromTop (45);
    freezeButton.setBounds (buttonRow.removeFromLeft (buttonRow.getWidth() / 2).reduced (4));
    writeButton.setBounds (buttonRow.reduced (4));

    area.removeFromTop (10);

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
    if (proc.isBusy())
    {
        statusLabel.setText ("Freezing...", juce::dontSendNotification);
        freezeButton.setEnabled (false);
    }
    else
    {
        freezeButton.setEnabled (true);
        if (proc.isFrozen())
        {
            int len = proc.getLoopLength();
            double secs = len / 44100.0; // approximate
            statusLabel.setText ("Frozen loop: " + juce::String (len) + " samples ("
                                + juce::String (secs, 2) + "s)", juce::dontSendNotification);
        }
        else
        {
            statusLabel.setText ("Ready - press SAMPLE to freeze", juce::dontSendNotification);
        }
    }
}
