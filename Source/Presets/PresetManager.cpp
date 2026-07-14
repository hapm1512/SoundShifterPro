#include "PresetManager.h"

namespace
{
    constexpr auto presetExtension = ".sspreset";
    constexpr auto presetRootTag = "SoundShifterProPreset";
}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& stateToManage)
    : apvts(stateToManage),
      presetDirectory(
          juce::File::getSpecialLocation(
              juce::File::userApplicationDataDirectory)
              .getChildFile("Hai Pham Audio")
              .getChildFile("SoundShifter Pro")
              .getChildFile("Presets"))
{
    presetDirectory.createDirectory();
}

bool PresetManager::saveUserPreset(const juce::String& name)
{
    const auto safeName = sanitisePresetName(name);

    if (safeName.isEmpty() || isFactoryPreset(safeName))
        return false;

    auto state = apvts.copyState();
    state.setProperty("presetName", safeName, nullptr);
    state.setProperty("presetVersion", 1, nullptr);

    auto xml = state.createXml();
    if (xml == nullptr)
        return false;

    xml->setTagName(presetRootTag);

    const auto file = getPresetFile(safeName);
    const auto saved = xml->writeTo(file);

    if (saved)
        currentPresetName = safeName;

    return saved;
}

bool PresetManager::loadPreset(const juce::String& name)
{
    if (const auto* factoryPreset =
            SoundShifterFactoryPresets::findByName(name))
    {
        return loadFactoryPreset(*factoryPreset);
    }

    return loadUserPreset(getPresetFile(name));
}

bool PresetManager::deleteUserPreset(const juce::String& name)
{
    if (isFactoryPreset(name))
        return false;

    const auto file = getPresetFile(name);
    if (!file.existsAsFile())
        return false;

    const auto deleted = file.deleteFile();

    if (deleted && currentPresetName.equalsIgnoreCase(name))
        currentPresetName = "Default";

    return deleted;
}

bool PresetManager::renameUserPreset(const juce::String& oldName,
                                     const juce::String& newName)
{
    if (isFactoryPreset(oldName) || isFactoryPreset(newName))
        return false;

    const auto safeNewName = sanitisePresetName(newName);
    if (safeNewName.isEmpty())
        return false;

    const auto source = getPresetFile(oldName);
    const auto destination = getPresetFile(safeNewName);

    if (!source.existsAsFile() || destination.existsAsFile())
        return false;

    const auto renamed = source.moveFileTo(destination);

    if (renamed && currentPresetName.equalsIgnoreCase(oldName))
        currentPresetName = safeNewName;

    return renamed;
}

bool PresetManager::presetExists(const juce::String& name) const
{
    return isFactoryPreset(name) || getPresetFile(name).existsAsFile();
}

bool PresetManager::isFactoryPreset(const juce::String& name) const noexcept
{
    return SoundShifterFactoryPresets::findByName(name) != nullptr;
}

juce::StringArray PresetManager::getFactoryPresetNames() const
{
    juce::StringArray names;

    for (const auto& preset : SoundShifterFactoryPresets::getPresets())
        names.add(preset.name);

    return names;
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;

    juce::Array<juce::File> files;
    presetDirectory.findChildFiles(
        files,
        juce::File::findFiles,
        false,
        "*" + juce::String(presetExtension));

    for (const auto& file : files)
        names.add(file.getFileNameWithoutExtension());

    names.sortNatural();
    return names;
}

juce::StringArray PresetManager::getAllPresetNames() const
{
    auto names = getFactoryPresetNames();
    names.addArray(getUserPresetNames());
    return names;
}

juce::String PresetManager::getCurrentPresetName() const
{
    return currentPresetName;
}

juce::File PresetManager::getPresetDirectory() const
{
    return presetDirectory;
}

juce::String PresetManager::sanitisePresetName(const juce::String& name)
{
    auto safeName = name.trim();

    for (const auto character : juce::String("\\/:*?\"<>|"))
        safeName = safeName.replaceCharacter(character, '_');

    return safeName.substring(0, 64).trim();
}

juce::File PresetManager::getPresetFile(const juce::String& name) const
{
    return presetDirectory.getChildFile(
        sanitisePresetName(name) + presetExtension);
}

bool PresetManager::loadFactoryPreset(
    const SoundShifterFactoryPresets::Preset& preset)
{
    setParameter("pitch", preset.pitch);
    setParameter("fine", preset.fine);
    setParameter("mix", preset.mix);
    setParameter("output", preset.output);
    setParameter("hq", preset.highQuality ? 1.0f : 0.0f);
    setParameter("bypass", preset.bypass ? 1.0f : 0.0f);

    currentPresetName = preset.name;
    return true;
}

bool PresetManager::loadUserPreset(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName(presetRootTag))
        return false;

    auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid())
        return false;

    const auto presetName = state.getProperty("presetName").toString();
    state.removeProperty("presetName", nullptr);
    state.removeProperty("presetVersion", nullptr);

    if (state.getType() != apvts.state.getType())
        return false;

    apvts.replaceState(state);
    currentPresetName = presetName.isNotEmpty()
        ? presetName
        : file.getFileNameWithoutExtension();

    return true;
}

void PresetManager::setParameter(const juce::String& parameterId,
                                 float plainValue)
{
    if (auto* parameter = apvts.getParameter(parameterId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(
            parameter->convertTo0to1(plainValue));
        parameter->endChangeGesture();
    }
}
