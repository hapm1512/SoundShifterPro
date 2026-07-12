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
        std::fill(data.begin(), data.end(), 0.0f);
        writeIndex = 0;
        validSamples = 0;
    }

    void push(float sample) noexcept
    {
        jassert(capacity > 0);
        data[static_cast<size_t>(writeIndex)] = sample;
        writeIndex = (writeIndex + 1) % capacity;
        validSamples = juce::jmin(validSamples + 1, capacity);
    }

    [[nodiscard]] bool isFull() const noexcept
    {
        return validSamples == capacity;
    }

    void copyOldestToNewest(float* destination, int numberOfSamples) const noexcept
    {
        jassert(destination != nullptr);
        jassert(numberOfSamples <= validSamples);
        jassert(numberOfSamples <= capacity);

        const auto start = (writeIndex - numberOfSamples + capacity) % capacity;
        const auto firstBlock = juce::jmin(numberOfSamples, capacity - start);

        juce::FloatVectorOperations::copy(destination,
                                          data.data() + start,
                                          firstBlock);

        const auto remaining = numberOfSamples - firstBlock;
        if (remaining > 0)
            juce::FloatVectorOperations::copy(destination + firstBlock,
                                              data.data(),
                                              remaining);
    }

private:
    std::vector<float> data;
    int capacity = 0;
    int writeIndex = 0;
    int validSamples = 0;
};
