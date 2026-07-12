#include "PeakDetector.h"

void PeakDetector::prepare(int newNumBins)
{
    const auto safeNumBins = juce::jmax(0, newNumBins);
    peakIndices.assign(static_cast<size_t>(safeNumBins), 0);
    peakMask.assign(static_cast<size_t>(safeNumBins), 0);
    reset();
}

void PeakDetector::reset() noexcept
{
    peakCount = 0;
    std::fill(peakMask.begin(), peakMask.end(), static_cast<uint8_t>(0));
}

void PeakDetector::setRelativeThreshold(float newThreshold) noexcept
{
    relativeThreshold = juce::jlimit(0.0f, 1.0f, newThreshold);
}

void PeakDetector::setMinimumMagnitude(float newMinimumMagnitude) noexcept
{
    minimumMagnitude = juce::jmax(0.0f, newMinimumMagnitude);
}

void PeakDetector::detect(const float* magnitudes, int magnitudeCount) noexcept
{
    peakCount = 0;
    std::fill(peakMask.begin(), peakMask.end(), static_cast<uint8_t>(0));

    if (magnitudes == nullptr || magnitudeCount < 3 || peakIndices.empty())
        return;

    const auto binsToScan = juce::jmin(magnitudeCount,
                                       static_cast<int>(peakIndices.size()));

    float maximumMagnitude = 0.0f;
    for (int bin = 0; bin < binsToScan; ++bin)
        maximumMagnitude = juce::jmax(maximumMagnitude, magnitudes[bin]);

    const auto adaptiveThreshold = juce::jmax(minimumMagnitude,
                                               maximumMagnitude * relativeThreshold);

    for (int bin = 1; bin < binsToScan - 1; ++bin)
    {
        const auto centre = magnitudes[bin];

        if (centre < adaptiveThreshold)
            continue;

        const auto left = magnitudes[bin - 1];
        const auto right = magnitudes[bin + 1];

        if (centre > left && centre >= right)
        {
            peakIndices[static_cast<size_t>(peakCount)] = bin;
            peakMask[static_cast<size_t>(bin)] = 1;
            ++peakCount;
        }
    }
}

bool PeakDetector::isPeak(int bin) const noexcept
{
    return juce::isPositiveAndBelow(bin, static_cast<int>(peakMask.size()))
        && peakMask[static_cast<size_t>(bin)] != 0;
}
