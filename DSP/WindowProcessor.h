#pragma once

#include <JuceHeader.h>
#include "DSPConfig.h"

class WindowProcessor
{
public:
    void prepare(int requestedSize)
    {
        size = juce::jmax(1, requestedSize);
        analysisCoefficients.resize(static_cast<size_t>(size));
        synthesisCoefficients.resize(static_cast<size_t>(size));

        juce::dsp::WindowingFunction<float>::fillWindowingTables(
            analysisCoefficients.data(),
            static_cast<size_t>(size),
            juce::dsp::WindowingFunction<float>::hann,
            false);

        // Normalise the analysis/synthesis window pair for the configured hop.
        // This keeps identity FFT -> IFFT reconstruction at unity gain.
        for (int sample = 0; sample < size; ++sample)
        {
            float overlapEnergy = 0.0f;

            for (int overlap = 0; overlap < SoundShifterDSP::Config::overlapFactor; ++overlap)
            {
                const auto shifted = (sample + overlap * SoundShifterDSP::Config::hopSize) % size;
                const auto coefficient = analysisCoefficients[static_cast<size_t>(shifted)];
                overlapEnergy += coefficient * coefficient;
            }

            const auto analysis = analysisCoefficients[static_cast<size_t>(sample)];
            synthesisCoefficients[static_cast<size_t>(sample)] =
                overlapEnergy > 1.0e-8f ? analysis / overlapEnergy : 0.0f;
        }
    }

    void applyAnalysis(float* samples, int numberOfSamples) const noexcept
    {
        jassert(samples != nullptr);
        jassert(numberOfSamples == size);
        juce::FloatVectorOperations::multiply(samples,
                                              analysisCoefficients.data(),
                                              numberOfSamples);
    }

    void applySynthesis(float* samples, int numberOfSamples) const noexcept
    {
        jassert(samples != nullptr);
        jassert(numberOfSamples == size);
        juce::FloatVectorOperations::multiply(samples,
                                              synthesisCoefficients.data(),
                                              numberOfSamples);
    }

    [[nodiscard]] int getSize() const noexcept { return size; }

private:
    std::vector<float> analysisCoefficients;
    std::vector<float> synthesisCoefficients;
    int size = 0;
};
