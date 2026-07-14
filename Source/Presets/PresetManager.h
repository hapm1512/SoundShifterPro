#pragma once

#include <JuceHeader.h>
#include "FactoryPresets.h"

class PresetManager final
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& stateToManage);

    bool saveUserPreset(const juce::String& name);
    bool loadPreset(const juce::String& name);
    bool deleteUserPreset(const juce::String& name);
    bool renameUserPreset(const juce::String& oldName,
                          const juce::String& newName);

    [[nodiscard]] bool presetExists(const juce::String& name) const;
    [[nodiscard]] bool isFactoryPreset(const juce::String& name) const noexcept;
    [[nodiscard]] juce::StringArray getFactoryPresetNames() const;
    [[nodiscard]] juce::StringArray getUserPresetNames() const;
    [[nodiscard]] juce::StringArray getAllPresetNames() const;

    [[nodiscard]] juce::String getCurrentPresetName() const;
    [[nodiscard]] juce::File getPresetDirectory() const;

private:
    static juce::String sanitisePresetName(const juce::String& name);
    juce::File getPresetFile(const juce::String& name) const;
    bool loadFactoryPreset(const SoundShifterFactoryPresets::Preset& preset);
    bool loadUserPreset(const juce::File& file);
    void setParameter(const juce::String& parameterId, float plainValue);

    juce::AudioProcessorValueTreeState& apvts;
    juce::File presetDirectory;
    juce::String currentPresetName { "Default" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
