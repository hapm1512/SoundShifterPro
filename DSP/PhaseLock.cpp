#include "PhaseLock.h"

void PhaseLock::prepare(int newNumBins)
{
    numBins = juce::jmax(0, newNumBins);
    targetPeaks.assign(static_cast<size_t>(numBins), 0);
    peakOwners.assign(static_cast<size_t>(numBins), -1);
    reset();
}

void PhaseLock::reset() noexcept
{
    std::fill(targetPeaks.begin(), targetPeaks.end(), 0);
    std::fill(peakOwners.begin(), peakOwners.end(), -1);
}

void PhaseLock::apply(float* synthesisPhase,
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

    auto targetPeakCount = 0;
    auto previousTargetPeak = -1;

    for (int index = 0; index < sourcePeakCount; ++index)
    {
        const auto sourcePeak = sourcePeakIndices[index];
        const auto targetPeak = juce::jlimit(0, numBins - 1,
                                             juce::roundToInt(static_cast<float>(sourcePeak)
                                                              * pitchRatio));

        if (targetPeak == previousTargetPeak)
            continue;

        if (synthesisMagnitude[targetPeak] <= 1.0e-12f)
            continue;

        targetPeaks[static_cast<size_t>(targetPeakCount++)] = targetPeak;
        previousTargetPeak = targetPeak;
    }

    if (targetPeakCount <= 0)
        return;

    std::fill(peakOwners.begin(), peakOwners.end(), -1);

    for (int peakIndex = 0; peakIndex < targetPeakCount; ++peakIndex)
    {
        const auto peak = targetPeaks[static_cast<size_t>(peakIndex)];
        const auto left = peakIndex == 0
            ? 0
            : (targetPeaks[static_cast<size_t>(peakIndex - 1)] + peak) / 2 + 1;
        const auto right = peakIndex == targetPeakCount - 1
            ? numBins - 1
            : (peak + targetPeaks[static_cast<size_t>(peakIndex + 1)]) / 2;

        for (int bin = left; bin <= right; ++bin)
            peakOwners[static_cast<size_t>(bin)] = peak;
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
}

float PhaseLock::wrapPhase(float phase) noexcept
{
    return std::remainder(phase, juce::MathConstants<float>::twoPi);
}
