#pragma once

#include <JuceHeader.h>
#include "Theme.h"

class StereoMeter final : public juce::Component
{
public:
    void setLevels(float leftDb, float rightDb)
    {
        left = leftDb;
        right = rightDb;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        drawChannel(g, bounds.removeFromTop(bounds.getHeight() * 0.42f), left, "L");
        bounds.removeFromTop(4.0f);
        drawChannel(g, bounds, right, "R");
    }

private:
    static float normalise(float db)
    {
        return juce::jlimit(0.0f, 1.0f, juce::jmap(db, -60.0f, 3.0f, 0.0f, 1.0f));
    }

    void drawChannel(juce::Graphics& g, juce::Rectangle<float> area, float db, const juce::String& name)
    {
        auto label = area.removeFromLeft(14.0f);
        g.setColour(SoundShifterTheme::textMuted);
        g.setFont(9.0f);
        g.drawText(name, label, juce::Justification::centredLeft);

        auto track = area.reduced(1.0f, 2.0f);
        g.setColour(SoundShifterTheme::meterTrack);
        g.fillRoundedRectangle(track, 2.5f);

        auto fill = track;
        fill.setWidth(track.getWidth() * normalise(db));
        juce::ColourGradient gradient(SoundShifterTheme::accent, track.getX(), 0.0f,
                                      SoundShifterTheme::warning, track.getRight(), 0.0f, false);
        gradient.addColour(0.88, SoundShifterTheme::danger);
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(fill, 2.5f);

        g.setColour(SoundShifterTheme::outline.withAlpha(0.7f));
        g.drawRoundedRectangle(track, 2.5f, 0.8f);
    }

    float left = -100.0f;
    float right = -100.0f;
};
