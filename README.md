# SoundShifter Pro

Milestone 2B establishes the real-time DSP foundation while preserving transparent audio passthrough.

## Included

- JUCE 8 VST3 and Standalone targets
- Commercial resizable GUI
- APVTS parameters and state recall
- Stereo input/output meters
- Allocation-free audio callback path
- Stereo ring-buffer capture
- 2048-point Hann-window FFT analysis
- 512-sample hop size
- Overlap-add infrastructure
- Fixed preallocated DSP scratch buffers

## Current DSP status

Audio remains unchanged. FFT analysis runs internally and prepares the project for spectral pitch shifting in Milestone 2C/2D.

## Build

```powershell
cd D:\ProjectsVST3\SoundShifterPro
cmake -S . -B Build -A x64
cmake --build Build --config Release
```
