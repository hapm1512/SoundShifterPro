#pragma once

#include <JuceHeader.h>
#include "DSPConfig.h"
#include "WindowProcessor.h"
#include "PeakDetector.h"
#include "IdentityPhaseLock.h"

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
        analysisPhase.assign(static_cast<size_t>(numBins), 0.0f);
        synthesisMagnitude.assign(static_cast<size_t>(numBins), 0.0f);
        synthesisFrequency.assign(static_cast<size_t>(numBins), 0.0f);
        synthesisPhaseReal.assign(static_cast<size_t>(numBins), 0.0f);
        synthesisPhaseImag.assign(static_cast<size_t>(numBins), 0.0f);
        mappedAnalysisPhase.assign(static_cast<size_t>(numBins), 0.0f);
        outputPhase.assign(static_cast<size_t>(numBins), 0.0f);
        peakDetector.prepare(numBins);
        identityPhaseLock.prepare(numBins, SoundShifterDSP::Config::maxPeaks);
        reset();
    }

    void reset() noexcept
    {
        std::fill(transformData.begin(), transformData.end(), 0.0f);
        std::fill(lastPhase.begin(), lastPhase.end(), 0.0f);
        std::fill(sumPhase.begin(), sumPhase.end(), 0.0f);
        std::fill(analysisMagnitude.begin(), analysisMagnitude.end(), 0.0f);
        std::fill(analysisFrequency.begin(), analysisFrequency.end(), 0.0f);
        std::fill(analysisPhase.begin(), analysisPhase.end(), 0.0f);
        std::fill(synthesisMagnitude.begin(), synthesisMagnitude.end(), 0.0f);
        std::fill(synthesisFrequency.begin(), synthesisFrequency.end(), 0.0f);
        std::fill(synthesisPhaseReal.begin(), synthesisPhaseReal.end(), 0.0f);
        std::fill(synthesisPhaseImag.begin(), synthesisPhaseImag.end(), 0.0f);
        std::fill(mappedAnalysisPhase.begin(), mappedAnalysisPhase.end(), 0.0f);
        std::fill(outputPhase.begin(), outputPhase.end(), 0.0f);
        peakDetector.reset();
        identityPhaseLock.reset();
    }

    void processPitchFrame(const float* input,
                           float* output,
                           float pitchRatio,
                           float transientAmount,
                           bool highQuality) noexcept
    {
        jassert(fft != nullptr && input != nullptr && output != nullptr);

        constexpr auto fftSize = SoundShifterDSP::Config::fftSize;
        constexpr auto hopSize = SoundShifterDSP::Config::hopSize;
        const auto safeRatio = juce::jlimit(0.5f, 2.0f, pitchRatio);
        const auto safeTransient = juce::jlimit(0.0f, 1.0f, transientAmount);
        const auto phaseResetMaximum = highQuality
            ? SoundShifterDSP::Config::transientPhaseResetHQ
            : SoundShifterDSP::Config::transientPhaseResetFast;
        const auto phaseResetAmount = safeTransient * phaseResetMaximum;
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
        std::fill(synthesisPhaseReal.begin(), synthesisPhaseReal.end(), 0.0f);
        std::fill(synthesisPhaseImag.begin(), synthesisPhaseImag.end(), 0.0f);
        std::fill(mappedAnalysisPhase.begin(), mappedAnalysisPhase.end(), 0.0f);
        std::fill(outputPhase.begin(), outputPhase.end(), 0.0f);

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
            analysisPhase[static_cast<size_t>(bin)] = phase;
            analysisFrequency[static_cast<size_t>(bin)] =
                (static_cast<float>(bin) + binDeviation) * frequencyPerBin;
        }

        peakDetector.detect(analysisMagnitude.data(), numBins);

        for (int sourceBin = 0; sourceBin < numBins; ++sourceBin)
        {
            const auto targetPosition = static_cast<float>(sourceBin) * safeRatio;
            const auto targetBin = static_cast<int>(targetPosition);
            const auto fraction = targetPosition - static_cast<float>(targetBin);
            const auto shiftedFrequency = analysisFrequency[static_cast<size_t>(sourceBin)] * safeRatio;
            const auto magnitude = analysisMagnitude[static_cast<size_t>(sourceBin)];

            addToSynthesisBin(targetBin, magnitude * (1.0f - fraction), shiftedFrequency,
                              analysisPhase[static_cast<size_t>(sourceBin)]);
            addToSynthesisBin(targetBin + 1, magnitude * fraction, shiftedFrequency,
                              analysisPhase[static_cast<size_t>(sourceBin)]);
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
            outputPhase[static_cast<size_t>(bin)] = sumPhase[static_cast<size_t>(bin)];

            const auto phaseWeight = std::sqrt(synthesisPhaseReal[static_cast<size_t>(bin)]
                                              * synthesisPhaseReal[static_cast<size_t>(bin)]
                                              + synthesisPhaseImag[static_cast<size_t>(bin)]
                                              * synthesisPhaseImag[static_cast<size_t>(bin)]);
            mappedAnalysisPhase[static_cast<size_t>(bin)] = phaseWeight > 1.0e-12f
                ? std::atan2(synthesisPhaseImag[static_cast<size_t>(bin)],
                             synthesisPhaseReal[static_cast<size_t>(bin)])
                : 0.0f;
        }

        identityPhaseLock.apply(outputPhase.data(),
                                mappedAnalysisPhase.data(),
                                synthesisMagnitude.data(),
                                peakDetector.getPeakIndices(),
                                peakDetector.getPeakCount(),
                                safeRatio);

        // Reset part of the synthesis phase on attacks. The shortest wrapped
        // phase path avoids discontinuities while restoring transient focus.
        if (phaseResetAmount > 0.0f)
        {
            for (int bin = 0; bin < numBins; ++bin)
            {
                if (synthesisMagnitude[static_cast<size_t>(bin)] <= 1.0e-12f)
                    continue;

                const auto currentPhase = outputPhase[static_cast<size_t>(bin)];
                const auto targetPhase = mappedAnalysisPhase[static_cast<size_t>(bin)];
                const auto wrappedDelta = std::remainder(
                    targetPhase - currentPhase,
                    juce::MathConstants<float>::twoPi);

                outputPhase[static_cast<size_t>(bin)] =
                    currentPhase + wrappedDelta * phaseResetAmount;
            }
        }

        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto magnitude = synthesisMagnitude[static_cast<size_t>(bin)];
            const auto phase = outputPhase[static_cast<size_t>(bin)];
            setComplex(bin, magnitude * std::cos(phase), magnitude * std::sin(phase));
        }

        fft->performRealOnlyInverseTransform(transformData.data());
        window.applySynthesis(transformData.data(), fftSize);
        juce::FloatVectorOperations::copy(output, transformData.data(), fftSize);
    }

    [[nodiscard]] int getPeakCount() const noexcept
    {
        return peakDetector.getPeakCount();
    }

    [[nodiscard]] const int* getPeakIndices() const noexcept
    {
        return peakDetector.getPeakIndices();
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

    void addToSynthesisBin(int bin, float weightedMagnitude, float shiftedFrequency,
                           float sourcePhase) noexcept
    {
        if (!juce::isPositiveAndBelow(bin, numBins) || weightedMagnitude <= 0.0f)
            return;

        synthesisMagnitude[static_cast<size_t>(bin)] += weightedMagnitude;
        synthesisFrequency[static_cast<size_t>(bin)] += weightedMagnitude * shiftedFrequency;
        synthesisPhaseReal[static_cast<size_t>(bin)] += weightedMagnitude * std::cos(sourcePhase);
        synthesisPhaseImag[static_cast<size_t>(bin)] += weightedMagnitude * std::sin(sourcePhase);
    }

    std::unique_ptr<juce::dsp::FFT> fft;
    WindowProcessor window;
    std::vector<float> transformData;
    std::vector<float> lastPhase;
    std::vector<float> sumPhase;
    std::vector<float> analysisMagnitude;
    std::vector<float> analysisFrequency;
    std::vector<float> analysisPhase;
    std::vector<float> synthesisMagnitude;
    std::vector<float> synthesisFrequency;
    std::vector<float> synthesisPhaseReal;
    std::vector<float> synthesisPhaseImag;
    std::vector<float> mappedAnalysisPhase;
    std::vector<float> outputPhase;
    PeakDetector peakDetector;
    IdentityPhaseLock identityPhaseLock;
    double sampleRate = 44100.0;
};
