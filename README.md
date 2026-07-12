# SoundShifter Pro

Stereo pitch-shifting VST3 and standalone application for musical beats.

## Milestone 1

- JUCE 8 + CMake project
- VST3 and Standalone targets
- APVTS parameter system
- Pitch, Fine, Mix, Output and Bypass controls
- Input and output meters
- State save and restore
- Transparent DSP pass-through

## Local paths

- Project: `D:\ProjectsVST3\SoundShifterPro`
- JUCE: `D:\JUCE8`

## Configure and build

```powershell
cd D:\ProjectsVST3\SoundShifterPro
cmake -S . -B Build -G "Visual Studio 18 2026" -A x64
cmake --build Build --config Release
```

If your Visual Studio generator has another name, run `cmake --help` and use the installed generator name.
