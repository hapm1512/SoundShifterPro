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
    [[nodiscard]] float getPeakConfidence(int bin) const noexcept;
    [[nodiscard]] int getPeakLifetime(int bin) const noexcept;

private:
    [[nodiscard]] int findTrackedPeak(int bin, int binsToScan) const noexcept;
    void addPeak(int bin, float magnitude, float maximumMagnitude) noexcept;

    std::vector<int> peakIndices;
    std::vector<uint8_t> peakMask;
    std::vector<uint8_t> previousPeakMask;
    std::vector<float> peakConfidence;
    std::vector<float> previousConfidence;
    std::vector<uint16_t> peakLifetime;
    std::vector<uint16_t> previousLifetime;

    int peakCount = 0;
    float relativeThreshold = 0.01f;
    float minimumMagnitude = 1.0e-8f;
    float smoothedMaximumMagnitude = 0.0f;
    float smoothedNoiseFloor = 0.0f;
};
