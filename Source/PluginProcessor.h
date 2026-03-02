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

    // --- Public API for editor ---
    void startRecording();
    void writeToDisk();

    enum State { Idle, Recording, Freezing, Playing };
    State getState() const { return state.load(); }
    float getRecordProgress() const;
    int getLoopLength() const { return loopLength.load(); }
    double getSampleRate() const { return currentSampleRate; }

    // Parameters
    juce::AudioParameterFloat* threshParam;
    juce::AudioParameterFloat* mixParam;
    juce::AudioParameterFloat* recLenParam;

private:
    void performFreeze();

    // FFT
    static constexpr int maxOrder = 17; // up to 131072 samples

    // Recording buffer (filled during Recording state)
    std::vector<float> recBuffer;
    std::atomic<int> recWritePos { 0 };
    int recTargetLength = 0;

    // Frozen loop (double-buffered: audio reads A, freeze writes B, then swap)
    std::vector<float> loopA, loopB;
    std::atomic<std::vector<float>*> activeLoop { nullptr };
    std::atomic<int> loopLength { 0 };
    int loopPlayhead = 0;

    // State machine
    std::atomic<State> state { Idle };
    std::atomic<bool> freezeDone { false };

    double currentSampleRate = 44100.0;
    std::mt19937 rng;

    std::shared_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FFTFreezerProcessor)
};
