#pragma once

#include <JuceHeader.h>

class WindowProcessor
{
public:
    void prepare(int requestedSize)
    {
        size = juce::jmax(1, requestedSize);
        coefficients.resize(static_cast<size_t>(size));

        juce::dsp::WindowingFunction<float>::fillWindowingTables(
            coefficients.data(),
            static_cast<size_t>(size),
            juce::dsp::WindowingFunction<float>::hann,
            false);
    }

    void apply(float* samples, int numberOfSamples) const noexcept
    {
        jassert(samples != nullptr);
        jassert(numberOfSamples == size);
        juce::FloatVectorOperations::multiply(samples,
                                              coefficients.data(),
                                              numberOfSamples);
    }

    [[nodiscard]] int getSize() const noexcept { return size; }

private:
    std::vector<float> coefficients;
    int size = 0;
};
