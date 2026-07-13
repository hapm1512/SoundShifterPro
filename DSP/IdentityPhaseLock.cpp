#include "IdentityPhaseLock.h"

void IdentityPhaseLock::prepare(int newNumBins, int newMaxPeaks)
{
    numBins = juce::jmax(0, newNumBins);
    maxPeaks = juce::jmax(1, newMaxPeaks);

    targetPeaks.assign(static_cast<size_t>(maxPeaks), 0);
    peakOwners.assign(static_cast<size_t>(numBins), -1);

    previousPeakBins.assign(static_cast<size_t>(maxPeaks), 0);
    previousPeakPhases.assign(static_cast<size_t>(maxPeaks), 0.0f);
    previousPeakMagnitudes.assign(static_cast<size_t>(maxPeaks), 0.0f);
    previousPeakVelocities.assign(static_cast<size_t>(maxPeaks), 0.0f);

    currentPeakBins.assign(static_cast<size_t>(maxPeaks), 0);
    currentPeakPhases.assign(static_cast<size_t>(maxPeaks), 0.0f);
    currentPeakMagnitudes.assign(static_cast<size_t>(maxPeaks), 0.0f);
    currentPeakVelocities.assign(static_cast<size_t>(maxPeaks), 0.0f);

    reset();
}

void IdentityPhaseLock::reset() noexcept
{
    std::fill(targetPeaks.begin(), targetPeaks.end(), 0);
    std::fill(peakOwners.begin(), peakOwners.end(), -1);

    std::fill(previousPeakBins.begin(), previousPeakBins.end(), 0);
    std::fill(previousPeakPhases.begin(), previousPeakPhases.end(), 0.0f);
    std::fill(previousPeakMagnitudes.begin(), previousPeakMagnitudes.end(), 0.0f);
    std::fill(previousPeakVelocities.begin(), previousPeakVelocities.end(), 0.0f);

    std::fill(currentPeakBins.begin(), currentPeakBins.end(), 0);
    std::fill(currentPeakPhases.begin(), currentPeakPhases.end(), 0.0f);
    std::fill(currentPeakMagnitudes.begin(), currentPeakMagnitudes.end(), 0.0f);
    std::fill(currentPeakVelocities.begin(), currentPeakVelocities.end(), 0.0f);

    previousPeakCount = 0;
    currentPeakCount = 0;
}

