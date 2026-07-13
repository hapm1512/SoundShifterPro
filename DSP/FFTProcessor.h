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
        adaptiveMagnitude.assign(static_cast<size_t>(numBins), 0.0f);
        spectralEnvelope.assign(static_cast<size_t>(numBins), 0.0f);
        leakageCompensatedMagnitude.assign(static_cast<size_t>(numBins), 0.0f);
        mappedEnergy.assign(static_cast<size_t>(numBins), 0.0f);
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
        clearFrameBuffers();
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
        const auto safeRatio = juce::jlimit(
            SoundShifterDSP::Config::minimumPitchRatio,
            SoundShifterDSP::Config::maximumPitchRatio,
            pitchRatio);
        const auto safeTransient = juce::jlimit(0.0f, 1.0f, transientAmount);
        const auto phaseResetMaximum = highQuality
            ? SoundShifterDSP::Config::transientPhaseResetHQ
            : SoundShifterDSP::Config::transientPhaseResetFast;
        const auto phaseResetAmount = safeTransient * phaseResetMaximum;
        const auto frequencyPerBin = static_cast<float>(sampleRate / fftSize);
        const auto expectedPhase = juce::MathConstants<float>::twoPi
                                 * static_cast<float>(hopSize)
                                 / static_cast<float>(fftSize);

        window.setHighQuality(highQuality);

        juce::FloatVectorOperations::clear(transformData.data(), fftSize * 2);
        juce::FloatVectorOperations::copy(transformData.data(), input, fftSize);
        window.applyAnalysis(transformData.data(), fftSize);
        fft->performRealOnlyForwardTransform(transformData.data());

        std::fill(synthesisMagnitude.begin(), synthesisMagnitude.end(), 0.0f);
        std::fill(synthesisFrequency.begin(), synthesisFrequency.end(), 0.0f);
        std::fill(synthesisPhaseReal.begin(), synthesisPhaseReal.end(), 0.0f);
        std::fill(synthesisPhaseImag.begin(), synthesisPhaseImag.end(), 0.0f);
        std::fill(adaptiveMagnitude.begin(), adaptiveMagnitude.end(), 0.0f);
        std::fill(spectralEnvelope.begin(), spectralEnvelope.end(), 0.0f);
        std::fill(leakageCompensatedMagnitude.begin(),
                  leakageCompensatedMagnitude.end(),
                  0.0f);
        std::fill(mappedEnergy.begin(), mappedEnergy.end(), 0.0f);
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

        prepareAdaptiveSpectrum(
            safeRatio,
            safeTransient,
            highQuality);

        applySpectralLeakageCompensation(
            safeRatio,
            safeTransient,
            highQuality);

        for (int sourceBin = 0; sourceBin < numBins; ++sourceBin)
        {
            const auto interpolationOffset =
                highQuality ? calculatePeakInterpolationOffset(sourceBin) : 0.0f;

            const auto sourcePosition =
                static_cast<float>(sourceBin) + interpolationOffset;

            const auto targetPosition = sourcePosition * safeRatio;
            const auto targetBin = static_cast<int>(std::floor(targetPosition));
            const auto fraction = targetPosition - static_cast<float>(targetBin);

            const auto shiftedFrequency =
                analysisFrequency[static_cast<size_t>(sourceBin)] * safeRatio;

            const auto enhancement =
                calculateHarmonicEnhancement(sourceBin, safeRatio, highQuality);

            const auto binWeight =
                calculateAdaptiveBinWeight(
                    sourceBin,
                    safeRatio,
                    safeTransient,
                    highQuality);

            const auto magnitude =
                leakageCompensatedMagnitude[static_cast<size_t>(sourceBin)]
                * enhancement
                * binWeight;

            distributePitchMappedEnergy(
                targetBin,
                fraction,
                magnitude,
                shiftedFrequency,
                analysisPhase[static_cast<size_t>(sourceBin)],
                sourceBin,
                safeRatio,
                safeTransient,
                highQuality);
        }

        applyHarmonicBinRedistribution(
            safeRatio,
            safeTransient,
            highQuality);

        preserveMappedEnergy(
            leakageCompensatedMagnitude,
            synthesisMagnitude,
            highQuality);

        juce::FloatVectorOperations::clear(transformData.data(), fftSize * 2);

        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto magnitude = synthesisMagnitude[static_cast<size_t>(bin)];
            auto frequency = synthesisFrequency[static_cast<size_t>(bin)];

            if (magnitude > SoundShifterDSP::Config::magnitudeFloor)
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
            mappedAnalysisPhase[static_cast<size_t>(bin)] = phaseWeight > SoundShifterDSP::Config::magnitudeFloor
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
                if (synthesisMagnitude[static_cast<size_t>(bin)] <= SoundShifterDSP::Config::magnitudeFloor)
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

    void clearFrameBuffers() noexcept
    {
        juce::FloatVectorOperations::clear(synthesisMagnitude.data(), numBins);
        juce::FloatVectorOperations::clear(synthesisFrequency.data(), numBins);
        juce::FloatVectorOperations::clear(synthesisPhaseReal.data(), numBins);
        juce::FloatVectorOperations::clear(synthesisPhaseImag.data(), numBins);
        juce::FloatVectorOperations::clear(adaptiveMagnitude.data(), numBins);
        juce::FloatVectorOperations::clear(spectralEnvelope.data(), numBins);
        juce::FloatVectorOperations::clear(leakageCompensatedMagnitude.data(), numBins);
        juce::FloatVectorOperations::clear(mappedEnergy.data(), numBins);
        juce::FloatVectorOperations::clear(mappedAnalysisPhase.data(), numBins);
        juce::FloatVectorOperations::clear(outputPhase.data(), numBins);
    }

    void prepareAdaptiveSpectrum(float pitchRatio,
                                 float transientAmount,
                                 bool highQuality) noexcept
    {
        if (!highQuality)
        {
            juce::FloatVectorOperations::copy(
                adaptiveMagnitude.data(),
                analysisMagnitude.data(),
                numBins);
            return;
        }

        const auto shiftDistance =
            juce::jlimit(0.0f, 1.0f, std::abs(std::log2(pitchRatio)));

        const auto tonalBlend =
            juce::jlimit(0.0f, 1.0f,
                         (1.0f - transientAmount) * (0.45f + 0.40f * shiftDistance));

        constexpr int envelopeRadius = 6;

        for (int bin = 0; bin < numBins; ++bin)
        {
            float envelopeSum = 0.0f;
            float envelopeWeight = 0.0f;

            for (int offset = -envelopeRadius; offset <= envelopeRadius; ++offset)
            {
                const auto sourceBin = juce::jlimit(0, numBins - 1, bin + offset);
                const auto distance = static_cast<float>(std::abs(offset));
                const auto weight = 1.0f - distance / static_cast<float>(envelopeRadius + 1);

                envelopeSum += analysisMagnitude[static_cast<size_t>(sourceBin)] * weight;
                envelopeWeight += weight;
            }

            spectralEnvelope[static_cast<size_t>(bin)] =
                envelopeWeight > 0.0f ? envelopeSum / envelopeWeight : 0.0f;
        }

        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto normalisedBin =
                static_cast<float>(bin) / static_cast<float>(numBins - 1);

            const auto radius =
                normalisedBin < 0.18f ? 4
              : normalisedBin < 0.55f ? 2
                                      : 1;

            float localSum = 0.0f;
            float localWeight = 0.0f;

            for (int offset = -radius; offset <= radius; ++offset)
            {
                const auto sourceBin = juce::jlimit(0, numBins - 1, bin + offset);
                const auto distance = static_cast<float>(std::abs(offset));
                const auto weight = 1.0f - distance / static_cast<float>(radius + 1);

                localSum += analysisMagnitude[static_cast<size_t>(sourceBin)] * weight;
                localWeight += weight;
            }

            const auto localAverage =
                localWeight > 0.0f ? localSum / localWeight : 0.0f;

            const auto original =
                analysisMagnitude[static_cast<size_t>(bin)];

            const auto bandBlend =
                normalisedBin < 0.18f ? tonalBlend
              : normalisedBin < 0.55f ? tonalBlend * 0.58f
                                      : tonalBlend * 0.24f;

            const auto resolutionMagnitude =
                juce::jmap(bandBlend, original, localAverage);

            const auto envelope =
                spectralEnvelope[static_cast<size_t>(bin)];

            const auto envelopeBlend =
                0.10f * shiftDistance * (1.0f - transientAmount);

            adaptiveMagnitude[static_cast<size_t>(bin)] =
                juce::jmax(0.0f,
                           juce::jmap(envelopeBlend,
                                      resolutionMagnitude,
                                      0.82f * resolutionMagnitude + 0.18f * envelope));
        }
    }

    void applySpectralLeakageCompensation(float pitchRatio,
                                            float transientAmount,
                                            bool highQuality) noexcept
    {
        if (!highQuality)
        {
            juce::FloatVectorOperations::copy(
                leakageCompensatedMagnitude.data(),
                adaptiveMagnitude.data(),
                numBins);
            return;
        }

        const auto shiftDistance = juce::jlimit(
            0.0f,
            1.0f,
            std::abs(std::log2(pitchRatio)));

        const auto transientProtection =
            1.0f - 0.82f * transientAmount;

        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto centre =
                adaptiveMagnitude[static_cast<size_t>(bin)];

            if (bin <= 1 || bin >= numBins - 2 || centre <= SoundShifterDSP::Config::magnitudeFloor)
            {
                leakageCompensatedMagnitude[static_cast<size_t>(bin)] = centre;
                continue;
            }

            const auto left1 =
                adaptiveMagnitude[static_cast<size_t>(bin - 1)];

            const auto right1 =
                adaptiveMagnitude[static_cast<size_t>(bin + 1)];

            const auto left2 =
                adaptiveMagnitude[static_cast<size_t>(bin - 2)];

            const auto right2 =
                adaptiveMagnitude[static_cast<size_t>(bin + 2)];

            const auto neighbourAverage =
                0.40f * (left1 + right1)
                + 0.10f * (left2 + right2);

            const auto localPeak =
                centre >= left1 && centre >= right1;

            const auto sideLobeRatio = juce::jlimit(
                0.0f,
                1.0f,
                neighbourAverage / (centre + neighbourAverage + SoundShifterDSP::Config::magnitudeFloor));

            const auto normalisedBin =
                static_cast<float>(bin)
                / static_cast<float>(numBins - 1);

            const auto highBandFactor = juce::jlimit(
                0.0f,
                1.0f,
                (normalisedBin - 0.55f) / 0.45f);

            const auto compensationStrength =
                (0.10f + 0.10f * highBandFactor)
                * shiftDistance
                * transientProtection;

            auto corrected = centre;

            if (localPeak)
            {
                const auto peakSharpening =
                    compensationStrength
                    * (1.0f - sideLobeRatio)
                    * centre;

                corrected += peakSharpening;
            }
            else
            {
                const auto suppression =
                    compensationStrength
                    * sideLobeRatio
                    * juce::jmin(centre, neighbourAverage);

                corrected -= suppression;
            }

            const auto continuityReference =
                0.50f * centre
                + 0.25f * (left1 + right1);

            const auto continuityBlend =
                0.05f
                * shiftDistance
                * transientProtection
                * (1.0f - highBandFactor * 0.35f);

            corrected = juce::jmap(
                continuityBlend,
                corrected,
                continuityReference);

            leakageCompensatedMagnitude[static_cast<size_t>(bin)] =
                juce::jmax(0.0f, corrected);
        }

        preserveLeakageEnergy();
    }

    void preserveLeakageEnergy() noexcept
    {
        double inputEnergy = 0.0;
        double outputEnergy = 0.0;

        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto input =
                static_cast<double>(
                    adaptiveMagnitude[static_cast<size_t>(bin)]);

            const auto output =
                static_cast<double>(
                    leakageCompensatedMagnitude[static_cast<size_t>(bin)]);

            inputEnergy += input * input;
            outputEnergy += output * output;
        }

        if (inputEnergy <= SoundShifterDSP::Config::energyFloor || outputEnergy <= SoundShifterDSP::Config::energyFloor)
            return;

        const auto gain = juce::jlimit(
            SoundShifterDSP::Config::hqLeakageGainMinimum,
            SoundShifterDSP::Config::hqLeakageGainMaximum,
            static_cast<float>(std::sqrt(inputEnergy / outputEnergy)));

        juce::FloatVectorOperations::multiply(
            leakageCompensatedMagnitude.data(),
            gain,
            numBins);
    }

    [[nodiscard]] float calculatePeakInterpolationOffset(int bin) const noexcept
    {
        if (bin <= 0 || bin >= numBins - 1)
            return 0.0f;

        const auto left = analysisMagnitude[static_cast<size_t>(bin - 1)];
        const auto centre = analysisMagnitude[static_cast<size_t>(bin)];
        const auto right = analysisMagnitude[static_cast<size_t>(bin + 1)];

        if (centre <= left || centre < right)
            return 0.0f;

        const auto denominator = left - 2.0f * centre + right;

        if (std::abs(denominator) <= SoundShifterDSP::Config::magnitudeFloor)
            return 0.0f;

        const auto offset = 0.5f * (left - right) / denominator;
        return juce::jlimit(-0.5f, 0.5f, offset);
    }

    [[nodiscard]] float calculateAdaptiveBinWeight(
        int bin,
        float pitchRatio,
        float transientAmount,
        bool highQuality) const noexcept
    {
        if (!highQuality || bin <= 0 || bin >= numBins - 1)
            return 1.0f;

        const auto magnitude =
            analysisMagnitude[static_cast<size_t>(bin)];

        if (magnitude <= SoundShifterDSP::Config::magnitudeFloor)
            return 1.0f;

        const auto left =
            analysisMagnitude[static_cast<size_t>(bin - 1)];

        const auto right =
            analysisMagnitude[static_cast<size_t>(bin + 1)];

        const auto localMaximum = juce::jmax(left, right);
        const auto peakDominance = juce::jlimit(
            0.0f,
            1.0f,
            (magnitude - localMaximum) / (magnitude + SoundShifterDSP::Config::magnitudeFloor));

        const auto envelope =
            spectralEnvelope[static_cast<size_t>(bin)];

        const auto envelopeRatio = juce::jlimit(
            0.0f,
            2.0f,
            magnitude / (envelope + SoundShifterDSP::Config::magnitudeFloor));

        const auto tonalConfidence = juce::jlimit(
            0.0f,
            1.0f,
            0.55f * peakDominance
                + 0.45f * juce::jlimit(0.0f, 1.0f, envelopeRatio - 0.65f));

        const auto normalisedBin =
            static_cast<float>(bin) / static_cast<float>(numBins - 1);

        const auto shiftDistance = juce::jlimit(
            0.0f,
            1.0f,
            std::abs(std::log2(pitchRatio)));

        const auto transientProtection =
            1.0f - 0.70f * transientAmount;

        const auto highBandProtection =
            1.0f - 0.35f * juce::jlimit(
                0.0f,
                1.0f,
                (normalisedBin - 0.72f) / 0.28f);

        const auto tonalBoost =
            0.10f * tonalConfidence * shiftDistance * transientProtection;

        const auto noiseSuppression =
            0.08f * (1.0f - tonalConfidence)
            * shiftDistance
            * transientProtection
            * highBandProtection;

        return juce::jlimit(
            0.92f,
            1.10f,
            1.0f + tonalBoost - noiseSuppression);
    }

    [[nodiscard]] static float calculateHarmonicEnhancement(
        int bin,
        float pitchRatio,
        bool highQuality) noexcept
    {
        if (!highQuality)
            return 1.0f;

        const auto normalisedBin =
            static_cast<float>(bin) / static_cast<float>(numBins - 1);

        const auto shiftDistance =
            juce::jlimit(0.0f, 1.0f, std::abs(std::log2(pitchRatio)));

        const auto presenceWeight =
            juce::jlimit(0.0f, 1.0f, (normalisedBin - 0.18f) / 0.62f);

        const auto airRolloff =
            1.0f - juce::jlimit(0.0f, 1.0f, (normalisedBin - 0.82f) / 0.18f);

        const auto enhancement =
            1.0f + 0.10f * shiftDistance * presenceWeight * airRolloff;

        return juce::jlimit(
            1.0f,
            SoundShifterDSP::Config::hqHarmonicEnhancementMaximum,
            enhancement);
    }

    void distributePitchMappedEnergy(int targetBin,
                                     float fraction,
                                     float magnitude,
                                     float shiftedFrequency,
                                     float sourcePhase,
                                     int sourceBin,
                                     float pitchRatio,
                                     float transientAmount,
                                     bool highQuality) noexcept
    {
        if (magnitude <= 0.0f)
            return;

        if (!highQuality)
        {
            addToSynthesisBin(
                targetBin,
                magnitude * (1.0f - fraction),
                shiftedFrequency,
                sourcePhase);

            addToSynthesisBin(
                targetBin + 1,
                magnitude * fraction,
                shiftedFrequency,
                sourcePhase);
            return;
        }

        const auto sourceMagnitude =
            analysisMagnitude[static_cast<size_t>(sourceBin)];

        const auto left =
            sourceBin > 0
                ? analysisMagnitude[static_cast<size_t>(sourceBin - 1)]
                : sourceMagnitude;

        const auto right =
            sourceBin < numBins - 1
                ? analysisMagnitude[static_cast<size_t>(sourceBin + 1)]
                : sourceMagnitude;

        const auto isPeak =
            sourceMagnitude >= left && sourceMagnitude >= right;

        const auto shiftDistance = juce::jlimit(
            0.0f,
            1.0f,
            std::abs(std::log2(pitchRatio)));

        const auto peakFocus =
            isPeak
                ? 0.82f + 0.14f * shiftDistance
                : 0.58f + 0.10f * shiftDistance;

        const auto transientFocus =
            juce::jmap(transientAmount, peakFocus, 0.96f);

        const auto cubicLeft =
            0.5f * std::pow(1.0f - fraction, 2.0f);

        const auto cubicCentre =
            0.75f - std::pow(fraction - 0.5f, 2.0f);

        const auto cubicRight =
            0.5f * std::pow(fraction, 2.0f);

        auto w0 = (1.0f - fraction) * transientFocus
                + cubicLeft * (1.0f - transientFocus);

        auto w1 = fraction * transientFocus
                + cubicRight * (1.0f - transientFocus);

        auto wMinus = (1.0f - transientFocus)
                    * cubicCentre
                    * (1.0f - fraction)
                    * 0.18f;

        auto wPlus = (1.0f - transientFocus)
                   * cubicCentre
                   * fraction
                   * 0.18f;

        const auto totalWeight = wMinus + w0 + w1 + wPlus;

        if (totalWeight > SoundShifterDSP::Config::magnitudeFloor)
        {
            const auto normalise = 1.0f / totalWeight;
            wMinus *= normalise;
            w0 *= normalise;
            w1 *= normalise;
            wPlus *= normalise;
        }

        addToSynthesisBin(
            targetBin - 1,
            magnitude * wMinus,
            shiftedFrequency,
            sourcePhase);

        addToSynthesisBin(
            targetBin,
            magnitude * w0,
            shiftedFrequency,
            sourcePhase);

        addToSynthesisBin(
            targetBin + 1,
            magnitude * w1,
            shiftedFrequency,
            sourcePhase);

        addToSynthesisBin(
            targetBin + 2,
            magnitude * wPlus,
            shiftedFrequency,
            sourcePhase);
    }

    void applyHarmonicBinRedistribution(float pitchRatio,
                                         float transientAmount,
                                         bool highQuality) noexcept
    {
        if (!highQuality)
            return;

        const auto shiftDistance = juce::jlimit(
            0.0f,
            1.0f,
            std::abs(std::log2(pitchRatio)));

        const auto transientProtection =
            1.0f - 0.85f * transientAmount;

        juce::FloatVectorOperations::copy(
            mappedEnergy.data(),
            synthesisMagnitude.data(),
            numBins);

        for (int bin = 2; bin < numBins - 2; ++bin)
        {
            const auto centre =
                mappedEnergy[static_cast<size_t>(bin)];

            if (centre <= SoundShifterDSP::Config::magnitudeFloor)
                continue;

            const auto left1 =
                mappedEnergy[static_cast<size_t>(bin - 1)];

            const auto right1 =
                mappedEnergy[static_cast<size_t>(bin + 1)];

            const auto left2 =
                mappedEnergy[static_cast<size_t>(bin - 2)];

            const auto right2 =
                mappedEnergy[static_cast<size_t>(bin + 2)];

            const auto localAverage =
                0.44f * centre
                + 0.20f * (left1 + right1)
                + 0.08f * (left2 + right2);

            const auto localMaximum =
                juce::jmax(
                    centre,
                    juce::jmax(
                        juce::jmax(left1, right1),
                        juce::jmax(left2, right2)));

            const auto harmonicConfidence =
                localMaximum > SoundShifterDSP::Config::magnitudeFloor
                    ? juce::jlimit(
                          0.0f,
                          1.0f,
                          centre / localMaximum)
                    : 0.0f;

            const auto valleyAmount =
                localAverage > SoundShifterDSP::Config::magnitudeFloor
                    ? juce::jlimit(
                          0.0f,
                          1.0f,
                          (localAverage - centre)
                              / localAverage)
                    : 0.0f;

            const auto normalisedBin =
                static_cast<float>(bin)
                / static_cast<float>(numBins - 1);

            const auto lowBandProtection =
                1.0f - 0.65f * juce::jlimit(
                    0.0f,
                    1.0f,
                    (0.16f - normalisedBin) / 0.16f);

            const auto highBandProtection =
                1.0f - 0.45f * juce::jlimit(
                    0.0f,
                    1.0f,
                    (normalisedBin - 0.78f) / 0.22f);

            const auto redistributionStrength =
                0.10f
                * shiftDistance
                * transientProtection
                * lowBandProtection
                * highBandProtection;

            const auto continuityBoost =
                redistributionStrength
                * valleyAmount
                * (0.55f + 0.45f * harmonicConfidence);

            const auto peakRestraint =
                redistributionStrength
                * juce::jlimit(
                    0.0f,
                    1.0f,
                    (centre - localAverage)
                        / (centre + SoundShifterDSP::Config::magnitudeFloor))
                * 0.35f;

            const auto gain = juce::jlimit(
                0.95f,
                1.08f,
                1.0f + continuityBoost - peakRestraint);

            synthesisMagnitude[static_cast<size_t>(bin)] *= gain;
            synthesisFrequency[static_cast<size_t>(bin)] *= gain;
            synthesisPhaseReal[static_cast<size_t>(bin)] *= gain;
            synthesisPhaseImag[static_cast<size_t>(bin)] *= gain;
        }
    }

    void preserveMappedEnergy(const std::vector<float>& sourceMagnitude,
                              std::vector<float>& targetMagnitude,
                              bool highQuality) noexcept
    {
        double sourceEnergy = 0.0;
        double targetEnergy = 0.0;

        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto source =
                static_cast<double>(sourceMagnitude[static_cast<size_t>(bin)]);

            const auto target =
                static_cast<double>(targetMagnitude[static_cast<size_t>(bin)]);

            sourceEnergy += source * source;
            targetEnergy += target * target;
        }

        if (sourceEnergy <= SoundShifterDSP::Config::energyFloor || targetEnergy <= SoundShifterDSP::Config::energyFloor)
            return;

        const auto minimumGain = highQuality
            ? SoundShifterDSP::Config::hqEnergyGainMinimum
            : SoundShifterDSP::Config::fastEnergyGainMinimum;

        const auto maximumGain = highQuality
            ? SoundShifterDSP::Config::hqEnergyGainMaximum
            : SoundShifterDSP::Config::fastEnergyGainMaximum;

        const auto gain = juce::jlimit(
            minimumGain,
            maximumGain,
            static_cast<float>(std::sqrt(sourceEnergy / targetEnergy)));

        juce::FloatVectorOperations::multiply(
            targetMagnitude.data(),
            gain,
            numBins);

        juce::FloatVectorOperations::multiply(
            synthesisFrequency.data(),
            gain,
            numBins);

        juce::FloatVectorOperations::multiply(
            synthesisPhaseReal.data(),
            gain,
            numBins);

        juce::FloatVectorOperations::multiply(
            synthesisPhaseImag.data(),
            gain,
            numBins);
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
    std::vector<float> adaptiveMagnitude;
    std::vector<float> spectralEnvelope;
    std::vector<float> leakageCompensatedMagnitude;
    std::vector<float> mappedEnergy;
    std::vector<float> mappedAnalysisPhase;
    std::vector<float> outputPhase;
    PeakDetector peakDetector;
    IdentityPhaseLock identityPhaseLock;
    double sampleRate = 44100.0;
};
