#pragma once

#include <JuceHeader.h>
#include "DSPConfig.h"

class OverlapAdd
{
public:
    void prepare(int channels)
    {
        const auto safeChannels = juce::jlimit(1, SoundShifterDSP::Config::maxChannels, channels);
        accumulation.setSize(safeChannels,
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
        jassert(juce::isPositiveAndBelow(channel, accumulation.getNumChannels()));
        jassert(frame != nullptr);

        auto* destination = accumulation.getWritePointer(channel);
        for (int i = 0; i < SoundShifterDSP::Config::fftSize; ++i)
        {
            const auto index = (readPosition + i) % SoundShifterDSP::Config::fftSize;
            destination[index] += frame[i];
        }
    }

    float popSample(int channel) noexcept
    {
        jassert(juce::isPositiveAndBelow(channel, accumulation.getNumChannels()));
        auto* samples = accumulation.getWritePointer(channel);
        const auto value = samples[readPosition];
        samples[readPosition] = 0.0f;
        return value;
    }

    void advance() noexcept
    {
        readPosition = (readPosition + 1) % SoundShifterDSP::Config::fftSize;
    }

private:
    juce::AudioBuffer<float> accumulation;
    int readPosition = 0;
};
