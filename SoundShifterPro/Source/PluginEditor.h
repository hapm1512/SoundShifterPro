#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "../GUI/SoundShifterLookAndFeel.h"
#include "../GUI/StereoMeter.h"

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
    void configureKnob(juce::Slider&, juce::Label&, const juce::String&, const juce::String&, bool);
    void drawPanel(juce::Graphics&, juce::Rectangle<float>, float) const;
    void refreshPresetList();
    void saveNextUserPreset();
    void deleteSelectedUserPreset();
    void toggleSelectedFavourite();

    SoundShifterProAudioProcessor& processor;
    SoundShifterLookAndFeel lookAndFeel;

    juce::Label brandLabel;
    juce::Label productLabel;
    juce::Label pitchCaption;
    juce::Label fineCaption;
    juce::Label mixCaption;
    juce::Label outputCaption;
    juce::Label inputCaption;
    juce::Label outputMeterCaption;
    juce::Label latencyLabel;
    juce::Label versionLabel;
    juce::Label engineLabel;

    juce::Slider pitchSlider;
    juce::Slider fineSlider;
    juce::Slider mixSlider;
    juce::Slider outputSlider;

    juce::TextButton hqButton { "HQ" };
    juce::TextButton bypassButton { "BYPASS" };

    juce::ComboBox presetBox;
    juce::TextButton savePresetButton { "SAVE" };
    juce::TextButton deletePresetButton { "DELETE" };
    juce::TextButton favouritePresetButton { "STAR" };

    juce::TextButton learnDownButton { "LEARN ▼" };
    juce::TextButton learnUpButton { "LEARN ▲" };
    juce::TextButton learnResetButton { "LEARN R" };

    StereoMeter inputMeter;
    StereoMeter outputMeter;

    std::unique_ptr<SliderAttachment> pitchAttachment;
    std::unique_ptr<SliderAttachment> fineAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<ButtonAttachment> hqAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    bool refreshingPresetList = false;

    float displayedInputLeftDb = -100.0f;
    float displayedInputRightDb = -100.0f;
    float displayedOutputLeftDb = -100.0f;
    float displayedOutputRightDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundShifterProAudioProcessorEditor)
};
