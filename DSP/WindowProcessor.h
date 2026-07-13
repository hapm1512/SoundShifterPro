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
        updateSelectedCoefficients();
    }

    void setHighQuality(bool shouldUseHighQuality) noexcept
    {
        if (highQuality != shouldUseHighQuality)
        {
            highQuality = shouldUseHighQuality;
            updateSelectedCoefficients();
        }
    }

    void applyAnalysis(float* samples, int numberOfSamples) const noexcept
    {
        jassert(samples != nullptr);
        jassert(numberOfSamples == size);

        const auto* coeffs = selectedAnalysis;

        juce::FloatVectorOperations::multiply(
            samples,
            coeffs,
            numberOfSamples);
    }

    void applySynthesis(float* samples, int numberOfSamples) const noexcept
    {
        jassert(samples != nullptr);
        jassert(numberOfSamples == size);

        const auto* coeffs = selectedSynthesis;

        juce::FloatVectorOperations::multiply(
            samples,
            coeffs,
            numberOfSamples);
    }

    [[nodiscard]] int getSize() const noexcept
    {
        return size;
    }

private:
    void updateSelectedCoefficients() noexcept
    {
        selectedAnalysis = highQuality
            ? hqAnalysis.data()
            : fastAnalysis.data();

        selectedSynthesis = highQuality
            ? hqSynthesis.data()
            : fastSynthesis.data();
    }

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
            const auto inverseEnergy =
                overlapEnergy > SoundShifterDSP::Config::magnitudeFloor
                    ? 1.0f / overlapEnergy
                    : 0.0f;

            synthesis[static_cast<size_t>(sample)] =
                coefficient * inverseEnergy;
        }
    }

    std::vector<float> fastAnalysis;
    std::vector<float> fastSynthesis;
    std::vector<float> hqAnalysis;
    std::vector<float> hqSynthesis;

    const float* selectedAnalysis = nullptr;
    const float* selectedSynthesis = nullptr;
    int size = 0;
    bool highQuality = true;
};