void IdentityPhaseLock::apply(float* synthesisPhase,
                              const float* mappedAnalysisPhase,
                              const float* synthesisMagnitude,
                              const int* sourcePeakIndices,
                              int sourcePeakCount,
                              float pitchRatio) noexcept
{
    if (numBins <= 0
        || synthesisPhase == nullptr
        || mappedAnalysisPhase == nullptr
        || synthesisMagnitude == nullptr
        || sourcePeakIndices == nullptr
        || sourcePeakCount <= 0)
        return;

    currentPeakCount = 0;
    auto previousTargetPeak = -1;

    for (int index = 0;
         index < sourcePeakCount && currentPeakCount < maxPeaks;
         ++index)
    {
        const auto sourcePeak = sourcePeakIndices[index];

        const auto targetPeak = juce::jlimit(
            0,
            numBins - 1,
            juce::roundToInt(
                static_cast<float>(sourcePeak) * pitchRatio));

        if (targetPeak == previousTargetPeak)
            continue;

        const auto peakMagnitude = synthesisMagnitude[targetPeak];

        if (peakMagnitude <= 1.0e-12f)
            continue;

        targetPeaks[static_cast<size_t>(currentPeakCount)] = targetPeak;
        currentPeakMagnitudes[static_cast<size_t>(currentPeakCount)] = peakMagnitude;
        ++currentPeakCount;
        previousTargetPeak = targetPeak;
    }

    if (currentPeakCount <= 0)
    {
        previousPeakCount = 0;
        return;
    }

    std::fill(peakOwners.begin(), peakOwners.end(), -1);

    for (int peakIndex = 0; peakIndex < currentPeakCount; ++peakIndex)
    {
        const auto peak =
            targetPeaks[static_cast<size_t>(peakIndex)];

        const auto left = peakIndex == 0
            ? 0
            : (targetPeaks[static_cast<size_t>(peakIndex - 1)] + peak) / 2 + 1;

        const auto right = peakIndex == currentPeakCount - 1
            ? numBins - 1
            : (peak + targetPeaks[static_cast<size_t>(peakIndex + 1)]) / 2;

        for (int bin = left; bin <= right; ++bin)
            peakOwners[static_cast<size_t>(bin)] = peak;
    }

    for (int peakIndex = 0; peakIndex < currentPeakCount; ++peakIndex)
    {
        const auto peak =
            targetPeaks[static_cast<size_t>(peakIndex)];

        const auto magnitude =
            currentPeakMagnitudes[static_cast<size_t>(peakIndex)];

        auto anchorPhase = synthesisPhase[peak];
        auto phaseVelocity = 0.0f;

        const auto previousIndex = findPreviousPeak(peak);

        if (previousIndex >= 0)
        {
            const auto previousPhase =
                previousPeakPhases[static_cast<size_t>(previousIndex)];

            const auto previousVelocity =
                previousPeakVelocities[static_cast<size_t>(previousIndex)];

            const auto unwrappedPhase =
                unwrapNear(anchorPhase, previousPhase);

            const auto measuredVelocity =
                wrapPhase(unwrappedPhase - previousPhase);

            const auto trackingBlend =
                calculateTrackingBlend(peak, previousIndex, magnitude);

            const auto predictedPhase =
                previousPhase + previousVelocity;

            const auto predictionError =
                wrapPhase(unwrappedPhase - predictedPhase);

            const auto velocityBlend =
                0.18f + 0.32f * (1.0f - trackingBlend);

            const auto dampedPrediction =
                predictedPhase
                + predictionError * (1.0f - trackingBlend * 0.82f);

            anchorPhase = unwrapNear(dampedPrediction, previousPhase);

            phaseVelocity =
                previousVelocity
                + wrapPhase(measuredVelocity - previousVelocity)
                  * velocityBlend;
        }

        synthesisPhase[peak] = anchorPhase;

        currentPeakBins[static_cast<size_t>(peakIndex)] = peak;
        currentPeakPhases[static_cast<size_t>(peakIndex)] = anchorPhase;
        currentPeakVelocities[static_cast<size_t>(peakIndex)] = phaseVelocity;
    }

    for (int bin = 0; bin < numBins; ++bin)
    {
        const auto ownerPeak =
            peakOwners[static_cast<size_t>(bin)];

        if (ownerPeak < 0 || synthesisMagnitude[bin] <= 1.0e-12f)
            continue;

        const auto relativeAnalysisPhase =
            wrapPhase(
                mappedAnalysisPhase[bin]
                - mappedAnalysisPhase[ownerPeak]);

        const auto lockedPhase =
            synthesisPhase[ownerPeak] + relativeAnalysisPhase;

        const auto ownerMagnitude =
            synthesisMagnitude[ownerPeak];

        const auto magnitudeRatio =
            ownerMagnitude > 1.0e-12f
                ? synthesisMagnitude[bin] / ownerMagnitude
                : 0.0f;

        const auto distance =
            static_cast<float>(std::abs(bin - ownerPeak));

        const auto distanceWeight =
            1.0f - juce::jlimit(0.0f, 1.0f, distance / 18.0f);

        const auto harmonicWeight =
            1.0f - juce::jlimit(0.0f, 1.0f, magnitudeRatio);

        const auto ownerPreviousIndex =
            findPreviousPeak(ownerPeak);

        const auto phaseContinuity =
            ownerPreviousIndex >= 0
                ? calculateTrackingBlend(
                      ownerPeak,
                      ownerPreviousIndex,
                      ownerMagnitude)
                : 0.55f;

        const auto lockStrength = juce::jlimit(
            0.58f,
            0.98f,
            0.60f
                + 0.20f * distanceWeight
                + 0.08f * harmonicWeight
                + 0.10f * phaseContinuity);

        const auto currentPhase = synthesisPhase[bin];

        const auto phaseError =
            wrapPhase(lockedPhase - currentPhase);

        synthesisPhase[bin] =
            currentPhase + phaseError * lockStrength;
    }

    previousPeakCount = currentPeakCount;

    for (int index = 0; index < currentPeakCount; ++index)
    {
        previousPeakBins[static_cast<size_t>(index)] =
            currentPeakBins[static_cast<size_t>(index)];

        previousPeakPhases[static_cast<size_t>(index)] =
            currentPeakPhases[static_cast<size_t>(index)];

        previousPeakMagnitudes[static_cast<size_t>(index)] =
            currentPeakMagnitudes[static_cast<size_t>(index)];

        previousPeakVelocities[static_cast<size_t>(index)] =
            currentPeakVelocities[static_cast<size_t>(index)];
    }
}

