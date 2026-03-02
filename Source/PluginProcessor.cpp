#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
// Worker thread for FFT freeze (avoid blocking audio thread)
//==============================================================================
class FreezeThread : public juce::Thread
{
public:
    FreezeThread (FFTFreezerProcessor& p) : juce::Thread ("FreezeThread"), proc (p) {}
    void run() override { proc.triggerFreeze(); }
private:
    FFTFreezerProcessor& proc;
};

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

FFTFreezerProcessor::~FFTFreezerProcessor()
{
    if (freezeThread) freezeThread->stopThread (2000);
}

//==============================================================================
void FFTFreezerProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // Allocate ring buffer for max record length (10s)
    ringSize = (int)(recLenParam->get() * sampleRate);
    if (ringSize < 2048) ringSize = 2048;
    ringBuffer.resize (ringSize, 0.0f);
    ringIndex = 0;

    // Build fade window (1000 sample linear fade in/out like the original)
    window.resize (ringSize);
    int fade = std::min (1000, ringSize / 2);
    for (int i = 0; i < fade; ++i)
        window[i] = window[ringSize - 1 - i] = (float)i / (float)fade;
    for (int i = fade; i < ringSize - fade; ++i)
        window[i] = 1.0f;
}

void FFTFreezerProcessor::releaseResources() {}

//==============================================================================
void FFTFreezerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    // --- Record mono mix into ring buffer ---
    // Recompute ringSize if recLen changed
    int desiredRingSize = (int)(recLenParam->get() * currentSampleRate);
    if (desiredRingSize < 2048) desiredRingSize = 2048;
    if (desiredRingSize != ringSize)
    {
        ringSize = desiredRingSize;
        ringBuffer.resize (ringSize, 0.0f);
        if (ringIndex >= ringSize) ringIndex = 0;
        // Rebuild window
        window.resize (ringSize);
        int fade = std::min (1000, ringSize / 2);
        for (int i = 0; i < fade; ++i)
            window[i] = window[ringSize - 1 - i] = (float)i / (float)fade;
        for (int i = fade; i < ringSize - fade; ++i)
            window[i] = 1.0f;
    }

    float scale = (numChannels > 0) ? 1.0f / (float)numChannels : 1.0f;
    for (int s = 0; s < numSamples; ++s)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            mono += buffer.getSample (ch, s);
        mono *= scale;

        ringBuffer[ringIndex] = mono;
        ringIndex = (ringIndex + 1) % ringSize;
    }

    // --- Check if freeze was requested (from GUI button) ---
    if (freezeRequested.exchange (false))
    {
        if (!busy.load())
        {
            busy.store (true);
            // Launch freeze on a background thread
            if (freezeThread) freezeThread->stopThread (2000);
            freezeThread = std::make_unique<FreezeThread> (*this);
            freezeThread->startThread();
        }
    }

    // --- Play frozen loop ---
    int len = loopLength.load();
    if (len > 0 && frozen.load())
    {
        float mix = mixParam->get();
        float dry = 1.0f - mix;

        const juce::ScopedLock sl (freezeLock);
        for (int s = 0; s < numSamples; ++s)
        {
            float frozenSample = loopBuffer[loopPlayhead];
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
void FFTFreezerProcessor::triggerFreeze()
{
    performFreeze();
    busy.store (false);
}

void FFTFreezerProcessor::performFreeze()
{
    // Determine FFT size = next power of 2 >= ringSize
    int n = ringSize;
    fftOrder = 1;
    while ((1 << fftOrder) < n) ++fftOrder;
    if (fftOrder > maxOrder) fftOrder = maxOrder;
    n = 1 << fftOrder;
    int n2 = n / 2;

    // Copy ring buffer content (unwrap from ringIndex) and apply window
    std::vector<float> input (n, 0.0f);
    int actualLen = std::min (ringSize, n);
    int idx = ringIndex;
    for (int i = 0; i < actualLen; ++i)
    {
        int ri = (idx + i) % ringSize;
        float w = (i < (int)window.size()) ? window[i] : 1.0f;
        input[i] = ringBuffer[ri] * w;
    }

    // Forward FFT (JUCE FFT is in-place, interleaved complex)
    juce::dsp::FFT fft (fftOrder);
    // JUCE FFT expects 2*n floats (n complex pairs) for real FFT
    std::vector<float> fftData (n * 2, 0.0f);
    for (int i = 0; i < n; ++i)
        fftData[i] = input[i];

    fft.performRealOnlyForwardTransform (fftData.data(), true);

    // Extract magnitudes (interleaved: [re0, im0, re1, im1, ...])
    std::vector<float> magnitudes (n2 + 1, 0.0f);
    float thresh = threshParam->get();
    for (int i = 1; i < n2; ++i)
    {
        float re = fftData[i * 2];
        float im = fftData[i * 2 + 1];
        float mag = std::sqrt (re * re + im * im);
        magnitudes[i] = (mag >= thresh) ? mag : 0.0f;
    }
    // Zero DC and Nyquist
    magnitudes[0] = 0.0f;
    magnitudes[n2] = 0.0f;

    // Randomize phases and reconstruct spectrum
    std::uniform_real_distribution<float> dist (-juce::MathConstants<float>::pi,
                                                 juce::MathConstants<float>::pi);
    for (int i = 0; i < n2 + 1; ++i)
    {
        float phase = dist (rng);
        fftData[i * 2]     = magnitudes[i] * std::cos (phase);
        fftData[i * 2 + 1] = magnitudes[i] * std::sin (phase);
    }
    // DC and Nyquist must be real (zero imaginary)
    fftData[0] = 0.0f; fftData[1] = 0.0f;
    fftData[n2 * 2] = 0.0f; fftData[n2 * 2 + 1] = 0.0f;

    // Inverse FFT
    fft.performRealOnlyInverseTransform (fftData.data());

    // Scale by 1/n (JUCE IFFT doesn't normalize)
    float invN = 1.0f / (float)n;

    // Write to loop buffer
    {
        const juce::ScopedLock sl (freezeLock);
        loopBuffer.resize (n);
        for (int i = 0; i < n; ++i)
            loopBuffer[i] = fftData[i] * invN;
        loopPlayhead = 0;
        loopLength.store (n);
        frozen.store (true);
    }
}

//==============================================================================
void FFTFreezerProcessor::writeToDisk()
{
    int len = loopLength.load();
    if (len <= 0 || !frozen.load()) return;

    fileChooser = std::make_shared<juce::FileChooser> (
        "Save Frozen Loop",
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
            .getChildFile ("frozen_loop.wav"),
        "*.wav");

    auto sr = currentSampleRate;
    auto chooserPtr = fileChooser;

    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, len, sr, chooserPtr] (const juce::FileChooser& fc)
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
                const juce::ScopedLock sl (freezeLock);
                juce::AudioBuffer<float> buf (1, len);
                buf.copyFrom (0, 0, loopBuffer.data(), len);
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
