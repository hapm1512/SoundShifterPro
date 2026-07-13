#pragma once

#include <JuceHeader.h>

class IdentityPhaseLock
{
public:
    void prepare(int newNumBins, int newMaxPeaks);
    void reset() noexcept;

    void apply(float* synthesisPhase,
               const float* mappedAnalysisPhase,
               const float* synthesisMagnitude,
               const int* sourcePeakIndices,
               int sourcePeakCount,
               float pitchRatio) noexcept;

private:
    static float wrapPhase(float phase) noexcept;
    static float unwrapNear(float phase, float reference) noexcept;

    int findPreviousPeak(int targetBin) const noexcept;
    float calculateTrackingBlend(int currentPeak,
                                 int previousIndex,
                                 float currentMagnitude) const noexcept;

    std::vector<int> targetPeaks;
    std::vector<int> peakOwners;
    std::vector<int> previousPeakBins;
    std::vector<float> previousPeakPhases;
    std::vector<float> previousPeakMagnitudes;
    std::vector<int> currentPeakBins;
    std::vector<float> currentPeakPhases;
    std::vector<float> currentPeakMagnitudes;

    int previousPeakCount = 0;
    int currentPeakCount = 0;
    int numBins = 0;
    int maxPeaks = 0;
};
