#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace juce;

const Identifier FossProcessor::learnedId ("learned");

namespace
{
    String dbText (float v, int)     { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; }
    String pctText (float v, int)    { return String (roundToInt (v)) + " %"; }
    String bipolarText (float v, int){ return (v > 0.5f ? "+" : "") + String (roundToInt (v)); }
    String hzText (float v, int)     { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz"; }

    NormalisableRange<float> logRange (float lo, float hi)
    {
        return { lo, hi, [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                         [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                         [] (float, float, float v) { return (float) roundToInt (v); } };
    }
}

AudioProcessorValueTreeState::ParameterLayout FossProcessor::createLayout()
{
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto pct = [&] (const char* id, const char* name, float def, float hi = 100.0f)
    {
        ps.push_back (std::make_unique<P> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, hi, 0.1f), def,
                                           A().withLabel ("%").withStringFromValueFunction (pctText)));
    };
    auto bip = [&] (const char* id, const char* name)
    {
        ps.push_back (std::make_unique<P> (ParameterID { id, 1 }, name, NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
                                           A().withStringFromValueFunction (bipolarText)));
    };

    pct (ParamIDs::recover, "Recover", 30.0f);
    pct (ParamIDs::tame, "Tame", 30.0f);
    bip (ParamIDs::bias, "Bias");
    bip (ParamIDs::brighten, "Brighten");
    bip (ParamIDs::bass, "Bass");
    pct (ParamIDs::correction, "Correction", 100.0f, 150.0f);
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::profile, 1 }, "Profile", foss::profileNames, 0));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::lowFreq, 1 }, "Low Freq", logRange (20.0f, 2000.0f), 20.0f,
                                       A().withLabel ("Hz").withStringFromValueFunction (hzText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::highFreq, 1 }, "High Freq", logRange (1000.0f, 20000.0f), 20000.0f,
                                       A().withLabel ("Hz").withStringFromValueFunction (hzText)));
    pct (ParamIDs::mix, "Mix", 100.0f);
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::output, 1 }, "Output", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
                                       A().withLabel ("dB").withStringFromValueFunction (dbText)));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::autoGain, 1 }, "Auto Gain", true));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::delta, 1 }, "Delta", false));
    return { ps.begin(), ps.end() };
}

FossProcessor::FossProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Foss", createLayout())
{
    pRecover    = apvts.getRawParameterValue (ParamIDs::recover);
    pTame       = apvts.getRawParameterValue (ParamIDs::tame);
    pBias       = apvts.getRawParameterValue (ParamIDs::bias);
    pBrighten   = apvts.getRawParameterValue (ParamIDs::brighten);
    pBass       = apvts.getRawParameterValue (ParamIDs::bass);
    pCorrection = apvts.getRawParameterValue (ParamIDs::correction);
    pProfile    = apvts.getRawParameterValue (ParamIDs::profile);
    pLow        = apvts.getRawParameterValue (ParamIDs::lowFreq);
    pHigh       = apvts.getRawParameterValue (ParamIDs::highFreq);
    pMix        = apvts.getRawParameterValue (ParamIDs::mix);
    pOutput     = apvts.getRawParameterValue (ParamIDs::output);
    pAutoGain   = apvts.getRawParameterValue (ParamIDs::autoGain);
    pDelta      = apvts.getRawParameterValue (ParamIDs::delta);

    // The learned curve belongs to the track, not to the sound of a preset: keep it when loading presets.
    presets = std::make_unique<sbaudio::PresetManager> (apvts, "Foss", ".fosspreset", foss::factoryPresets(),
                                                        Array<Identifier> { learnedId });
    engine.prepare (48000.0, 2);
    setLatencySamples (engine.getLatency());
}

bool FossProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo());
}

void FossProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, getTotalNumOutputChannels());
    setLatencySamples (engine.getLatency());
}

foss::Settings FossProcessor::readSettings() const
{
    foss::Settings s;
    s.recover    = pRecover->load() * 0.01f;
    s.tame       = pTame->load() * 0.01f;
    s.bias       = pBias->load() * 0.01f;
    s.brighten   = pBrighten->load() * 0.01f;
    s.bass       = pBass->load() * 0.01f;
    s.correction = pCorrection->load() * 0.01f;
    s.profile    = jlimit (0, foss::numProfiles - 1, (int) pProfile->load());
    s.lowHz      = pLow->load();
    s.highHz     = jmax (s.lowHz * 1.5f, pHigh->load());
    s.mix        = pMix->load() * 0.01f;
    s.outputDb   = pOutput->load();
    s.autoGain   = pAutoGain->load() > 0.5f;
    s.delta      = pDelta->load() > 0.5f;
    return s;
}

void FossProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());
    engine.process (buffer, readSettings());
}

void FossProcessor::processBlockBypassed (AudioBuffer<float>& buffer, MidiBuffer&)
{
    // Unity gain through the same path, so latency stays constant while bypassed.
    ScopedNoDenormals noDenormals;
    auto s = readSettings();
    s.bypass = true;
    engine.process (buffer, s);
}

//==============================================================================
void FossProcessor::clearLearned()
{
    shared.learnCommand.store (3);
    shared.setLearned (nullptr, false);
    apvts.state.removeProperty (learnedId, nullptr);
    learnedString = {};
    stateVersionSynced = shared.learnedVersion.load();
}

void FossProcessor::syncLearnedToState()
{
    const int v = shared.learnedVersion.load();
    if (v == stateVersionSynced)
    {
        // the state was replaced from elsewhere (e.g. a user preset saved with a learned curve)
        if (apvts.state.getProperty (learnedId).toString() != learnedString)
            loadLearnedFromState();
        return;
    }
    stateVersionSynced = v;
    if (! shared.hasLearned.load())
    {
        apvts.state.removeProperty (learnedId, nullptr);
        learnedString = {};
        return;
    }
    StringArray vals;
    for (auto& a : shared.learnedDb)
        vals.add (String (a.load(), 2));
    learnedString = vals.joinIntoString (" ");
    apvts.state.setProperty (learnedId, learnedString, nullptr);
}

void FossProcessor::loadLearnedFromState()
{
    learnedString = apvts.state.getProperty (learnedId).toString();
    auto vals = StringArray::fromTokens (learnedString, " ", "");
    vals.removeEmptyStrings();
    if (vals.size() == foss::numBands)
    {
        std::array<float, foss::numBands> db {};
        for (int b = 0; b < foss::numBands; ++b) db[(size_t) b] = vals[b].getFloatValue();
        shared.setLearned (db.data(), true);
    }
    else
        shared.setLearned (nullptr, false);
    stateVersionSynced = shared.learnedVersion.load();
}

int FossProcessor::getNumPrograms()                { return jmax (1, presets->getNumFactoryPresets()); }
const String FossProcessor::getProgramName (int i) { return presets->getFactoryPresetName (i); }

void FossProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, presets->getNumFactoryPresets()))
        return;
    currentProgram = i;
    presets->loadFactoryPreset (i);
}

void FossProcessor::getStateInformation (MemoryBlock& destData)
{
    if (MessageManager::getInstanceWithoutCreating() != nullptr && MessageManager::getInstance()->isThisTheMessageThread())
        syncLearnedToState();
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FossProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml));
            loadLearnedFromState();
        }
}

AudioProcessorEditor* FossProcessor::createEditor() { return new FossEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FossProcessor(); }
