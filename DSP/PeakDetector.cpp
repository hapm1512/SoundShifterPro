#include "PeakDetector.h"

void PeakDetector::prepare(int newNumBins)
{
    const auto safeNumBins = juce::jmax(0, newNumBins);
    const auto size = static_cast<size_t>(safeNumBins);

    peakIndices.assign(size, 0);
    peakMask.assign(size, 0);
    previousPeakMask.assign(size, 0);
    peakConfidence.assign(size, 0.0f);
    previousConfidence.assign(size, 0.0f);
    peakLifetime.assign(size, 0);
    previousLifetime.assign(size, 0);

    reset();
}

void PeakDetector::reset() noexcept
{
    peakCount = 0;
    smoothedMaximumMagnitude = 0.0f;
    smoothedNoiseFloor = 0.0f;

    std::fill(peakMask.begin(), peakMask.end(), static_cast<uint8_t>(0));
    std::fill(previousPeakMask.begin(), previousPeakMask.end(), static_cast<uint8_t>(0));
    std::fill(peakConfidence.begin(), peakConfidence.end(), 0.0f);
    std::fill(previousConfidence.begin(), previousConfidence.end(), 0.0f);
    std::fill(peakLifetime.begin(), peakLifetime.end(), static_cast<uint16_t>(0));
    std::fill(previousLifetime.begin(), previousLifetime.end(), static_cast<uint16_t>(0));
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

    previousPeakMask.swap(peakMask);
    previousConfidence.swap(peakConfidence);
    previousLifetime.swap(peakLifetime);

    std::fill(peakMask.begin(), peakMask.end(), static_cast<uint8_t>(0));
    std::fill(peakConfidence.begin(), peakConfidence.end(), 0.0f);
    std::fill(peakLifetime.begin(), peakLifetime.end(), static_cast<uint16_t>(0));

    if (magnitudes == nullptr || magnitudeCount < 3 || peakIndices.empty())
        return;

    const auto binsToScan = juce::jmin(
        magnitudeCount,
        static_cast<int>(peakIndices.size()));

    float maximumMagnitude = 0.0f;
    double magnitudeSum = 0.0;

    for (int bin = 0; bin < binsToScan; ++bin)
    {
        const auto magnitude = juce::jmax(0.0f, magnitudes[bin]);
        maximumMagnitude = juce::jmax(maximumMagnitude, magnitude);
        magnitudeSum += static_cast<double>(magnitude);
    }

    const auto meanMagnitude = static_cast<float>(
        magnitudeSum / static_cast<double>(binsToScan));

    const auto maximumSmoothing =
        maximumMagnitude > smoothedMaximumMagnitude ? 0.48f : 0.10f;

    smoothedMaximumMagnitude += maximumSmoothing
                              * (maximumMagnitude - smoothedMaximumMagnitude);

    smoothedNoiseFloor += 0.07f * (meanMagnitude - smoothedNoiseFloor);

    const auto adaptiveThreshold = juce::jmax(
        minimumMagnitude,
        juce::jmax(smoothedMaximumMagnitude * relativeThreshold,
                   smoothedNoiseFloor * 1.75f));

    for (int bin = 1; bin < binsToScan - 1; ++bin)
    {
        const auto centre = magnitudes[bin];
        const auto left = magnitudes[bin - 1];
        const auto right = magnitudes[bin + 1];

        if (!(centre > left && centre >= right))
            continue;

        const auto trackedBin = findTrackedPeak(bin, binsToScan);
        const auto hasTrack = trackedBin >= 0;
        const auto trackedLifetime = hasTrack
            ? previousLifetime[static_cast<size_t>(trackedBin)]
            : static_cast<uint16_t>(0);

        const auto lifetimeSupport = juce::jlimit(
            0.0f,
            1.0f,
            static_cast<float>(trackedLifetime) / 10.0f);

        const auto requiredThreshold = adaptiveThreshold
            * (hasTrack ? juce::jmap(lifetimeSupport, 0.68f, 0.52f) : 1.0f);

        if (centre < requiredThreshold)
            continue;

        const auto prominence =
            calculateLocalProminence(magnitudes, bin, binsToScan);

        if (!hasTrack && prominence < 0.05f)
            continue;

        addPeak(bin,
                centre,
                juce::jmax(maximumMagnitude, minimumMagnitude),
                prominence,
                trackedBin);
    }
}

bool PeakDetector::isPeak(int bin) const noexcept
{
    return juce::isPositiveAndBelow(bin, static_cast<int>(peakMask.size()))
        && peakMask[static_cast<size_t>(bin)] != 0;
}

