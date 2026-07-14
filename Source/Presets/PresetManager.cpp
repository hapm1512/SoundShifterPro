#include "PresetManager.h"

namespace
{
    constexpr auto presetExtension = ".sspreset";
    constexpr auto presetRootTag = "SoundShifterProPreset";
    constexpr int presetVersion = 3;
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
    reloadPresetCache();
    captureSnapshotA();
    captureSnapshotB();
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
        metadata.author = oldMetadata.author;
        metadata.category = oldMetadata.category;
    }

    metadata.name = safeName;
    metadata.modified = juce::Time::getCurrentTime();

    state.setProperty("presetName", metadata.name, nullptr);
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
    {
        currentPresetName = safeName;
        reloadPresetCache();
    }

    return saved;
}

bool PresetManager::saveUserPresetAs(const juce::String& oldName,
                                     const juce::String& newName)
{
    juce::ignoreUnused(oldName);
    const auto uniqueName = createUniquePresetName(newName);
    return uniqueName.isNotEmpty() && saveUserPreset(uniqueName);
}

bool PresetManager::loadPreset(const juce::String& name)
{
    if (const auto* factoryPreset = SoundShifterFactoryPresets::findByName(name))
        return loadFactoryPreset(*factoryPreset);

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
    if (deleted)
    {
        if (currentPresetName.equalsIgnoreCase(name))
            currentPresetName = "Default";
        reloadPresetCache();
    }

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

    if (!source.moveFileTo(destination))
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

    reloadPresetCache();
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

    const auto updated = updateMetadataInFile(file, metadata);
    if (updated)
        reloadPresetCache();
    return updated;
}

bool PresetManager::isFavourite(const juce::String& name) const
{
    return metadataCache.contains(name) && metadataCache[name].favourite;
}

PresetManager::PresetMetadata PresetManager::getMetadata(const juce::String& name) const
{
    return metadataCache.contains(name) ? metadataCache[name] : PresetMetadata {};
}

juce::StringArray PresetManager::getFavouritePresets() const
{
    juce::StringArray favourites;
    for (const auto& name : cachedUserPresets)
        if (isFavourite(name))
            favourites.add(name);
    favourites.sortNatural();
    return favourites;
}

bool PresetManager::presetExists(const juce::String& name) const
{
    return cachedFactoryPresets.contains(name, true)
        || cachedUserPresets.contains(name, true)
        || getPresetFile(name).existsAsFile();
}

bool PresetManager::isFactoryPreset(const juce::String& name) const noexcept
{
    return SoundShifterFactoryPresets::findByName(name) != nullptr;
}

juce::StringArray PresetManager::getFactoryPresetNames() const
{
    return cachedFactoryPresets;
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    return cachedUserPresets;
}

juce::StringArray PresetManager::getAllPresetNames() const
{
    auto names = getFavouritePresets();
    for (const auto& name : cachedFactoryPresets)
        if (!names.contains(name, true))
            names.add(name);
    for (const auto& name : cachedUserPresets)
        if (!names.contains(name, true))
            names.add(name);
    return names;
}

juce::String PresetManager::createUniquePresetName(const juce::String& baseName) const
{
    const auto safeBase = sanitisePresetName(baseName);
    if (safeBase.isEmpty())
        return {};
    if (!presetExists(safeBase))
        return safeBase;

    for (int index = 1; index < 10000; ++index)
    {
        const auto candidate = safeBase + " (" + juce::String(index) + ")";
        if (!presetExists(candidate))
            return candidate;
    }
    return {};
}

