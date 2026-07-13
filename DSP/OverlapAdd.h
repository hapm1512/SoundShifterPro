#pragma once

#include <JuceHeader.h>
#include "DSPConfig.h"

class OverlapAdd
{
public:
    void prepare(int channels)
    {
        const auto safeChannels = juce::jlimit(
            1,
            SoundShifterDSP::Config::maxChannels,
            channels);

        accumulation.setSize(
            safeChannels,
            SoundShifterDSP::Config::fftSize,
            false,
            true,
            false);

        reset();
    }

    void reset() noexcept
    {
        accumulation.clear();
        readPosition = 0;
    }

    void addFrame(int channel, const float* frame) noexcept
    {
        jassert(juce::isPositiveAndBelow(
            channel,
            accumulation.getNumChannels()));
        jassert(frame != nullptr);

        auto* destination =
            accumulation.getWritePointer(channel);

        constexpr auto frameSize =
            SoundShifterDSP::Config::fftSize;

        const auto firstBlock =
            frameSize - readPosition;

        juce::FloatVectorOperations::add(
            destination + readPosition,
            frame,
            firstBlock);

        if (readPosition > 0)
        {
            juce::FloatVectorOperations::add(
                destination,
                frame + firstBlock,
                readPosition);
        }
    }

    float popSample(int channel) noexcept
    {
        jassert(juce::isPositiveAndBelow(
            channel,
            accumulation.getNumChannels()));

        auto* samples =
            accumulation.getWritePointer(channel);

        const auto value = samples[readPosition];
        samples[readPosition] = 0.0f;
        return value;
    }

    void advance() noexcept
    {
        ++readPosition;

        if (readPosition == SoundShifterDSP::Config::fftSize)
            readPosition = 0;
    }

private:
    juce::AudioBuffer<float> accumulation;
    int readPosition = 0;
};
