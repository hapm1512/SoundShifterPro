#pragma once

#include <JuceHeader.h>
#include "DSPConfig.h"

class TransientDetector
{
public:
    void reset() noexcept;

    [[nodiscard]] float process(const float* samples,
                                int numSamples) noexcept;

    [[nodiscard]] float getStrength() const noexcept
    {
        return transientStrength;
    }

private:
    [[nodiscard]] float calculateFrameEnergy(const float* samples,
                                             int numSamples) const noexcept;

    [[nodiscard]] float calculateAttackFlux(const float* samples,
                                            int numSamples) noexcept;

    float smoothedEnergy = 0.0f;
    float smoothedNoiseFloor = 0.0f;
    float smoothedFlux = 0.0f;
    float previousSample = 0.0f;
    float transientStrength = 0.0f;
};
