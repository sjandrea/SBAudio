#include "PresetManager.h"
#include "PluginProcessor.h"

using namespace juce;
using namespace prism;

namespace
{
    struct B
    {
        int type; float freq, gain, q;
        int slope = 3;
        bool dyn = false; float range = -6.0f, thr = -30.0f, att = 5.0f, rel = 80.0f;
        bool sc = false;
        int place = Stereo;
    };

    B cut (int type, float f, int slope, float q = 0.71f) { B b { type, f, 0.0f, q }; b.slope = slope; return b; }
    B dynBand (int type, float f, float gain, float q, float range, float thr, float att, float rel, bool sc = false)
    {
        B b { type, f, gain, q };
        b.dyn = true; b.range = range; b.thr = thr; b.att = att; b.rel = rel; b.sc = sc;
        return b;
    }

    struct Factory { const char* name; std::vector<B> bands; };

    // Slope index: 0 = 6, 1 = 12, 2 = 18, 3 = 24 dB/oct
    const std::vector<Factory>& factoryPresets()
    {
        static const std::vector<Factory> list {
            { "Standard (flat)", {} },

            { "Vokal – Ren og klar", {
                cut (LowCut, 80.0f, 3),
                { Bell, 300.0f, -2.5f, 1.0f },
                { Bell, 3000.0f, 2.0f, 0.9f },
                { HighShelf, 11000.0f, 2.5f, 0.71f },
                dynBand (Bell, 6800.0f, 0.0f, 3.0f, -6.0f, -32.0f, 1.0f, 60.0f) } },

            { "Vokal – Varm og nær", {
                cut (LowCut, 70.0f, 2),
                { LowShelf, 180.0f, 1.5f, 0.71f },
                dynBand (Bell, 3200.0f, 0.0f, 1.4f, -4.0f, -30.0f, 3.0f, 100.0f),
                { HighShelf, 12000.0f, 1.5f, 0.71f } } },

            { "Vokal – Luft og nærvær", {
                cut (LowCut, 90.0f, 3),
                { Bell, 4500.0f, 2.5f, 0.7f },
                { HighShelf, 14000.0f, 4.0f, 0.6f },
                dynBand (Bell, 7000.0f, 0.0f, 3.5f, -5.0f, -34.0f, 1.0f, 60.0f) } },

            { "Vokal – De-esser", {
                cut (LowCut, 60.0f, 1),
                dynBand (Bell, 6500.0f, 0.0f, 2.5f, -9.0f, -36.0f, 0.5f, 50.0f) } },

            { "Vokal – Proximity-kontroll", {
                cut (LowCut, 70.0f, 2),
                dynBand (LowShelf, 220.0f, 0.0f, 0.71f, -6.0f, -26.0f, 10.0f, 150.0f) } },

            { "Vokal – Telefon", {
                cut (LowCut, 500.0f, 3, 1.2f),
                cut (HighCut, 3500.0f, 3, 1.2f),
                { Bell, 1500.0f, 4.0f, 1.2f } } },

            { "Akustisk gitar – Klar og luftig", {
                cut (LowCut, 90.0f, 2),
                { Bell, 220.0f, -3.0f, 1.2f },
                { Bell, 5000.0f, 2.0f, 0.8f },
                { HighShelf, 12000.0f, 2.5f, 0.71f } } },

            { "Akustisk gitar – Slank i miks", {
                cut (LowCut, 140.0f, 3),
                { Bell, 250.0f, -3.5f, 1.1f },
                { Bell, 3500.0f, 1.5f, 0.9f },
                { HighShelf, 10000.0f, 1.5f, 0.71f } } },

            { "Akustisk gitar – Temme boom", {
                cut (LowCut, 60.0f, 1),
                dynBand (Bell, 140.0f, 0.0f, 1.4f, -6.0f, -24.0f, 8.0f, 120.0f),
                dynBand (Bell, 2500.0f, 0.0f, 2.0f, -3.0f, -30.0f, 3.0f, 80.0f) } },

            { "Akustisk gitar – Varm fingerspill", {
                cut (LowCut, 60.0f, 1),
                { LowShelf, 150.0f, 1.5f, 0.71f },
                { Bell, 1000.0f, -1.5f, 1.0f },
                dynBand (Bell, 3000.0f, 0.0f, 2.0f, -3.0f, -32.0f, 2.0f, 70.0f),
                { HighShelf, 9000.0f, 1.0f, 0.71f } } },

            { "Akustisk gitar – Plass til vokal (sidechain)", {
                cut (LowCut, 100.0f, 2),
                dynBand (Bell, 2500.0f, 0.0f, 0.8f, -4.0f, -30.0f, 10.0f, 150.0f, true),
                { HighShelf, 10000.0f, 1.0f, 0.71f } } },
        };
        return list;
    }

