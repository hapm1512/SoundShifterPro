#pragma once

#include <JuceHeader.h>

class PeakDetector
{
public:
    void prepare(int newNumBins);
    void reset() noexcept;

    void setRelativeThreshold(float newThreshold) noexcept;
    void setMinimumMagnitude(float newMinimumMagnitude) noexcept;

    void detect(const float* magnitudes, int magnitudeCount) noexcept;

    [[nodiscard]] int getPeakCount() const noexcept { return peakCount; }
    [[nodiscard]] const int* getPeakIndices() const noexcept { return peakIndices.data(); }
    [[nodiscard]] bool isPeak(int bin) const noexcept;

private:
    std::vector<int> peakIndices;
    std::vector<uint8_t> peakMask;
    int peakCount = 0;
    float relativeThreshold = 0.01f;
    float minimumMagnitude = 1.0e-8f;
};
