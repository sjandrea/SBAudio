namespace sbaudio
{
using namespace juce;

namespace
{
    const Identifier presetNameId ("presetName");
    const Identifier presetFactoryId ("presetFactory");
}

PresetManager::PresetManager (AudioProcessorValueTreeState& s, const String& name, const String& ext,
                              std::vector<FactoryPreset> f, Array<Identifier> keep)
    : apvts (s), pluginName (name), extension (ext.startsWithChar ('.') ? ext : "." + ext),
      factory (std::move (f)), keepOnLoad (std::move (keep))
{
}

String PresetManager::getFactoryPresetName (int i) const
{
    return isPositiveAndBelow (i, getNumFactoryPresets()) ? factory[(size_t) i].name : String();
}

void PresetManager::setParam (RangedAudioParameter& p, float normalised)
{
    if (std::abs (p.getValue() - normalised) < 1.0e-7f)
        return;
    p.beginChangeGesture();
    p.setValueNotifyingHost (normalised);
    p.endChangeGesture();
}

void PresetManager::loadFactoryPreset (int i)
{
    if (! isPositiveAndBelow (i, getNumFactoryPresets()))
        return;

    const auto& preset = factory[(size_t) i];
    for (auto* base : apvts.processor.getParameters())
    {
        auto* p = dynamic_cast<RangedAudioParameter*> (base);
        if (p == nullptr)
            continue;

        float norm = p->getDefaultValue();
        for (const auto& [id, plain] : preset.values)
            if (id == p->getParameterID())
                norm = p->convertTo0to1 (plain);
        setParam (*p, norm);
    }
    setCurrent (preset.name, true);
}

File PresetManager::getUserPresetDirectory() const
{
    return File::getSpecialLocation (File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/SB Audio")
               .getChildFile (pluginName);
}

StringArray PresetManager::getUserPresetNames() const
{
    StringArray names;
    for (auto& f : getUserPresetDirectory().findChildFiles (File::findFiles, false, "*" + extension))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

bool PresetManager::userPresetExists (const String& name) const
{
    return getUserPresetDirectory().getChildFile (File::createLegalFileName (name.trim()) + extension).existsAsFile();
}

bool PresetManager::saveUserPreset (const String& rawName)
{
    const auto name = File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;

    auto dir = getUserPresetDirectory();
    if (! dir.createDirectory())
        return false;

    setCurrent (name, false);
    auto xml = apvts.copyState().createXml();
    return xml != nullptr && xml->writeTo (dir.getChildFile (name + extension));
}

bool PresetManager::loadUserPreset (const String& name)
{
    auto xml = XmlDocument::parse (getUserPresetDirectory().getChildFile (name + extension));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    // Keep display settings etc. that are not part of the sound.
    auto current = apvts.copyState();
    auto tree = ValueTree::fromXml (*xml);
    for (auto& id : keepOnLoad)
        if (current.hasProperty (id))
            tree.setProperty (id, current[id], nullptr);

    apvts.replaceState (tree);
    setCurrent (name, false);
    return true;
}

bool PresetManager::deleteUserPreset (const String& name)
{
    auto file = getUserPresetDirectory().getChildFile (name + extension);
    if (! file.existsAsFile())
        return false;
    const bool ok = file.moveToTrash();
    if (ok && getCurrentPresetName() == name && ! isCurrentFactory())
        setCurrent ({}, false);
    return ok;
}

std::vector<PresetManager::Entry> PresetManager::getAllPresets() const
{
    std::vector<Entry> all;
    for (int i = 0; i < getNumFactoryPresets(); ++i)
        all.push_back ({ getFactoryPresetName (i), true, i });
    int u = 0;
    for (auto& n : getUserPresetNames())
        all.push_back ({ n, false, u++ });
    return all;
}

void PresetManager::load (const Entry& e)
{
    if (e.factory) loadFactoryPreset (e.index);
    else           loadUserPreset (e.name);
}

void PresetManager::step (int delta)
{
    auto all = getAllPresets();
    if (all.empty())
        return;
    int cur = -1;
    for (int i = 0; i < (int) all.size(); ++i)
        if (all[(size_t) i].name == getCurrentPresetName() && all[(size_t) i].factory == isCurrentFactory())
            cur = i;
    const int n = (int) all.size();
    const int next = cur < 0 ? (delta > 0 ? 0 : n - 1) : ((cur + delta) % n + n) % n;
    load (all[(size_t) next]);
}

String PresetManager::getCurrentPresetName() const { return apvts.state.getProperty (presetNameId).toString(); }
bool PresetManager::isCurrentFactory() const      { return (bool) apvts.state.getProperty (presetFactoryId, false); }

void PresetManager::setCurrent (const String& name, bool isFactory)
{
    apvts.state.setProperty (presetNameId, name, nullptr);
    apvts.state.setProperty (presetFactoryId, isFactory, nullptr);
}
}
