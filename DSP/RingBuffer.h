#pragma once

#include <JuceHeader.h>

class RingBuffer
{
public:
    void prepare(int requestedCapacity)
    {
        capacity = juce::jmax(1, requestedCapacity);
        data.assign(static_cast<size_t>(capacity), 0.0f);
        reset();
    }

    void reset() noexcept
    {
        juce::FloatVectorOperations::clear(
            data.data(),
            capacity);

        writeIndex = 0;
        validSamples = 0;
    }

    void push(float sample) noexcept
    {
        jassert(capacity > 0);

        data[static_cast<size_t>(writeIndex)] = sample;

        ++writeIndex;

        if (writeIndex == capacity)
            writeIndex = 0;

        if (validSamples < capacity)
            ++validSamples;
    }

    [[nodiscard]] bool isFull() const noexcept
    {
        return validSamples == capacity;
    }

    void copyOldestToNewest(float* destination,
                            int numberOfSamples) const noexcept
    {
        jassert(destination != nullptr);
        jassert(numberOfSamples <= validSamples);
        jassert(numberOfSamples <= capacity);

        auto start = writeIndex - numberOfSamples;

        if (start < 0)
            start += capacity;

        const auto firstBlock =
            juce::jmin(numberOfSamples, capacity - start);

        juce::FloatVectorOperations::copy(
            destination,
            data.data() + start,
            firstBlock);

        const auto remaining =
            numberOfSamples - firstBlock;

        if (remaining > 0)
        {
            juce::FloatVectorOperations::copy(
                destination + firstBlock,
                data.data(),
                remaining);
        }
    }

private:
    std::vector<float> data;
    int capacity = 0;
    int writeIndex = 0;
    int validSamples = 0;
};
