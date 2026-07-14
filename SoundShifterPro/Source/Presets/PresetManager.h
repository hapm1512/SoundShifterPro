#pragma once

#include <JuceHeader.h>
#include "FactoryPresets.h"

class PresetManager final
{
public:
    struct PresetMetadata
    {
        juce::String name;
        juce::String author { "Hai Pham" };
        juce::String category { "User" };
        juce::String description;
        juce::Time created;
        juce::Time modified;
        bool favourite = false;
    };

    explicit PresetManager(juce::AudioProcessorValueTreeState& stateToManage);

    bool saveUserPreset(const juce::String& name);
    bool saveUserPresetAs(const juce::String& oldName,
                          const juce::String& newName);
    bool loadPreset(const juce::String& name);
    bool deleteUserPreset(const juce::String& name);
    bool renameUserPreset(const juce::String& oldName,
                          const juce::String& newName);

    bool setFavourite(const juce::String& name, bool state);
    [[nodiscard]] bool isFavourite(const juce::String& name) const;
    [[nodiscard]] PresetMetadata getMetadata(const juce::String& name) const;
    [[nodiscard]] juce::StringArray getFavouritePresets() const;

    [[nodiscard]] bool presetExists(const juce::String& name) const;
    [[nodiscard]] bool isFactoryPreset(const juce::String& name) const noexcept;
    [[nodiscard]] juce::StringArray getFactoryPresetNames() const;
    [[nodiscard]] juce::StringArray getUserPresetNames() const;
    [[nodiscard]] juce::StringArray getAllPresetNames() const;
    [[nodiscard]] juce::String createUniquePresetName(const juce::String& baseName) const;

    void reloadPresetCache();
    void captureSnapshotA();
    void captureSnapshotB();
    bool restoreSnapshotA();
    bool restoreSnapshotB();

    [[nodiscard]] juce::String getCurrentPresetName() const;
    [[nodiscard]] juce::File getPresetDirectory() const;

private:
    static juce::String sanitisePresetName(const juce::String& name);
    juce::File getPresetFile(const juce::String& name) const;

    bool loadFactoryPreset(const SoundShifterFactoryPresets::Preset& preset);
    bool loadUserPreset(const juce::File& file);
    bool updateMetadataInFile(const juce::File& file,
                              const PresetMetadata& metadata) const;
    PresetMetadata readMetadataFromFile(const juce::File& file) const;
    PresetMetadata createDefaultMetadata(const juce::String& name,
                                         bool factory) const;
    void setParameter(const juce::String& parameterId, float plainValue);
    bool restoreSnapshot(const juce::ValueTree& snapshot);

    juce::AudioProcessorValueTreeState& apvts;
    juce::File presetDirectory;
    juce::String currentPresetName { "Default" };

    juce::StringArray cachedFactoryPresets;
    juce::StringArray cachedUserPresets;
    juce::HashMap<juce::String, PresetMetadata> metadataCache;

    juce::ValueTree snapshotA;
    juce::ValueTree snapshotB;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
