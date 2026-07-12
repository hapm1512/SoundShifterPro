# SoundShifter Pro — Milestone 2D

## Completed

- Real-time polyphonic pitch shifting V1
- Pitch range: -12 to +12 semitones
- Fine tuning: -100 to +100 cents
- Tempo remains unchanged
- Stereo FFT phase-vocoder processing
- Latency-compensated dry/wet mix
- Latency-compensated bypass
- MIDI input enabled
- GUI and host automation remain synchronized
- No allocation inside processBlock

## Default MIDI mapping

- CC 30, value >= 64: Tone Down
- CC 31, value >= 64: Tone Up
- CC 32, value >= 64: Tone Reset
- Note C4 / MIDI 60: Tone Down
- Note D4 / MIDI 62: Tone Up
- Note E4 / MIDI 64: Tone Reset

For CC buttons, send value 127 when pressed and value 0 when released.

## Build

```powershell
cd D:\ProjectsVST3\SoundShifterPro
cmake -S . -B Build -A x64
cmake --build Build --config Release
```

## Initial test

- Pitch: +3, -5, +12, -12
- Fine: +50, -25
- Mix: 100%
- Output: 0 dB
- Test stereo beats and transient-heavy material

Milestone 2E will improve phase locking, transient preservation, stereo coherence, and sound quality.


## Epic 3A - Peak Detection

- Adaptive spectral peak detection.
- Local-maxima identification.
- Preallocated peak storage.
- No audio-thread allocations.
- Peak data exposed for Phase Locking.
