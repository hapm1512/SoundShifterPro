#include "TransientDetector.h"

void TransientDetector::reset() noexcept
{
    smoothedEnergy = 0.0f;
    transientStrength = 0.0f;
}

float TransientDetector::process(const float* samples,
                                 int numSamples) noexcept
{
    if (samples == nullptr || numSamples <= 0)
    {
        transientStrength = 0.0f;
        return transientStrength;
    }

    float frameEnergy = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
        frameEnergy += samples[sample] * samples[sample];

    frameEnergy /= static_cast<float>(numSamples);

    const auto positiveFlux = juce::jmax(0.0f, frameEnergy - smoothedEnergy);
    const auto reference = juce::jmax(smoothedEnergy,
                                      SoundShifterDSP::Config::transientEnergyFloor);
    const auto normalisedFlux = positiveFlux / reference;
    const auto detectedStrength = juce::jlimit(
        0.0f,
        1.0f,
        (normalisedFlux - SoundShifterDSP::Config::transientThreshold)
            * SoundShifterDSP::Config::transientSensitivity);

    transientStrength = juce::jmax(
        detectedStrength,
        transientStrength * SoundShifterDSP::Config::transientRelease);

    smoothedEnergy += (frameEnergy - smoothedEnergy)
                    * SoundShifterDSP::Config::transientEnergySmoothing;

    return transientStrength;
}
