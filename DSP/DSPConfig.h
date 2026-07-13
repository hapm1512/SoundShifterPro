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
    };
}
