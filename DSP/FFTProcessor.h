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
        timeDomain.assign(static_cast<size_t>(SoundShifterDSP::Config::fftSize), 0.0f);
        frequencyDomain.assign(static_cast<size_t>(SoundShifterDSP::Config::fftSize * 2), 0.0f);
        reset();
    }

    void reset() noexcept
    {
        std::fill(timeDomain.begin(), timeDomain.end(), 0.0f);
        std::fill(frequencyDomain.begin(), frequencyDomain.end(), 0.0f);
    }

    void analyseFrame(const float* input) noexcept
    {
        jassert(fft != nullptr);
        jassert(input != nullptr);

        juce::FloatVectorOperations::copy(timeDomain.data(),
                                          input,
                                          SoundShifterDSP::Config::fftSize);
        window.apply(timeDomain.data(), SoundShifterDSP::Config::fftSize);

        juce::FloatVectorOperations::clear(frequencyDomain.data(),
                                           SoundShifterDSP::Config::fftSize * 2);
        juce::FloatVectorOperations::copy(frequencyDomain.data(),
                                          timeDomain.data(),
                                          SoundShifterDSP::Config::fftSize);

        fft->performRealOnlyForwardTransform(frequencyDomain.data(), true);
    }

    [[nodiscard]] const float* getFrequencyData() const noexcept
    {
        return frequencyDomain.data();
    }

private:
    std::unique_ptr<juce::dsp::FFT> fft;
    WindowProcessor window;
    std::vector<float> timeDomain;
    std::vector<float> frequencyDomain;
};
