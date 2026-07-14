#pragma once

#include <JuceHeader.h>

namespace SoundShifterFactoryPresets
{
    struct Preset
    {
        juce::String name;
        juce::String category;
        float pitch = 0.0f;
        float fine = 0.0f;
        float mix = 100.0f;
        float output = 0.0f;
        bool highQuality = true;
        bool bypass = false;
    };

    const std::vector<Preset>& getPresets();
    const Preset* findByName(const juce::String& name) noexcept;
}
