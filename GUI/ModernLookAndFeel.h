#pragma once

#include <JuceHeader.h>

class ModernLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ModernLookAndFeel()
    {
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe8edf2));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff171b20));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::textColourId, juce::Colour(0xffd8dde3));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff272d35));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff5cb8ff));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd8dde3));
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101419));
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x),
                                              static_cast<float>(y),
                                              static_cast<float>(width),
                                              static_cast<float>(height))
                          .reduced(9.0f);

        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        g.setColour(juce::Colour(0xff0d1014));
        g.fillEllipse(bounds.translated(0.0f, 3.0f));

        g.setColour(juce::Colour(0xff242b33));
        g.fillEllipse(bounds);

        juce::Path track;
        track.addCentredArc(centre.x,
                            centre.y,
                            radius - 2.0f,
                            radius - 2.0f,
                            0.0f,
                            rotaryStartAngle,
                            rotaryEndAngle,
                            true);
        g.setColour(juce::Colour(0xff3a424c));
        g.strokePath(track, juce::PathStrokeType(3.0f));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x,
                               centre.y,
                               radius - 2.0f,
                               radius - 2.0f,
                               0.0f,
                               rotaryStartAngle,
                               angle,
                               true);
        g.setColour(juce::Colour(0xff5cb8ff));
        g.strokePath(valueArc, juce::PathStrokeType(3.2f));

        juce::Path pointer;
        const auto pointerLength = radius * 0.62f;
        const auto pointerThickness = juce::jmax(2.0f, radius * 0.07f);
        pointer.addRoundedRectangle(-pointerThickness * 0.5f,
                                    -pointerLength,
                                    pointerThickness,
                                    pointerLength * 0.58f,
                                    pointerThickness * 0.5f);
        pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xfff4f7fa));
        g.fillPath(pointer);
    }
};
