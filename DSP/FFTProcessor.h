#pragma once

#include <JuceHeader.h>
#include "DSPConfig.h"
#include "WindowProcessor.h"

class FFTProcessor
{
public:
    void prepare()
    {
        fft = std::make_unique<juce::dsp::FFT>(SoundShifterDSP::Config::fftOrder);
        window.prepare(SoundShifterDSP::Config::fftSize);
        transformData.assign(static_cast<size_t>(SoundShifterDSP::Config::fftSize * 2), 0.0f);
        reset();
    }

    void reset() noexcept
    {
        std::fill(transformData.begin(), transformData.end(), 0.0f);
    }

    void processIdentityFrame(const float* input, float* output) noexcept
    {
        jassert(fft != nullptr);
        jassert(input != nullptr);
        jassert(output != nullptr);

        juce::FloatVectorOperations::clear(transformData.data(),
                                           SoundShifterDSP::Config::fftSize * 2);
        juce::FloatVectorOperations::copy(transformData.data(),
                                          input,
                                          SoundShifterDSP::Config::fftSize);

        window.applyAnalysis(transformData.data(), SoundShifterDSP::Config::fftSize);
        fft->performRealOnlyForwardTransform(transformData.data());

        // Milestone 2C intentionally leaves the spectrum unchanged.
        // Milestone 2D will modify magnitude and phase here.

        fft->performRealOnlyInverseTransform(transformData.data());
        window.applySynthesis(transformData.data(), SoundShifterDSP::Config::fftSize);

        juce::FloatVectorOperations::copy(output,
                                          transformData.data(),
                                          SoundShifterDSP::Config::fftSize);
    }

    [[nodiscard]] const float* getTransformData() const noexcept
    {
        return transformData.data();
    }

private:
    std::unique_ptr<juce::dsp::FFT> fft;
    WindowProcessor window;
    std::vector<float> transformData;
};
