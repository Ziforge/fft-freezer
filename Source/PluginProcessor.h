#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <random>
#include <atomic>

class FFTFreezerProcessor : public juce::AudioProcessor
{
public:
    FFTFreezerProcessor();
    ~FFTFreezerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    // --- Freeze API ---
    void triggerFreeze();
    void writeToDisk();

    // State queries for the editor
    bool isFrozen() const { return frozen.load(); }
    bool isBusy() const { return busy.load(); }
    int getLoopLength() const { return loopLength.load(); }

    // Parameters
    juce::AudioParameterFloat* threshParam;
    juce::AudioParameterFloat* mixParam;
    juce::AudioParameterFloat* recLenParam;

private:
    void performFreeze();

    // FFT
    static constexpr int maxOrder = 16; // up to 65536 samples
    int fftOrder = 14;                  // default 16384

    // Recording ring buffer
    std::vector<float> ringBuffer;
    int ringIndex = 0;
    int ringSize = 0;

    // Frozen loop
    std::vector<float> loopBuffer;
    std::atomic<int> loopLength { 0 };
    int loopPlayhead = 0;

    // Fade window for recording
    std::vector<float> window;

public:
    std::atomic<bool> freezeRequested { false };
private:
    std::atomic<bool> frozen { false };
    std::atomic<bool> busy { false };

    double currentSampleRate = 44100.0;
    std::mt19937 rng;

    // Thread for freeze computation
    std::unique_ptr<juce::Thread> freezeThread;

    juce::CriticalSection freezeLock;

    std::shared_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FFTFreezerProcessor)
};
