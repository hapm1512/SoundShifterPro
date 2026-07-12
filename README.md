# SoundShifter Pro

Milestone 2A commercial GUI foundation.

## Included

- JUCE 8 VST3 and Standalone targets
- Resizable commercial interface
- Primary Pitch Shift control
- Fine, Mix, and Output controls
- HQ and Bypass parameters
- Stereo input and output meters
- Latency and engine status display
- APVTS state persistence
- DSP remains transparent until Milestone 2B

## Build

```powershell
cmake -S . -B Build -A x64
cmake --build Build --config Release
```
