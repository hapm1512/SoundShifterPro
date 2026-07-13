#pragma once

#include <JuceHeader.h>

namespace SoundShifterDSP
{
    struct Config
    {
        static constexpr int fftOrder = 11;
        static constexpr int fftSize = 1 << fftOrder;
        static constexpr int hopSize = fftSize / 4;
        static constexpr int overlapFactor = fftSize / hopSize;

        static constexpr int maxChannels = 2;
        static constexpr int maxPeaks = 256;

        static constexpr float silenceFloorDb = -120.0f;
        static constexpr float magnitudeFloor = 1.0e-12f;
        static constexpr float energyFloor = 1.0e-18f;

        // Supported pitch range: -12 to +12 semitones.
        static constexpr float minimumPitchRatio = 0.5f;
        static constexpr float maximumPitchRatio = 2.0f;

        // FAST quality profile.
        static constexpr float fastEnergyGainMinimum = 0.94f;
        static constexpr float fastEnergyGainMaximum = 1.06f;

        // HQ quality profile.
        static constexpr float hqEnergyGainMinimum = 0.90f;
        static constexpr float hqEnergyGainMaximum = 1.12f;
        static constexpr float hqHarmonicEnhancementMaximum = 1.10f;
        static constexpr float hqLeakageGainMinimum = 0.94f;
        static constexpr float hqLeakageGainMaximum = 1.06f;

        // Epic 3D: transient analysis foundation.
        static constexpr bool enableTransient = true;
        static constexpr float transientThreshold = 0.18f;
        static constexpr float transientSensitivity = 1.75f;
        static constexpr float transientRelease = 0.82f;
        static constexpr float transientEnergySmoothing = 0.12f;
        static constexpr float transientEnergyFloor = 1.0e-9f;
        static constexpr float transientBlend = 0.35f;
        static constexpr float transientPhaseResetHQ = 0.72f;
        static constexpr float transientPhaseResetFast = 0.48f;

        // Sprint DSP 01C: adaptive transient profile.
        static constexpr float transientThresholdMinimum = 0.10f;
        static constexpr float transientThresholdMaximum = 0.42f;
        static constexpr float transientNoiseSmoothing = 0.08f;
        static constexpr float transientFluxSmoothing = 0.18f;
        static constexpr float transientAttack = 0.58f;
        static constexpr float transientDynamicReleaseMinimum = 0.68f;
        static constexpr float transientDynamicReleaseMaximum = 0.90f;

        // Sprint DSP 01C: stereo energy profile.
        static constexpr float stereoGainMinimum = 0.76f;
        static constexpr float stereoGainMaximum = 1.28f;
        static constexpr float stereoSideMinimum = 0.90f;
        static constexpr float stereoSideMaximum = 1.22f;
        static constexpr float stereoEnergySmoothingHQ = 0.12f;
        static constexpr float stereoEnergySmoothingFast = 0.22f;

        static_assert(fftSize > 0);
        static_assert(hopSize > 0);
        static_assert(fftSize % hopSize == 0);
        static_assert(overlapFactor >= 2);
    };
}