void PresetManager::reloadPresetCache()
{
    cachedFactoryPresets.clear();
    cachedUserPresets.clear();
    metadataCache.clear();

    for (const auto& preset : SoundShifterFactoryPresets::getPresets())
    {
        cachedFactoryPresets.add(preset.name);
        auto metadata = createDefaultMetadata(preset.name, true);
        metadata.category = preset.category;
        metadata.author = preset.author;
        metadata.description = preset.description;
        metadataCache.set(preset.name, metadata);
    }
    cachedFactoryPresets.sortNatural();

    juce::Array<juce::File> files;
    presetDirectory.findChildFiles(files, juce::File::findFiles, false,
                                   "*" + juce::String(presetExtension));
    for (const auto& file : files)
    {
        const auto name = file.getFileNameWithoutExtension();
        cachedUserPresets.add(name);
        metadataCache.set(name, readMetadataFromFile(file));
    }
    cachedUserPresets.sortNatural();
}

void PresetManager::captureSnapshotA()
{
    snapshotA = apvts.copyState();
}

void PresetManager::captureSnapshotB()
{
    snapshotB = apvts.copyState();
}

bool PresetManager::selectSnapshotA()
{
    return selectSnapshot(SnapshotSlot::A);
}

bool PresetManager::selectSnapshotB()
{
    return selectSnapshot(SnapshotSlot::B);
}

void PresetManager::copySnapshotAToB()
{
    if (activeSnapshot == SnapshotSlot::A)
        captureSnapshotA();

    snapshotB = snapshotA.createCopy();

    if (activeSnapshot == SnapshotSlot::B)
        restoreSnapshot(snapshotB);
}

void PresetManager::copySnapshotBToA()
{
    if (activeSnapshot == SnapshotSlot::B)
        captureSnapshotB();

    snapshotA = snapshotB.createCopy();

    if (activeSnapshot == SnapshotSlot::A)
        restoreSnapshot(snapshotA);
}

bool PresetManager::swapSnapshots()
{
    if (activeSnapshot == SnapshotSlot::A)
        captureSnapshotA();
    else
        captureSnapshotB();

    std::swap(snapshotA, snapshotB);
    activeSnapshot = activeSnapshot == SnapshotSlot::A
        ? SnapshotSlot::B
        : SnapshotSlot::A;

    return restoreSnapshot(activeSnapshot == SnapshotSlot::A
                               ? snapshotA
                               : snapshotB);
}

PresetManager::SnapshotSlot PresetManager::getActiveSnapshot() const noexcept
{
    return activeSnapshot;
}

bool PresetManager::captureHistorySnapshot(int slot, const juce::String& name)
{
    if (!juce::isPositiveAndBelow(slot, snapshotHistorySize))
        return false;

    historySnapshots[static_cast<size_t>(slot)] = apvts.copyState();
    historySnapshotNames[static_cast<size_t>(slot)] =
        name.isNotEmpty() ? name : "Snapshot " + juce::String(slot + 1);
    historySnapshotTimes[static_cast<size_t>(slot)] = juce::Time::getCurrentTime();
    activeHistorySnapshot = slot;
    return true;
}

bool PresetManager::recallHistorySnapshot(int slot)
{
    if (!hasHistorySnapshot(slot))
        return false;

    if (!restoreSnapshot(historySnapshots[static_cast<size_t>(slot)]))
        return false;

    activeHistorySnapshot = slot;
    return true;
}

bool PresetManager::renameHistorySnapshot(int slot, const juce::String& name)
{
    if (!hasHistorySnapshot(slot) || name.trim().isEmpty())
        return false;

    historySnapshotNames[static_cast<size_t>(slot)] = name.trim().substring(0, 48);
    return true;
}

bool PresetManager::deleteHistorySnapshot(int slot)
{
    if (!juce::isPositiveAndBelow(slot, snapshotHistorySize))
        return false;

    const auto index = static_cast<size_t>(slot);
    historySnapshots[index] = {};
    historySnapshotNames[index].clear();
    historySnapshotTimes[index] = {};
    if (activeHistorySnapshot == slot)
        activeHistorySnapshot = -1;
    return true;
}

void PresetManager::clearHistorySnapshots()
{
    for (int slot = 0; slot < snapshotHistorySize; ++slot)
        deleteHistorySnapshot(slot);
}

