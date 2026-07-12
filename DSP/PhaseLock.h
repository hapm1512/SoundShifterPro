#pragma once

#include <JuceHeader.h>

class PhaseLock
{
public:
    void prepare(int newNumBins);
    void reset() noexcept;

    void apply(float* synthesisPhase,
               const float* mappedAnalysisPhase,
               const float* synthesisMagnitude,
               const int* sourcePeakIndices,
               int sourcePeakCount,
               float pitchRatio) noexcept;

private:
    static float wrapPhase(float phase) noexcept;

    std::vector<int> targetPeaks;
    std::vector<int> peakOwners;
    int numBins = 0;
};
