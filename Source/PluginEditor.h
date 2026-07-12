#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "../GUI/ModernLookAndFeel.h"

class SoundShifterProAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                   private juce::Timer
{
public:
    explicit SoundShifterProAudioProcessorEditor(SoundShifterProAudioProcessor&);
    ~SoundShifterProAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void configureKnob(juce::Slider& slider,
                       juce::Label& label,
                       const juce::String& labelText,
                       const juce::String& suffix);
    void drawMeter(juce::Graphics& g,
                   juce::Rectangle<float> area,
                   float levelDb,
                   const juce::String& title) const;

    SoundShifterProAudioProcessor& processor;
    ModernLookAndFeel lookAndFeel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;

    juce::Slider pitchSlider;
    juce::Slider fineSlider;
    juce::Slider mixSlider;
    juce::Slider outputSlider;

    juce::Label pitchLabel;
    juce::Label fineLabel;
    juce::Label mixLabel;
    juce::Label outputLabel;

    juce::TextButton bypassButton { "BYPASS" };

    std::unique_ptr<SliderAttachment> pitchAttachment;
    std::unique_ptr<SliderAttachment> fineAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    float displayedInputDb = -100.0f;
    float displayedOutputDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundShifterProAudioProcessorEditor)
};
