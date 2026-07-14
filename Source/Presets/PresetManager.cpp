#include "PresetManager.h"

namespace
{
    constexpr auto presetExtension = ".sspreset";
    constexpr auto presetRootTag = "SoundShifterProPreset";
    constexpr int presetVersion = 2;
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
    auto metadata = createDefaultMetadata(safeName, false);

    const auto destination = getPresetFile(safeName);
    if (destination.existsAsFile())
    {
        const auto oldMetadata = readMetadataFromFile(destination);
        metadata.created = oldMetadata.created;
        metadata.favourite = oldMetadata.favourite;
        metadata.description = oldMetadata.description;
    }

    state.setProperty("presetName", safeName, nullptr);
    state.setProperty("presetVersion", presetVersion, nullptr);
    state.setProperty("author", metadata.author, nullptr);
    state.setProperty("category", metadata.category, nullptr);
    state.setProperty("description", metadata.description, nullptr);
    state.setProperty("created", metadata.created.toMilliseconds(), nullptr);
    state.setProperty("modified", metadata.modified.toMilliseconds(), nullptr);
    state.setProperty("favourite", metadata.favourite, nullptr);

    auto xml = state.createXml();
    if (xml == nullptr)
        return false;

    xml->setTagName(presetRootTag);

    const auto saved = xml->writeTo(destination);

    if (saved)
        currentPresetName = safeName;

    return saved;
}

bool PresetManager::saveUserPresetAs(const juce::String& oldName,
                                     const juce::String& newName)
{
    juce::ignoreUnused(oldName);

    const auto safeNewName = sanitisePresetName(newName);
    if (safeNewName.isEmpty() || presetExists(safeNewName))
        return false;

    return saveUserPreset(safeNewName);
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
    if (!renamed)
        return false;

    auto metadata = readMetadataFromFile(destination);
    metadata.name = safeNewName;
    metadata.modified = juce::Time::getCurrentTime();

    if (!updateMetadataInFile(destination, metadata))
    {
        destination.moveFileTo(source);
        return false;
    }

    if (currentPresetName.equalsIgnoreCase(oldName))
        currentPresetName = safeNewName;

    return true;
}

bool PresetManager::setFavourite(const juce::String& name, bool state)
{
    if (isFactoryPreset(name))
        return false;

    const auto file = getPresetFile(name);
    if (!file.existsAsFile())
        return false;

    auto metadata = readMetadataFromFile(file);
    metadata.favourite = state;
    metadata.modified = juce::Time::getCurrentTime();

    return updateMetadataInFile(file, metadata);
}

bool PresetManager::isFavourite(const juce::String& name) const
{
    return getMetadata(name).favourite;
}

PresetManager::PresetMetadata
PresetManager::getMetadata(const juce::String& name) const
{
    if (isFactoryPreset(name))
        return createDefaultMetadata(name, true);

    const auto file = getPresetFile(name);
    if (!file.existsAsFile())
        return {};

    return readMetadataFromFile(file);
}

juce::StringArray PresetManager::getFavouritePresets() const
{
    juce::StringArray favourites;

    for (const auto& name : getUserPresetNames())
    {
        if (isFavourite(name))
            favourites.add(name);
    }

    favourites.sortNatural();
    return favourites;
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

    names.sortNatural();
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

    for (const auto* property : {
             "presetName", "presetVersion", "author", "category",
             "description", "created", "modified", "favourite" })
    {
        state.removeProperty(property, nullptr);
    }

    if (state.getType() != apvts.state.getType())
        return false;

    apvts.replaceState(state);
    currentPresetName = presetName.isNotEmpty()
        ? presetName
        : file.getFileNameWithoutExtension();

    return true;
}

bool PresetManager::updateMetadataInFile(
    const juce::File& file,
    const PresetMetadata& metadata) const
{
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName(presetRootTag))
        return false;

    auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid())
        return false;

    state.setProperty("presetName", metadata.name, nullptr);
    state.setProperty("presetVersion", presetVersion, nullptr);
    state.setProperty("author", metadata.author, nullptr);
    state.setProperty("category", metadata.category, nullptr);
    state.setProperty("description", metadata.description, nullptr);
    state.setProperty("created", metadata.created.toMilliseconds(), nullptr);
    state.setProperty("modified", metadata.modified.toMilliseconds(), nullptr);
    state.setProperty("favourite", metadata.favourite, nullptr);

    auto updatedXml = state.createXml();
    if (updatedXml == nullptr)
        return false;

    updatedXml->setTagName(presetRootTag);
    return updatedXml->writeTo(file);
}

PresetManager::PresetMetadata
PresetManager::readMetadataFromFile(const juce::File& file) const
{
    auto metadata = createDefaultMetadata(
        file.getFileNameWithoutExtension(), false);

    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName(presetRootTag))
        return metadata;

    const auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid())
        return metadata;

    metadata.name = state.getProperty("presetName", metadata.name).toString();
    metadata.author = state.getProperty("author", metadata.author).toString();
    metadata.category = state.getProperty("category", metadata.category).toString();
    metadata.description = state.getProperty("description").toString();

    const auto createdMs = static_cast<juce::int64>(
        state.getProperty("created", metadata.created.toMilliseconds()));
    const auto modifiedMs = static_cast<juce::int64>(
        state.getProperty("modified", metadata.modified.toMilliseconds()));

    metadata.created = juce::Time(createdMs);
    metadata.modified = juce::Time(modifiedMs);
    metadata.favourite = static_cast<bool>(state.getProperty("favourite", false));

    return metadata;
}

PresetManager::PresetMetadata
PresetManager::createDefaultMetadata(const juce::String& name,
                                     bool factory) const
{
    PresetMetadata metadata;
    metadata.name = name;
    metadata.author = "Hai Pham";
    metadata.category = factory ? "Factory" : "User";
    metadata.created = juce::Time::getCurrentTime();
    metadata.modified = metadata.created;
    metadata.favourite = false;
    return metadata;
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