bool PresetManager::hasHistorySnapshot(int slot) const noexcept
{
    return juce::isPositiveAndBelow(slot, snapshotHistorySize)
        && historySnapshots[static_cast<size_t>(slot)].isValid();
}

juce::String PresetManager::getHistorySnapshotName(int slot) const
{
    return hasHistorySnapshot(slot)
        ? historySnapshotNames[static_cast<size_t>(slot)]
        : juce::String("Snapshot ") + juce::String(slot + 1);
}

juce::Time PresetManager::getHistorySnapshotTime(int slot) const
{
    return hasHistorySnapshot(slot)
        ? historySnapshotTimes[static_cast<size_t>(slot)]
        : juce::Time();
}

int PresetManager::getActiveHistorySnapshot() const noexcept
{
    return activeHistorySnapshot;
}

juce::String PresetManager::getCurrentPresetName() const { return currentPresetName; }
juce::File PresetManager::getPresetDirectory() const { return presetDirectory; }

juce::String PresetManager::sanitisePresetName(const juce::String& name)
{
    auto safeName = name.trim();
    for (const auto character : juce::String("\\/:*?\"<>|"))
        safeName = safeName.replaceCharacter(character, '_');
    return safeName.substring(0, 64).trim();
}

juce::File PresetManager::getPresetFile(const juce::String& name) const
{
    return presetDirectory.getChildFile(sanitisePresetName(name) + presetExtension);
}

bool PresetManager::loadFactoryPreset(const SoundShifterFactoryPresets::Preset& preset)
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
    for (const auto* property : { "presetName", "presetVersion", "author", "category",
                                  "description", "created", "modified", "favourite" })
        state.removeProperty(property, nullptr);

    if (state.getType() != apvts.state.getType())
        return false;

    apvts.replaceState(state);
    currentPresetName = presetName.isNotEmpty() ? presetName : file.getFileNameWithoutExtension();
    return true;
}

bool PresetManager::updateMetadataInFile(const juce::File& file,
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

PresetManager::PresetMetadata PresetManager::readMetadataFromFile(const juce::File& file) const
{
    auto metadata = createDefaultMetadata(file.getFileNameWithoutExtension(), false);
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
    metadata.created = juce::Time(static_cast<juce::int64>(
        state.getProperty("created", metadata.created.toMilliseconds())));
    metadata.modified = juce::Time(static_cast<juce::int64>(
        state.getProperty("modified", metadata.modified.toMilliseconds())));
    metadata.favourite = static_cast<bool>(state.getProperty("favourite", false));
    return metadata;
}

PresetManager::PresetMetadata PresetManager::createDefaultMetadata(const juce::String& name,
                                                                   bool factory) const
{
    PresetMetadata metadata;
    metadata.name = name;
    metadata.author = "Hai Pham";
    metadata.category = factory ? "Factory" : "User";
    metadata.created = juce::Time::getCurrentTime();
    metadata.modified = metadata.created;
    return metadata;
}

void PresetManager::setParameter(const juce::String& parameterId, float plainValue)
{
    if (auto* parameter = apvts.getParameter(parameterId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
        parameter->endChangeGesture();
    }
}

bool PresetManager::restoreSnapshot(const juce::ValueTree& snapshot)
{
    if (!snapshot.isValid() || snapshot.getType() != apvts.state.getType())
        return false;
    apvts.replaceState(snapshot.createCopy());
    return true;
}


bool PresetManager::selectSnapshot(SnapshotSlot target)
{
    if (target == activeSnapshot)
        return true;

    if (activeSnapshot == SnapshotSlot::A)
        captureSnapshotA();
    else
        captureSnapshotB();

    const auto& targetState = target == SnapshotSlot::A ? snapshotA : snapshotB;
    if (!restoreSnapshot(targetState))
        return false;

    activeSnapshot = target;
    return true;
}
