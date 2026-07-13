# SoundShifter Pro — Epic 3D.3

## Added
- Adaptive transient phase reset
- Stereo-linked transient strength
- HQ and fast transient response depths

## Changed
- `FFTProcessor::processPitchFrame` accepts transient strength and quality mode
- Attack frames blend synthesis phase toward mapped analysis phase
- Project version updated to `0.3.4`

## Behaviour
- Pitch ratio remains unchanged during attacks
- Transient focus improves without dry-signal pitch mismatch
- No allocation is added to the audio callback