float IdentityPhaseLock::wrapPhase(float phase) noexcept
{
    return std::remainder(
        phase,
        juce::MathConstants<float>::twoPi);
}

float IdentityPhaseLock::unwrapNear(float phase, float reference) noexcept
{
    return reference + wrapPhase(phase - reference);
}

int IdentityPhaseLock::findPreviousPeak(int targetBin) const noexcept
{
    constexpr int maximumTrackingDistance = 12;

    auto bestIndex = -1;
    auto bestScore = -1.0f;

    for (int index = 0; index < previousPeakCount; ++index)
    {
        const auto distance = static_cast<float>(
            std::abs(
                previousPeakBins[static_cast<size_t>(index)] - targetBin));

        if (distance > static_cast<float>(maximumTrackingDistance))
            continue;

        const auto distanceConfidence =
            1.0f - distance / static_cast<float>(maximumTrackingDistance + 1);

        const auto magnitude =
            previousPeakMagnitudes[static_cast<size_t>(index)];

        const auto magnitudeConfidence =
            juce::jlimit(0.0f, 1.0f, magnitude);

        const auto score =
            0.64f * distanceConfidence
            + 0.24f * magnitudeConfidence
            + 0.12f * (1.0f - distance / static_cast<float>(maximumTrackingDistance));

        if (score > bestScore)
        {
            bestScore = score;
            bestIndex = index;
        }
    }

    return bestIndex;
}

float IdentityPhaseLock::calculateTrackingBlend(
    int currentPeak,
    int previousIndex,
    float currentMagnitude) const noexcept
{
    if (previousIndex < 0 || previousIndex >= previousPeakCount)
        return 0.0f;

    const auto previousBin =
        previousPeakBins[static_cast<size_t>(previousIndex)];

    const auto distance =
        static_cast<float>(std::abs(currentPeak - previousBin));

    const auto distanceConfidence =
        1.0f - juce::jlimit(0.0f, 1.0f, distance / 12.0f);

    const auto previousMagnitude =
        previousPeakMagnitudes[static_cast<size_t>(previousIndex)];

    const auto magnitudeRatio =
        previousMagnitude > 1.0e-12f
            ? currentMagnitude / previousMagnitude
            : 1.0f;

    const auto magnitudeConfidence =
        1.0f - juce::jlimit(
            0.0f,
            1.0f,
            std::abs(
                std::log2(
                    juce::jmax(1.0e-6f, magnitudeRatio))) * 0.45f);

    const auto blend =
        0.46f
        + 0.46f * distanceConfidence * magnitudeConfidence;

    return juce::jlimit(0.46f, 0.92f, blend);
}
