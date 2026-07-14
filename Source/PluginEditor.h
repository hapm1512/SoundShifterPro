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
    bool keyPressed(const juce::KeyPress&) override;

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
    void updateSnapshotHistoryButtons();
    void configureTooltips();
    void updateCommercialUiState();

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

    juce::TextButton snapshotAButton { "A" };
    juce::TextButton snapshotBButton { "B" };
    juce::TextButton copyAToBButton { "A>B" };
    juce::TextButton copyBToAButton { "B>A" };
    juce::TextButton swapSnapshotsButton { "SWAP" };

    std::array<juce::TextButton, 8> historySnapshotButtons {
        juce::TextButton { "1" }, juce::TextButton { "2" },
        juce::TextButton { "3" }, juce::TextButton { "4" },
        juce::TextButton { "5" }, juce::TextButton { "6" },
        juce::TextButton { "7" }, juce::TextButton { "8" }
    };
    juce::TextButton saveHistoryButton { "SNAP SAVE" };
    juce::TextButton deleteHistoryButton { "SNAP DEL" };
    juce::TextButton clearHistoryButton { "CLEAR" };
    int selectedHistorySlot = 0;

    juce::TextButton undoButton { "UNDO" };
    juce::TextButton redoButton { "REDO" };
    juce::Label undoHistoryLabel;
    juce::TooltipWindow tooltipWindow { this, 450 };

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
    int uiTick = 0;

    float displayedInputLeftDb = -100.0f;
    float displayedInputRightDb = -100.0f;
    float displayedOutputLeftDb = -100.0f;
    float displayedOutputRightDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundShifterProAudioProcessorEditor)
};
