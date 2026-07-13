#pragma once

#include <JuceHeader.h>
#include "DSPConfig.h"

class WindowProcessor
{
public:
    void prepare(int requestedSize)
    {
        size = juce::jmax(1, requestedSize);

        fastAnalysis.resize(static_cast<size_t>(size));
        fastSynthesis.resize(static_cast<size_t>(size));
        hqAnalysis.resize(static_cast<size_t>(size));
        hqSynthesis.resize(static_cast<size_t>(size));

        juce::dsp::WindowingFunction<float>::fillWindowingTables(
            fastAnalysis.data(),
            static_cast<size_t>(size),
            juce::dsp::WindowingFunction<float>::hann,
            false);

        juce::dsp::WindowingFunction<float>::fillWindowingTables(
            hqAnalysis.data(),
            static_cast<size_t>(size),
            juce::dsp::WindowingFunction<float>::blackmanHarris,
            false);

        normaliseSynthesisWindow(fastAnalysis, fastSynthesis);
        normaliseSynthesisWindow(hqAnalysis, hqSynthesis);
    }

    void setHighQuality(bool shouldUseHighQuality) noexcept
    {
        highQuality = shouldUseHighQuality;
    }

    void applyAnalysis(float* samples, int numberOfSamples) const noexcept
    {
        jassert(samples != nullptr);
        jassert(numberOfSamples == size);

        const auto& coefficients = highQuality ? hqAnalysis : fastAnalysis;

        juce::FloatVectorOperations::multiply(
            samples,
            coefficients.data(),
            numberOfSamples);
    }

    void applySynthesis(float* samples, int numberOfSamples) const noexcept
    {
        jassert(samples != nullptr);
        jassert(numberOfSamples == size);

        const auto& coefficients = highQuality ? hqSynthesis : fastSynthesis;

        juce::FloatVectorOperations::multiply(
            samples,
            coefficients.data(),
            numberOfSamples);
    }

    [[nodiscard]] int getSize() const noexcept
    {
        return size;
    }

private:
    void normaliseSynthesisWindow(const std::vector<float>& analysis,
                                  std::vector<float>& synthesis)
    {
        for (int sample = 0; sample < size; ++sample)
        {
            float overlapEnergy = 0.0f;

            for (int overlap = 0;
                 overlap < SoundShifterDSP::Config::overlapFactor;
                 ++overlap)
            {
                const auto shifted =
                    (sample + overlap * SoundShifterDSP::Config::hopSize) % size;

                const auto coefficient =
                    analysis[static_cast<size_t>(shifted)];

                overlapEnergy += coefficient * coefficient;
            }

            const auto coefficient = analysis[static_cast<size_t>(sample)];

            synthesis[static_cast<size_t>(sample)] =
                overlapEnergy > 1.0e-8f
                    ? coefficient / overlapEnergy
                    : 0.0f;
        }
    }

    std::vector<float> fastAnalysis;
    std::vector<float> fastSynthesis;
    std::vector<float> hqAnalysis;
    std::vector<float> hqSynthesis;

    int size = 0;
    bool highQuality = true;
};
