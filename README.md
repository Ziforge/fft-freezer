# FFT Freezer

JUCE audio plugin that captures live audio and creates perfect seamless loops via spectral freezing (FFT magnitude preservation with phase randomization).

Ported from [vb.freezer~](https://github.com/v7b1/vb-objects/tree/main/source/projects/vb.freezer_tilde) by Volker Boehm (Max/MSP external) to a standalone JUCE VST3/Standalone plugin.

## How it works

1. Audio is continuously recorded into a ring buffer
2. Press **SAMPLE** — the buffer is windowed, FFT'd, magnitudes are extracted, phases are randomized, then IFFT'd back to time domain
3. The result is a **perfect seamless loop** — because the magnitude spectrum is preserved but phase information is destroyed, the waveform tiles seamlessly at the FFT boundary
4. Press **WRITE TO DISK** — saves the frozen loop as a 24-bit WAV file

## Controls

| Control | Description |
|---------|-------------|
| **SAMPLE** | Freeze the current audio input |
| **WRITE TO DISK** | Save the frozen loop as WAV |
| **Threshold** | Spectral gate — bins below this magnitude are zeroed |
| **Mix** | Dry/wet blend (0 = passthrough, 1 = fully frozen) |
| **Rec Length** | Recording buffer duration (0.1s – 10s) |

## Building

Requires CMake 3.22+ and a C++17 compiler. JUCE is fetched automatically.

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j$(nproc)
```

Outputs:
- **Standalone**: `build/FFTFreezer_artefacts/Release/Standalone/FFT Freezer`
- **VST3**: `build/FFTFreezer_artefacts/Release/VST3/FFT Freezer.vst3/`

The VST3 is also auto-installed to `~/.vst3/` on Linux.

## Credits

- Original algorithm by [Volker Boehm](https://github.com/v7b1) (`vb.freezer~`, 2011–2020)
- JUCE port uses `juce::dsp::FFT` — no external FFTW dependency needed

## License

MIT
