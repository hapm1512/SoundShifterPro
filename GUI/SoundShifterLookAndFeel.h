#pragma once

#include <JuceHeader.h>
#include "Theme.h"

class SoundShifterLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SoundShifterLookAndFeel()
    {
        setColour(juce::Label::textColourId, SoundShifterTheme::text);
        setColour(juce::Slider::textBoxTextColourId, SoundShifterTheme::text);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::TextButton::buttonColourId, SoundShifterTheme::panelRaised);
        setColour(juce::TextButton::buttonOnColourId, SoundShifterTheme::accent);
        setColour(juce::TextButton::textColourOffId, SoundShifterTheme::textMuted);
        setColour(juce::TextButton::textColourOnId, SoundShifterTheme::background);
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height))
                          .reduced(slider.getName() == "PITCH" ? 12.0f : 10.0f);

        const auto diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
        bounds = juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre());
        const auto centre = bounds.getCentre();
        const auto radius = diameter * 0.5f;
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.55f), 12, { 0, 5 });
        shadow.drawForRectangle(g, bounds.toNearestInt());

        juce::ColourGradient bodyGradient(juce::Colour(0xff343d47), bounds.getX(), bounds.getY(),
                                          juce::Colour(0xff10151a), bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill(bodyGradient);
        g.fillEllipse(bounds);

        g.setColour(SoundShifterTheme::outline.withAlpha(0.85f));
        g.drawEllipse(bounds, 1.2f);

        const auto arcRadius = radius - 4.5f;
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                            rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff39434e));
        g.strokePath(track, juce::PathStrokeType(slider.getName() == "PITCH" ? 5.0f : 3.6f,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                               rotaryStartAngle, angle, true);
        g.setColour(SoundShifterTheme::accent);
        g.strokePath(valueArc, juce::PathStrokeType(slider.getName() == "PITCH" ? 5.2f : 3.8f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        const auto cap = bounds.reduced(radius * 0.20f);
        g.setColour(juce::Colour(0xff202831));
        g.fillEllipse(cap);
        g.setColour(juce::Colour(0xff46515d));
        g.drawEllipse(cap, 1.0f);

        const auto pointerLength = radius * 0.53f;
        const auto pointerWidth = juce::jmax(2.0f, radius * 0.055f);
        juce::Path pointer;
        pointer.addRoundedRectangle(-pointerWidth * 0.5f, -pointerLength,
                                    pointerWidth, pointerLength * 0.42f,
                                    pointerWidth * 0.5f);
        pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
        g.setColour(SoundShifterTheme::text);
        g.fillPath(pointer);
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour&,
                              bool highlighted,
                              bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto colour = button.getToggleState() ? SoundShifterTheme::accent
                                              : SoundShifterTheme::panelRaised;

        if (highlighted)
            colour = colour.brighter(0.08f);
        if (down)
            colour = colour.darker(0.12f);

        g.setColour(colour);
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(button.getToggleState() ? SoundShifterTheme::accent.brighter(0.2f)
                                            : SoundShifterTheme::outline);
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool,
                        bool) override
    {
        g.setFont(SoundShifterTheme::labelFont(11.0f));
        g.setColour(button.getToggleState() ? SoundShifterTheme::background
                                            : SoundShifterTheme::textMuted);
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        return label.getFont();
    }
};
