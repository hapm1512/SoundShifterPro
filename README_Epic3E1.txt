SoundShifter Pro - Epic 3E.1

Đã thực hiện:
- Adaptive Window:
  FAST = Hann
  HQ = Blackman-Harris
- Peak Interpolation:
  Nội suy parabol quanh spectral peak
- Harmonic Enhancement:
  Bù presence nhẹ khi pitch shift lớn
- HQ Mode:
  Điều khiển window, interpolation, enhancement
- Transient Detector:
  Đã nối vào FFTProcessor thay cho 0.0f

Thay 4 file:
DSP/WindowProcessor.h
DSP/FFTProcessor.h
DSP/PitchShiftEngine.h
DSP/PitchShiftEngine.cpp

Chưa thực hiện:
- Multi-Resolution FFT thật sự
- Auto Quality
- Benchmark
