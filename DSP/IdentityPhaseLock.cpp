#include "IdentityPhaseLock.h"

void IdentityPhaseLock::prepare(int newNumBins, int newMaxPeaks)
{
    numBins = juce::jmax(0, newNumBins);
    maxPeaks = juce::jmax(1, newMaxPeaks);

    targetPeaks.assign(static_cast<size_t>(maxPeaks), 0);
    peakOwners.assign(static_cast<size_t>(numBins), -1);
    previousPeakBins.assign(static_cast<size_t>(maxPeaks), 0);
    previousPeakPhases.assign(static_cast<size_t>(maxPeaks), 0.0f);
    currentPeakBins.assign(static_cast<size_t>(maxPeaks), 0);
    currentPeakPhases.assign(static_cast<size_t>(maxPeaks), 0.0f);
    reset();
}

void IdentityPhaseLock::reset() noexcept
{
    std::fill(targetPeaks.begin(), targetPeaks.end(), 0);
    std::fill(peakOwners.begin(), peakOwners.end(), -1);
    std::fill(previousPeakBins.begin(), previousPeakBins.end(), 0);
    std::fill(previousPeakPhases.begin(), previousPeakPhases.end(), 0.0f);
    std::fill(currentPeakBins.begin(), currentPeakBins.end(), 0);
    std::fill(currentPeakPhases.begin(), currentPeakPhases.end(), 0.0f);
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
    if (numBins <= 0 || synthesisPhase == nullptr || mappedAnalysisPhase == nullptr
        || synthesisMagnitude == nullptr || sourcePeakIndices == nullptr
        || sourcePeakCount <= 0)
        return;

    currentPeakCount = 0;
    auto previousTargetPeak = -1;

    for (int index = 0; index < sourcePeakCount && currentPeakCount < maxPeaks; ++index)
    {
        const auto sourcePeak = sourcePeakIndices[index];
        const auto targetPeak = juce::jlimit(0, numBins - 1,
                                             juce::roundToInt(static_cast<float>(sourcePeak)
                                                              * pitchRatio));

        if (targetPeak == previousTargetPeak)
            continue;

        if (synthesisMagnitude[targetPeak] <= 1.0e-12f)
            continue;

        targetPeaks[static_cast<size_t>(currentPeakCount++)] = targetPeak;
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
        const auto peak = targetPeaks[static_cast<size_t>(peakIndex)];
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
        const auto peak = targetPeaks[static_cast<size_t>(peakIndex)];
        auto anchorPhase = synthesisPhase[peak];
        const auto previousIndex = findPreviousPeak(peak);

        if (previousIndex >= 0)
            anchorPhase = unwrapNear(anchorPhase,
                                     previousPeakPhases[static_cast<size_t>(previousIndex)]);

        synthesisPhase[peak] = anchorPhase;
        currentPeakBins[static_cast<size_t>(peakIndex)] = peak;
        currentPeakPhases[static_cast<size_t>(peakIndex)] = anchorPhase;
    }

    for (int bin = 0; bin < numBins; ++bin)
    {
        const auto ownerPeak = peakOwners[static_cast<size_t>(bin)];

        if (ownerPeak < 0 || synthesisMagnitude[bin] <= 1.0e-12f)
            continue;

        const auto relativeAnalysisPhase = wrapPhase(mappedAnalysisPhase[bin]
                                                      - mappedAnalysisPhase[ownerPeak]);
        synthesisPhase[bin] = synthesisPhase[ownerPeak] + relativeAnalysisPhase;
    }

    previousPeakCount = currentPeakCount;
    for (int index = 0; index < currentPeakCount; ++index)
    {
        previousPeakBins[static_cast<size_t>(index)] = currentPeakBins[static_cast<size_t>(index)];
        previousPeakPhases[static_cast<size_t>(index)] = currentPeakPhases[static_cast<size_t>(index)];
    }
}

float IdentityPhaseLock::wrapPhase(float phase) noexcept
{
    return std::remainder(phase, juce::MathConstants<float>::twoPi);
}

float IdentityPhaseLock::unwrapNear(float phase, float reference) noexcept
{
    return reference + wrapPhase(phase - reference);
}

int IdentityPhaseLock::findPreviousPeak(int targetBin) const noexcept
{
    constexpr int maximumTrackingDistance = 8;
    auto bestIndex = -1;
    auto bestDistance = maximumTrackingDistance + 1;

    for (int index = 0; index < previousPeakCount; ++index)
    {
        const auto distance = std::abs(previousPeakBins[static_cast<size_t>(index)] - targetBin);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = index;
        }
    }

    return bestDistance <= maximumTrackingDistance ? bestIndex : -1;
}
