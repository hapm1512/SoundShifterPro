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
    float smoothedEnergy = 0.0f;
    float transientStrength = 0.0f;
};
