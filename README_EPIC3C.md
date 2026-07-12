# SoundShifter Pro — Epic 3C

Version 0.3.2

## Added
- Identity Phase Lock module.
- Peak identity tracking between FFT frames.
- Relative phase propagation around spectral peaks.
- Phase unwrapping against the nearest tracked peak.
- Fixed preallocated buffers; no allocation during frame processing.

## MIDI
Existing mappings are unchanged:
- CC30: Tone Down
- CC31: Tone Up
- CC32: Tone Reset

## Build
```powershell
cd D:\ProjectsVST3\SoundShifterPro
cmake -S . -B Build -A x64
cmake --build Build --config Release
```