    const Identifier presetNameId ("presetName");
    const Identifier presetFactoryId ("presetFactory");
}

PresetManager::PresetManager (PrismEQProcessor& p) : proc (p) {}

int PresetManager::getNumFactoryPresets() const { return (int) factoryPresets().size(); }

String PresetManager::getFactoryPresetName (int i) const
{
    return isPositiveAndBelow (i, getNumFactoryPresets()) ? String (CharPointer_UTF8 (factoryPresets()[(size_t) i].name)) : String();
}

void PresetManager::loadFactoryPreset (int i)
{
    if (! isPositiveAndBelow (i, getNumFactoryPresets()))
        return;

    proc.resetAllParameters();
    int b = 0;
    for (const auto& s : factoryPresets()[(size_t) i].bands)
    {
        proc.setBandParam (b, "type", (float) s.type);
        proc.setBandParam (b, "freq", s.freq);
        proc.setBandParam (b, "gain", s.gain);
        proc.setBandParam (b, "q", s.q);
        proc.setBandParam (b, "slope", (float) s.slope);
        proc.setBandParam (b, "place", (float) s.place);
        proc.setBandParam (b, "dyn", s.dyn ? 1.0f : 0.0f);
        proc.setBandParam (b, "range", s.range);
        proc.setBandParam (b, "thr", s.thr);
        proc.setBandParam (b, "att", s.att);
        proc.setBandParam (b, "rel", s.rel);
        proc.setBandParam (b, "sc", s.sc ? 1.0f : 0.0f);
        proc.setBandParam (b, "on", 1.0f);
        proc.setBandParam (b, "used", 1.0f);
        ++b;
    }
    setCurrent (getFactoryPresetName (i), true);
}

File PresetManager::getUserPresetDirectory()
{
    return File::getSpecialLocation (File::userHomeDirectory)
               .getChildFile ("Library/Audio/Presets/SB Audio/PrismEQ");
}

StringArray PresetManager::getUserPresetNames() const
{
    StringArray names;
    for (auto& f : getUserPresetDirectory().findChildFiles (File::findFiles, false, String ("*") + fileExtension))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

bool PresetManager::userPresetExists (const String& name) const
{
    return getUserPresetDirectory().getChildFile (File::createLegalFileName (name) + fileExtension).existsAsFile();
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
    auto state = proc.apvts.copyState();
    auto xml = state.createXml();
    return xml != nullptr && xml->writeTo (dir.getChildFile (name + fileExtension));
}

bool PresetManager::loadUserPreset (const String& name)
{
    auto file = getUserPresetDirectory().getChildFile (name + fileExtension);
    auto xml = XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (proc.apvts.state.getType()))
        return false;

    // Keep the current display settings (not part of the sound).
    auto current = proc.apvts.copyState();
    auto tree = ValueTree::fromXml (*xml);
    for (auto id : { Identifier ("displayRange"), Identifier ("analyzer") })
        if (current.hasProperty (id))
            tree.setProperty (id, current[id], nullptr);

    proc.apvts.replaceState (tree);
    setCurrent (name, false);
    return true;
}

bool PresetManager::deleteUserPreset (const String& name)
{
    auto file = getUserPresetDirectory().getChildFile (name + fileExtension);
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

String PresetManager::getCurrentPresetName() const { return proc.apvts.state.getProperty (presetNameId).toString(); }
bool PresetManager::isCurrentFactory() const      { return (bool) proc.apvts.state.getProperty (presetFactoryId, false); }

void PresetManager::setCurrent (const String& name, bool factory)
{
    proc.apvts.state.setProperty (presetNameId, name, nullptr);
    proc.apvts.state.setProperty (presetFactoryId, factory, nullptr);
}
