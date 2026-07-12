# SoundShifter Pro

Milestone 2C activates the complete identity STFT signal path.

## Included

- JUCE 8 VST3 and Standalone targets
- Commercial resizable GUI
- APVTS parameters and state recall
- Stereo input/output meters
- Allocation-free audio callback path
- 2048-point real FFT and IFFT
- 512-sample hop size
- Hann analysis window
- Normalised synthesis window
- Stereo overlap-add reconstruction
- Fixed reported plugin latency

## Current DSP status

Audio now passes through the complete FFT -> spectrum -> IFFT -> overlap-add path. The spectrum remains unchanged, so the expected result is transparent delayed audio. Pitch and Fine are connected but spectral shifting starts in Milestone 2D.

## Test checklist

- Pitch = 0 semitone
- Fine = 0 cent
- Mix = 100%
- Output = 0 dB
- HQ = ON
- No clicks or pops
- Stereo image remains centred
- Output level closely matches input

## Build

```powershell
cd D:\ProjectsVST3\SoundShifterPro
cmake -S . -B Build -A x64
cmake --build Build --config Release
```
