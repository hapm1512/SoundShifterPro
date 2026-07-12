#pragma once

#include <JuceHeader.h>
#include "DSPConfig.h"
#include "WindowProcessor.h"

class FFTProcessor
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = juce::jmax(1.0, newSampleRate);
        fft = std::make_unique<juce::dsp::FFT>(SoundShifterDSP::Config::fftOrder);
        window.prepare(SoundShifterDSP::Config::fftSize);

        transformData.assign(static_cast<size_t>(SoundShifterDSP::Config::fftSize * 2), 0.0f);
        lastPhase.assign(static_cast<size_t>(numBins), 0.0f);
        sumPhase.assign(static_cast<size_t>(numBins), 0.0f);
        analysisMagnitude.assign(static_cast<size_t>(numBins), 0.0f);
        analysisFrequency.assign(static_cast<size_t>(numBins), 0.0f);
        synthesisMagnitude.assign(static_cast<size_t>(numBins), 0.0f);
        synthesisFrequency.assign(static_cast<size_t>(numBins), 0.0f);
        reset();
    }

    void reset() noexcept
    {
        std::fill(transformData.begin(), transformData.end(), 0.0f);
        std::fill(lastPhase.begin(), lastPhase.end(), 0.0f);
        std::fill(sumPhase.begin(), sumPhase.end(), 0.0f);
        std::fill(analysisMagnitude.begin(), analysisMagnitude.end(), 0.0f);
        std::fill(analysisFrequency.begin(), analysisFrequency.end(), 0.0f);
        std::fill(synthesisMagnitude.begin(), synthesisMagnitude.end(), 0.0f);
        std::fill(synthesisFrequency.begin(), synthesisFrequency.end(), 0.0f);
    }

    void processPitchFrame(const float* input, float* output, float pitchRatio) noexcept
    {
        jassert(fft != nullptr && input != nullptr && output != nullptr);

        constexpr auto fftSize = SoundShifterDSP::Config::fftSize;
        constexpr auto hopSize = SoundShifterDSP::Config::hopSize;
        const auto safeRatio = juce::jlimit(0.5f, 2.0f, pitchRatio);
        const auto frequencyPerBin = static_cast<float>(sampleRate / fftSize);
        const auto expectedPhase = juce::MathConstants<float>::twoPi
                                 * static_cast<float>(hopSize)
                                 / static_cast<float>(fftSize);

        juce::FloatVectorOperations::clear(transformData.data(), fftSize * 2);
        juce::FloatVectorOperations::copy(transformData.data(), input, fftSize);
        window.applyAnalysis(transformData.data(), fftSize);
        fft->performRealOnlyForwardTransform(transformData.data());

        std::fill(synthesisMagnitude.begin(), synthesisMagnitude.end(), 0.0f);
        std::fill(synthesisFrequency.begin(), synthesisFrequency.end(), 0.0f);

        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto real = getReal(bin);
            const auto imag = getImag(bin);
            const auto magnitude = std::sqrt(real * real + imag * imag);
            const auto phase = std::atan2(imag, real);

            auto phaseDelta = phase - lastPhase[static_cast<size_t>(bin)];
            lastPhase[static_cast<size_t>(bin)] = phase;
            phaseDelta -= static_cast<float>(bin) * expectedPhase;
            phaseDelta = std::remainder(phaseDelta, juce::MathConstants<float>::twoPi);

            const auto binDeviation = phaseDelta / expectedPhase;
            analysisMagnitude[static_cast<size_t>(bin)] = magnitude;
            analysisFrequency[static_cast<size_t>(bin)] =
                (static_cast<float>(bin) + binDeviation) * frequencyPerBin;
        }

        for (int sourceBin = 0; sourceBin < numBins; ++sourceBin)
        {
            const auto targetPosition = static_cast<float>(sourceBin) * safeRatio;
            const auto targetBin = static_cast<int>(targetPosition);
            const auto fraction = targetPosition - static_cast<float>(targetBin);
            const auto shiftedFrequency = analysisFrequency[static_cast<size_t>(sourceBin)] * safeRatio;
            const auto magnitude = analysisMagnitude[static_cast<size_t>(sourceBin)];

            addToSynthesisBin(targetBin, magnitude * (1.0f - fraction), shiftedFrequency);
            addToSynthesisBin(targetBin + 1, magnitude * fraction, shiftedFrequency);
        }

        juce::FloatVectorOperations::clear(transformData.data(), fftSize * 2);

        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto magnitude = synthesisMagnitude[static_cast<size_t>(bin)];
            auto frequency = synthesisFrequency[static_cast<size_t>(bin)];

            if (magnitude > 1.0e-12f)
                frequency /= magnitude;
            else
                frequency = static_cast<float>(bin) * frequencyPerBin;

            const auto binDeviation = frequency / frequencyPerBin - static_cast<float>(bin);
            const auto phaseIncrement = (static_cast<float>(bin) + binDeviation) * expectedPhase;
            sumPhase[static_cast<size_t>(bin)] += phaseIncrement;

            setComplex(bin,
                       magnitude * std::cos(sumPhase[static_cast<size_t>(bin)]),
                       magnitude * std::sin(sumPhase[static_cast<size_t>(bin)]));
        }

        fft->performRealOnlyInverseTransform(transformData.data());
        window.applySynthesis(transformData.data(), fftSize);
        juce::FloatVectorOperations::copy(output, transformData.data(), fftSize);
    }

private:
    static constexpr int numBins = SoundShifterDSP::Config::fftSize / 2 + 1;

    float getReal(int bin) const noexcept
    {
        if (bin == 0) return transformData[0];
        if (bin == SoundShifterDSP::Config::fftSize / 2) return transformData[1];
        return transformData[static_cast<size_t>(bin * 2)];
    }

    float getImag(int bin) const noexcept
    {
        if (bin == 0 || bin == SoundShifterDSP::Config::fftSize / 2) return 0.0f;
        return transformData[static_cast<size_t>(bin * 2 + 1)];
    }

    void setComplex(int bin, float real, float imag) noexcept
    {
        if (bin == 0)
        {
            transformData[0] = real;
            return;
        }

        if (bin == SoundShifterDSP::Config::fftSize / 2)
        {
            transformData[1] = real;
            return;
        }

        transformData[static_cast<size_t>(bin * 2)] = real;
        transformData[static_cast<size_t>(bin * 2 + 1)] = imag;
    }

    void addToSynthesisBin(int bin, float weightedMagnitude, float shiftedFrequency) noexcept
    {
        if (!juce::isPositiveAndBelow(bin, numBins) || weightedMagnitude <= 0.0f)
            return;

        synthesisMagnitude[static_cast<size_t>(bin)] += weightedMagnitude;
        synthesisFrequency[static_cast<size_t>(bin)] += weightedMagnitude * shiftedFrequency;
    }

    std::unique_ptr<juce::dsp::FFT> fft;
    WindowProcessor window;
    std::vector<float> transformData;
    std::vector<float> lastPhase;
    std::vector<float> sumPhase;
    std::vector<float> analysisMagnitude;
    std::vector<float> analysisFrequency;
    std::vector<float> synthesisMagnitude;
    std::vector<float> synthesisFrequency;
    double sampleRate = 44100.0;
};
