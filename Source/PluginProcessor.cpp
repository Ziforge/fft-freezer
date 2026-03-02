#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <thread>

//==============================================================================
FFTFreezerProcessor::FFTFreezerProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      rng (std::random_device{}())
{
    addParameter (threshParam = new juce::AudioParameterFloat (
        "thresh", "Threshold", 0.0f, 0.01f, 0.0f));
    addParameter (mixParam = new juce::AudioParameterFloat (
        "mix", "Freeze Mix", 0.0f, 1.0f, 1.0f));
    addParameter (recLenParam = new juce::AudioParameterFloat (
        "reclen", "Record Length (s)", 0.1f, 10.0f, 2.0f));
}

FFTFreezerProcessor::~FFTFreezerProcessor() {}

//==============================================================================
void FFTFreezerProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // Pre-allocate record buffer for max length (10s)
    int maxRec = (int)(10.0 * sampleRate);
    recBuffer.resize (maxRec, 0.0f);

    // Pre-allocate loop buffers
    loopA.resize (maxRec, 0.0f);
    loopB.resize (maxRec, 0.0f);
}

void FFTFreezerProcessor::releaseResources() {}

//==============================================================================
void FFTFreezerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    auto curState = state.load();

    // --- Recording state: capture mono-mixed input ---
    if (curState == Recording)
    {
        float scale = (numChannels > 0) ? 1.0f / (float)numChannels : 1.0f;
        int pos = recWritePos.load();

        for (int s = 0; s < numSamples && pos < recTargetLength; ++s)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                mono += buffer.getSample (ch, s);
            recBuffer[pos] = mono * scale;
            ++pos;
        }
        recWritePos.store (pos);

        // Done recording? Launch freeze on a detached thread
        if (pos >= recTargetLength)
        {
            state.store (Freezing);
            std::thread ([this]() {
                performFreeze();
                freezeDone.store (true);
            }).detach();
        }
    }

    // --- Check if freeze just finished (lock-free swap) ---
    if (freezeDone.exchange (false))
    {
        loopPlayhead = 0;
        state.store (Playing);
    }

    // --- Play frozen loop ---
    int len = loopLength.load();
    auto* loop = activeLoop.load();
    if (len > 0 && loop != nullptr && (state.load() == Playing))
    {
        float mix = mixParam->get();
        float dry = 1.0f - mix;

        for (int s = 0; s < numSamples; ++s)
        {
            float frozenSample = (*loop)[loopPlayhead];
            loopPlayhead = (loopPlayhead + 1) % len;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float orig = buffer.getSample (ch, s);
                buffer.setSample (ch, s, orig * dry + frozenSample * mix);
            }
        }
    }
}

//==============================================================================
void FFTFreezerProcessor::startRecording()
{
    auto curState = state.load();
    if (curState == Recording || curState == Freezing)
        return; // already busy

    recTargetLength = (int)(recLenParam->get() * currentSampleRate);
    if (recTargetLength < 2048) recTargetLength = 2048;
    if (recTargetLength > (int)recBuffer.size())
        recTargetLength = (int)recBuffer.size();

    recWritePos.store (0);
    state.store (Recording);
}

float FFTFreezerProcessor::getRecordProgress() const
{
    if (state.load() != Recording) return 0.0f;
    if (recTargetLength <= 0) return 0.0f;
    return (float)recWritePos.load() / (float)recTargetLength;
}

//==============================================================================
void FFTFreezerProcessor::performFreeze()
{
    int n = recTargetLength;

    // Find next power of 2
    int fftOrder = 1;
    while ((1 << fftOrder) < n) ++fftOrder;
    if (fftOrder > maxOrder) fftOrder = maxOrder;
    n = 1 << fftOrder;
    int n2 = n / 2;

    // Apply fade window to recorded audio
    int actualLen = recTargetLength;
    std::vector<float> input (n, 0.0f);
    int fade = std::min (1000, actualLen / 2);
    for (int i = 0; i < actualLen; ++i)
    {
        float w = 1.0f;
        if (i < fade)          w = (float)i / (float)fade;
        else if (i >= actualLen - fade) w = (float)(actualLen - 1 - i) / (float)fade;
        input[i] = recBuffer[i] * w;
    }

    // Forward FFT
    juce::dsp::FFT fft (fftOrder);
    std::vector<float> fftData (n * 2, 0.0f);
    for (int i = 0; i < n; ++i)
        fftData[i] = input[i];

    fft.performRealOnlyForwardTransform (fftData.data(), true);

    // Extract magnitudes with threshold
    std::vector<float> magnitudes (n2 + 1, 0.0f);
    float thresh = threshParam->get();
    for (int i = 1; i < n2; ++i)
    {
        float re = fftData[i * 2];
        float im = fftData[i * 2 + 1];
        float mag = std::sqrt (re * re + im * im);
        magnitudes[i] = (mag >= thresh) ? mag : 0.0f;
    }
    magnitudes[0] = 0.0f;
    magnitudes[n2] = 0.0f;

    // Randomize phases
    std::uniform_real_distribution<float> dist (-juce::MathConstants<float>::pi,
                                                 juce::MathConstants<float>::pi);
    for (int i = 0; i < n2 + 1; ++i)
    {
        float phase = dist (rng);
        fftData[i * 2]     = magnitudes[i] * std::cos (phase);
        fftData[i * 2 + 1] = magnitudes[i] * std::sin (phase);
    }
    fftData[0] = 0.0f; fftData[1] = 0.0f;
    fftData[n2 * 2] = 0.0f; fftData[n2 * 2 + 1] = 0.0f;

    // Inverse FFT
    fft.performRealOnlyInverseTransform (fftData.data());

    // Write to the inactive loop buffer, then swap
    float invN = 1.0f / (float)n;
    auto* current = activeLoop.load();
    auto* target = (current == &loopA) ? &loopB : &loopA;

    if ((int)target->size() < n)
        target->resize (n);

    for (int i = 0; i < n; ++i)
        (*target)[i] = fftData[i] * invN;

    // Atomic swap — audio thread picks this up next block
    loopLength.store (n);
    activeLoop.store (target);
}

//==============================================================================
void FFTFreezerProcessor::writeToDisk()
{
    int len = loopLength.load();
    auto* loop = activeLoop.load();
    if (len <= 0 || loop == nullptr || state.load() != Playing) return;

    fileChooser = std::make_shared<juce::FileChooser> (
        "Save Frozen Loop",
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
            .getChildFile ("frozen_loop.wav"),
        "*.wav");

    auto sr = currentSampleRate;
    auto chooserPtr = fileChooser;

    // Copy loop data so we don't hold references during async callback
    std::vector<float> loopCopy (loop->begin(), loop->begin() + len);

    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [len, sr, chooserPtr, loopData = std::move (loopCopy)] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{}) return;

            file.deleteFile();
            std::unique_ptr<juce::FileOutputStream> fos (file.createOutputStream());
            if (!fos) return;

            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> writer (
                wav.createWriterFor (fos.get(), sr, 1, 24, {}, 0));

            if (writer)
            {
                fos.release();
                juce::AudioBuffer<float> buf (1, len);
                buf.copyFrom (0, 0, loopData.data(), len);
                writer->writeFromAudioSampleBuffer (buf, 0, len);
            }
        });
}

//==============================================================================
juce::AudioProcessorEditor* FFTFreezerProcessor::createEditor()
{
    return new FFTFreezerEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FFTFreezerProcessor();
}
