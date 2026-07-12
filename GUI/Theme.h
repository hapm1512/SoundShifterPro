#pragma once

#include <JuceHeader.h>

namespace SoundShifterTheme
{
    inline const juce::Colour background { 0xff0b0e12 };
    inline const juce::Colour panel { 0xff14191f };
    inline const juce::Colour panelRaised { 0xff1b222a };
    inline const juce::Colour outline { 0xff303946 };
    inline const juce::Colour text { 0xffeef3f7 };
    inline const juce::Colour textMuted { 0xff84909d };
    inline const juce::Colour accent { 0xff33d2c3 };
    inline const juce::Colour accentDim { 0xff167b76 };
    inline const juce::Colour warning { 0xffffb14a };
    inline const juce::Colour danger { 0xffff5964 };
    inline const juce::Colour meterTrack { 0xff080a0d };

    inline juce::Font titleFont(float size)
    {
        return juce::Font(juce::FontOptions(size, juce::Font::bold));
    }

    inline juce::Font labelFont(float size)
    {
        return juce::Font(juce::FontOptions(size, juce::Font::bold));
    }
}
