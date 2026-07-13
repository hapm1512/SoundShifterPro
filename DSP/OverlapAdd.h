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

        auto* const destination =
            accumulation.getWritePointer(channel);

        constexpr auto frameSize =
            SoundShifterDSP::Config::fftSize;

        const auto currentRead = readPosition;
        const auto firstBlock =
            frameSize - currentRead;

        juce::FloatVectorOperations::add(
            destination + currentRead,
            frame,
            firstBlock);

        if (readPosition > 0)
        {
            juce::FloatVectorOperations::add(
                destination,
                frame + firstBlock,
                currentRead);
        }
    }

    float popSample(int channel) noexcept
    {
        jassert(juce::isPositiveAndBelow(
            channel,
            accumulation.getNumChannels()));

        auto* const samples =
            accumulation.getWritePointer(channel);

        const auto currentRead = readPosition;
        const auto value = samples[currentRead];
        samples[currentRead] = 0.0f;
        return value;
    }

    void advance() noexcept
    {
        readPosition += 1;
        readPosition -= (readPosition == SoundShifterDSP::Config::fftSize)
                            ? SoundShifterDSP::Config::fftSize
                            : 0;
    }

private:
    juce::AudioBuffer<float> accumulation;
    int readPosition = 0;
};
