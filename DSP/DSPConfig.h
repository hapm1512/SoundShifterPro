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
    };
}