float PeakDetector::getPeakConfidence(int bin) const noexcept
{
    if (!juce::isPositiveAndBelow(bin, static_cast<int>(peakConfidence.size())))
        return 0.0f;

    return peakConfidence[static_cast<size_t>(bin)];
}

int PeakDetector::getPeakLifetime(int bin) const noexcept
{
    if (!juce::isPositiveAndBelow(bin, static_cast<int>(peakLifetime.size())))
        return 0;

    return static_cast<int>(peakLifetime[static_cast<size_t>(bin)]);
}

int PeakDetector::findTrackedPeak(int bin, int binsToScan) const noexcept
{
    constexpr int trackingRadius = 4;

    const auto firstBin = juce::jmax(1, bin - trackingRadius);
    const auto lastBin = juce::jmin(binsToScan - 2, bin + trackingRadius);

    auto bestBin = -1;
    auto bestScore = -1.0f;

    for (int candidate = firstBin; candidate <= lastBin; ++candidate)
    {
        if (previousPeakMask[static_cast<size_t>(candidate)] == 0)
            continue;

        const auto distance = static_cast<float>(std::abs(candidate - bin));
        const auto distanceWeight =
            1.0f - juce::jlimit(0.0f, 1.0f, distance / 5.0f);

        const auto lifetimeWeight = juce::jlimit(
            0.0f,
            1.0f,
            static_cast<float>(previousLifetime[static_cast<size_t>(candidate)]) / 12.0f);

        const auto score =
            previousConfidence[static_cast<size_t>(candidate)]
            * (0.72f * distanceWeight + 0.28f * lifetimeWeight);

        if (score > bestScore)
        {
            bestScore = score;
            bestBin = candidate;
        }
    }

    return bestBin;
}

float PeakDetector::calculateLocalProminence(const float* magnitudes,
                                             int bin,
                                             int binsToScan) const noexcept
{
    const auto left2 = magnitudes[juce::jmax(0, bin - 2)];
    const auto left1 = magnitudes[bin - 1];
    const auto centre = magnitudes[bin];
    const auto right1 = magnitudes[bin + 1];
    const auto right2 = magnitudes[juce::jmin(binsToScan - 1, bin + 2)];

    const auto shoulder =
        0.35f * (left2 + right2)
        + 0.65f * (left1 + right1);

    return juce::jlimit(
        0.0f,
        1.0f,
        (centre - 0.5f * shoulder)
            / (centre + minimumMagnitude));
}

void PeakDetector::addPeak(int bin,
                           float magnitude,
                           float maximumMagnitude,
                           float prominence,
                           int trackedBin) noexcept
{
    if (!juce::isPositiveAndBelow(bin, static_cast<int>(peakMask.size())))
        return;

    const auto normalisedMagnitude = juce::jlimit(
        0.0f,
        1.0f,
        magnitude / juce::jmax(maximumMagnitude, minimumMagnitude));

    auto confidence = juce::jlimit(
        0.0f,
        1.0f,
        0.62f * normalisedMagnitude + 0.38f * prominence);

    uint16_t lifetime = 1;

    if (trackedBin >= 0)
    {
        const auto previous =
            previousConfidence[static_cast<size_t>(trackedBin)];

        confidence = juce::jlimit(
            0.0f,
            1.0f,
            0.76f * previous + 0.24f * confidence);

        lifetime = static_cast<uint16_t>(juce::jmin(
            65535,
            static_cast<int>(
                previousLifetime[static_cast<size_t>(trackedBin)]) + 1));
    }

    constexpr int minimumPeakSpacing = 2;

    if (peakCount > 0)
    {
        const auto previousIndex =
            peakIndices[static_cast<size_t>(peakCount - 1)];

        if (bin - previousIndex < minimumPeakSpacing)
        {
            const auto previousConfidenceValue =
                peakConfidence[static_cast<size_t>(previousIndex)];

            const auto previousLifetimeValue =
                peakLifetime[static_cast<size_t>(previousIndex)];

            const auto replacePrevious =
                confidence > previousConfidenceValue + 0.03f
                || (std::abs(confidence - previousConfidenceValue) <= 0.03f
                    && lifetime > previousLifetimeValue);

            if (!replacePrevious)
                return;

            peakMask[static_cast<size_t>(previousIndex)] = 0;
            peakConfidence[static_cast<size_t>(previousIndex)] = 0.0f;
            peakLifetime[static_cast<size_t>(previousIndex)] = 0;
            --peakCount;
        }
    }

    peakIndices[static_cast<size_t>(peakCount)] = bin;
    peakMask[static_cast<size_t>(bin)] = 1;
    peakConfidence[static_cast<size_t>(bin)] = confidence;
    peakLifetime[static_cast<size_t>(bin)] = lifetime;
    ++peakCount;
}
