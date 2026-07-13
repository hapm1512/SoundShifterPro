#include "TransientDetector.h"

void TransientDetector::reset() noexcept
{
    smoothedEnergy = 0.0f;
    smoothedNoiseFloor = 0.0f;
    smoothedFlux = 0.0f;
    previousSample = 0.0f;
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

    const auto frameEnergy =
        calculateFrameEnergy(samples, numSamples);

    const auto attackFlux =
        calculateAttackFlux(samples, numSamples);

    const auto referenceEnergy = juce::jmax(
        smoothedEnergy,
        SoundShifterDSP::Config::transientEnergyFloor);

    const auto energyRise = juce::jmax(
        0.0f,
        (frameEnergy - smoothedEnergy) / referenceEnergy);

    const auto noiseReference = juce::jmax(
        smoothedNoiseFloor,
        SoundShifterDSP::Config::transientEnergyFloor);

    const auto signalToNoise = juce::jlimit(
        0.0f,
        8.0f,
        frameEnergy / noiseReference);

    const auto noiseRejection = juce::jlimit(
        0.0f,
        1.0f,
        (signalToNoise - 1.0f) / 3.0f);

    const auto adaptiveThreshold = juce::jlimit(
        SoundShifterDSP::Config::transientThresholdMinimum,
        SoundShifterDSP::Config::transientThresholdMaximum,
        SoundShifterDSP::Config::transientThreshold
            + 0.18f * smoothedFlux
            + 0.10f * (1.0f - noiseRejection));

    const auto combinedFlux =
        0.62f * energyRise + 0.38f * attackFlux;

    const auto detectedStrength = juce::jlimit(
        0.0f,
        1.0f,
        (combinedFlux - adaptiveThreshold)
            * SoundShifterDSP::Config::transientSensitivity
            * noiseRejection);

    const auto attackCoefficient =
        SoundShifterDSP::Config::transientAttack;

    const auto releaseCoefficient = juce::jmap(
        detectedStrength,
        SoundShifterDSP::Config::transientDynamicReleaseMinimum,
        SoundShifterDSP::Config::transientDynamicReleaseMaximum);

    if (detectedStrength > transientStrength)
    {
        transientStrength += attackCoefficient
                           * (detectedStrength - transientStrength);
    }
    else
    {
        transientStrength *= releaseCoefficient;
    }

    smoothedEnergy +=
        SoundShifterDSP::Config::transientEnergySmoothing
        * (frameEnergy - smoothedEnergy);

    const auto noiseTarget =
        detectedStrength < 0.08f ? frameEnergy : smoothedNoiseFloor;

    smoothedNoiseFloor +=
        SoundShifterDSP::Config::transientNoiseSmoothing
        * (noiseTarget - smoothedNoiseFloor);

    smoothedFlux +=
        SoundShifterDSP::Config::transientFluxSmoothing
        * (combinedFlux - smoothedFlux);

    transientStrength = juce::jlimit(
        0.0f,
        1.0f,
        transientStrength);

    return transientStrength;
}

float TransientDetector::calculateFrameEnergy(
    const float* samples,
    int numSamples) const noexcept
{
    double energy = 0.0;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto value =
            static_cast<double>(samples[sample]);

        energy += value * value;
    }

    return static_cast<float>(
        energy / static_cast<double>(numSamples));
}

float TransientDetector::calculateAttackFlux(
    const float* samples,
    int numSamples) noexcept
{
    double positiveDerivative = 0.0;
    double absoluteDerivative = 0.0;
    auto previous = previousSample;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto current = samples[sample];
        const auto derivative = current - previous;

        positiveDerivative +=
            static_cast<double>(juce::jmax(0.0f, derivative));

        absoluteDerivative +=
            static_cast<double>(std::abs(derivative));

        previous = current;
    }

    previousSample = previous;

    if (absoluteDerivative <= 1.0e-12)
        return 0.0f;

    return juce::jlimit(
        0.0f,
        1.0f,
        static_cast<float>(
            positiveDerivative / absoluteDerivative));
}
