#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace juce;

namespace
{
    String dbText (float v, int)     { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; }
    String lufsText (float v, int)   { return String (v, 1) + " LUFS"; }
    String dbtpText (float v, int)   { return String (v, 1) + " dBTP"; }
    String pctText (float v, int)    { return String (roundToInt (v)) + " %"; }

    const Identifier analysisAttr ("glansAnalysis");
    const Identifier autoStartAttr ("glansAutoStartDone");
}

AudioProcessorValueTreeState::ParameterLayout DominaProcessor::createLayout()
{
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto toneParam = [&] (const char* id, const char* name)
    {
        ps.push_back (std::make_unique<P> (ParameterID { id, 1 }, name, NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f,
                                           A().withLabel ("dB").withStringFromValueFunction (dbText)));
    };
    auto pctParam = [&] (const char* id, const char* name, float max, float def)
    {
        ps.push_back (std::make_unique<P> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, max, 1.0f), def,
                                           A().withLabel ("%").withStringFromValueFunction (pctText)));
    };

    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::style, 1 }, "Style", domina::styleNames, domina::Balanced));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::loudness, 1 }, "Loudness",
                                       NormalisableRange<float> (domina::minTargetLufs, domina::maxTargetLufs, 0.1f), -11.0f,
                                       A().withLabel ("LUFS").withStringFromValueFunction (lufsText)));
    toneParam (ParamIDs::low, "Low");
    toneParam (ParamIDs::mid, "Mid");
    toneParam (ParamIDs::presence, "Presence");
    toneParam (ParamIDs::high, "High");
    pctParam (ParamIDs::autoEq, "Auto EQ", 100.0f, 70.0f);
    pctParam (ParamIDs::compression, "Compression", 100.0f, 40.0f);
    pctParam (ParamIDs::deess, "De-ess", 100.0f, 20.0f);
    pctParam (ParamIDs::saturation, "Saturation", 100.0f, 20.0f);
    pctParam (ParamIDs::width, "Width", 200.0f, 100.0f);
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::monoBass, 1 }, "Mono Bass", false));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::ceiling, 1 }, "Ceiling",
                                       NormalisableRange<float> (-3.0f, 0.0f, 0.1f), -1.0f,
                                       A().withLabel ("dBTP").withStringFromValueFunction (dbtpText)));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::compare, 1 }, "Compare", false));
    return { ps.begin(), ps.end() };
}

DominaProcessor::DominaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Glans", createLayout())   // state type kept from the Glans days (saved projects)
{
    pStyle       = apvts.getRawParameterValue (ParamIDs::style);
    pLoudness    = apvts.getRawParameterValue (ParamIDs::loudness);
    pLow         = apvts.getRawParameterValue (ParamIDs::low);
    pMid         = apvts.getRawParameterValue (ParamIDs::mid);
    pPresence    = apvts.getRawParameterValue (ParamIDs::presence);
    pHigh        = apvts.getRawParameterValue (ParamIDs::high);
    pAutoEq      = apvts.getRawParameterValue (ParamIDs::autoEq);
    pCompression = apvts.getRawParameterValue (ParamIDs::compression);
    pDeess       = apvts.getRawParameterValue (ParamIDs::deess);
    pSaturation  = apvts.getRawParameterValue (ParamIDs::saturation);
    pWidth       = apvts.getRawParameterValue (ParamIDs::width);
    pMonoBass    = apvts.getRawParameterValue (ParamIDs::monoBass);
    pCeiling     = apvts.getRawParameterValue (ParamIDs::ceiling);
    pCompare     = apvts.getRawParameterValue (ParamIDs::compare);

    presets = std::make_unique<sbaudio::PresetManager> (apvts, "Domina", ".dominapreset", domina::factoryPresets());
}

bool DominaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo());
}

void DominaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatency());
}

domina::Settings DominaProcessor::readSettings() const
{
    domina::Settings s;
    s.style       = jlimit (0, 2, (int) pStyle->load());
    s.targetLufs  = pLoudness->load();
    s.tone        = { pLow->load(), pMid->load(), pPresence->load(), pHigh->load() };
    s.correction  = pAutoEq->load() * 0.01f;
    s.compression = pCompression->load() * 0.01f;
    s.deess       = pDeess->load() * 0.01f;
    s.saturation  = pSaturation->load() * 0.01f;
    s.width       = pWidth->load() * 0.01f;
    s.monoBass    = pMonoBass->load() > 0.5f;
    s.ceilingDb   = pCeiling->load();
    s.compare     = pCompare->load() > 0.5f;
    return s;
}

void DominaProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());
    engine.process (buffer, readSettings());
}

void DominaProcessor::processBlockBypassed (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    engine.processBypassed (buffer);
}

//==============================================================================
int DominaProcessor::getNumPrograms()                { return jmax (1, presets->getNumFactoryPresets()); }
const String DominaProcessor::getProgramName (int i) { return presets->getFactoryPresetName (i); }

void DominaProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, presets->getNumFactoryPresets()))
        return;
    currentProgram = i;
    presets->loadFactoryPreset (i);
}

// The analysis is stored next to the parameters, not in apvts.state, so presets never carry it.
void DominaProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute (analysisAttr, shared.loadData().toString());
        xml->setAttribute (autoStartAttr, shared.autoStartDone.load() ? 1 : 0);
        copyXmlToBinary (*xml, destData);
    }
}

void DominaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    const auto analysis = domina::AnalysisData::fromString (xml->getStringAttribute (analysisAttr));
    shared.autoStartDone.store (xml->getIntAttribute (autoStartAttr, 0) != 0 || analysis.valid);
    xml->removeAttribute (analysisAttr);
    xml->removeAttribute (autoStartAttr);
    apvts.replaceState (ValueTree::fromXml (*xml));

    shared.storeData (analysis);
    shared.loadVersion.fetch_add (1);
}

AudioProcessorEditor* DominaProcessor::createEditor() { return new DominaEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DominaProcessor(); }
