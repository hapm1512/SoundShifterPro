#pragma once

#include <JuceHeader.h>

class FixedAudioBlock
{
public:
    void prepare(int channels, int samples)
    {
        buffer.setSize(juce::jmax(1, channels),
                       juce::jmax(1, samples),
                       false,
                       true,
                       false);
        buffer.clear();
    }

    void reset() noexcept { buffer.clear(); }
    juce::AudioBuffer<float>& get() noexcept { return buffer; }
    const juce::AudioBuffer<float>& get() const noexcept { return buffer; }

private:
    juce::AudioBuffer<float> buffer;
};
