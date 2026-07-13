# SoundShifter Pro — Epic 3D.2

## Added
- `TransientDetector.h/.cpp`
- Energy-flux transient analysis per channel
- Adaptive threshold and release smoothing
- Preallocated, audio-thread-safe state

## Changed
- `PitchShiftEngine` now analyses every FFT frame
- `DSPConfig` contains transient tuning constants
- Project version updated to `0.3.3`

## Behaviour
- Pitch processing is unchanged in this patch.
- `transientStrength` is prepared for Epic 3D.3.
