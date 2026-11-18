#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace juce;
using namespace ember;

namespace
{
    String dbText (float v, int)   { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; }
    String pctText (float v, int)  { return String (roundToInt (v)) + " %"; }
    String bipolarText (float v, int) { const int r = roundToInt (v); return (r > 0 ? "+" : "") + String (r) + " %"; }
    String hzText (float v, int)   { return v < 1000.0f ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, v < 10000.0f ? 2 : 1) + " kHz"; }

    float hzValue (const String& t)
    {
        const auto s = t.trim().toLowerCase();
        const float v = s.getFloatValue();
        return s.contains ("k") ? v * 1000.0f : v;
    }

    NormalisableRange<float> freqRange()
    {
        NormalisableRange<float> r (20.0f, 20000.0f, 0.0f);
        r.setSkewForCentre (632.0f); // geometric centre: log-like
        return r;
    }

    enum BandIdx { iStyle, iDrive, iDyn, iTone, iFb, iFbFreq, iMix, iLevel, iOn, iMute };
}

AudioProcessorValueTreeState::ParameterLayout EmberProcessor::createLayout()
{
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto dbParam = [] (const String& id, const String& name, float lo, float hi, float def)
    {
        return std::make_unique<P> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.1f), def,
                                    A().withLabel ("dB").withStringFromValueFunction (dbText));
    };
    auto pct = [] (const String& id, const String& name, float lo, float def, bool bipolar = false)
    {
        return std::make_unique<P> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, 100.0f, 0.1f), def,
                                    A().withLabel ("%").withStringFromValueFunction (bipolar ? bipolarText : pctText));
    };
    auto hz = [] (const String& id, const String& name, float def)
    {
        return std::make_unique<P> (ParameterID { id, 1 }, name, freqRange(), def,
                                    A().withLabel ("Hz").withStringFromValueFunction (hzText)
                                       .withValueFromStringFunction (hzValue));
    };

    layout.add (dbParam (ParamIDs::input, "Input", -24.0f, 24.0f, 0.0f));
    layout.add (dbParam (ParamIDs::output, "Output", -24.0f, 24.0f, 0.0f));
    layout.add (pct (ParamIDs::mix, "Mix", 0.0f, 100.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::quality, 1 }, "Quality", qualityNames, 1));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::autoGain, 1 }, "Auto Gain", true));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::bands, 1 }, "Bands", 1, maxBands, 1));
    for (int i = 0; i < maxSplits; ++i)
        layout.add (hz (ParamIDs::split (i), "Crossover " + String (i + 1), defaultSplits[i]));

    for (int b = 0; b < maxBands; ++b)
    {
        auto group = std::make_unique<AudioProcessorParameterGroup> ("band" + String (b + 1), "Band " + String (b + 1), " | ");
        const String n = "B" + String (b + 1) + " ";
        auto id = [b] (const char* name) { return ParamIDs::band (b, name); };
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { id ("style"), 1 }, n + "Style", styleNames, WarmTube));
        group->addChild (pct (id ("drive"), n + "Drive", 0.0f, 25.0f));
        group->addChild (pct (id ("dyn"), n + "Dynamics", -100.0f, 0.0f, true));
        group->addChild (pct (id ("tone"), n + "Tone", -100.0f, 0.0f, true));
        group->addChild (pct (id ("fb"), n + "Feedback", 0.0f, 0.0f));
        group->addChild (hz (id ("fbfreq"), n + "Feedback Freq", 1000.0f));
        group->addChild (pct (id ("mix"), n + "Mix", 0.0f, 100.0f));
        group->addChild (dbParam (id ("level"), n + "Level", -24.0f, 24.0f, 0.0f));
        group->addChild (std::make_unique<AudioParameterBool> (ParameterID { id ("on"), 1 }, n + "On", true));
        group->addChild (std::make_unique<AudioParameterBool> (ParameterID { id ("mute"), 1 }, n + "Mute", false));
        layout.add (std::move (group));
    }
    return layout;
}

EmberProcessor::EmberProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Ember", createLayout())
{
    pInput    = apvts.getRawParameterValue (ParamIDs::input);
    pOutput   = apvts.getRawParameterValue (ParamIDs::output);
    pMix      = apvts.getRawParameterValue (ParamIDs::mix);
    pQuality  = apvts.getRawParameterValue (ParamIDs::quality);
    pAutoGain = apvts.getRawParameterValue (ParamIDs::autoGain);
    pBands    = apvts.getRawParameterValue (ParamIDs::bands);
    for (int i = 0; i < maxSplits; ++i)
        splitPtrs[(size_t) i] = apvts.getRawParameterValue (ParamIDs::split (i));
    for (int b = 0; b < maxBands; ++b)
        for (int i = 0; i < 10; ++i)
            bandPtrs[(size_t) b].p[i] = apvts.getRawParameterValue (ParamIDs::band (b, ParamIDs::bandParamNames[i]));

    presets = std::make_unique<sbaudio::PresetManager> (apvts, "Ember", ".emberpreset", ember::factoryPresets(),
                                                        Array<Identifier> { Identifier ("selectedBand") });
}

bool EmberProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo());
}

void EmberProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    maxBlock = jmax (1, samplesPerBlock);
    engine.prepare (sampleRate, maxBlock);
    monoScratch.assign ((size_t) maxBlock, 0.0f);
    lastLatency = -1;
}

void EmberProcessor::storeMax (std::atomic<float>& a, float v) noexcept
{
    float cur = a.load (std::memory_order_relaxed);
    while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
}

int EmberProcessor::getNumBands() const
{
    return jlimit (1, maxBands, roundToInt (pBands->load()));
}

std::array<float, maxSplits> EmberProcessor::getSplits() const
{
    std::array<float, maxSplits> s {};
    for (int i = 0; i < maxSplits; ++i)
        s[(size_t) i] = splitPtrs[(size_t) i]->load();
    return sanitiseSplits (s, getNumBands());
}

void EmberProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    // While the GUI restructures bands, keep the previous settings and let the engine go dry;
    // once it is fully dry the new structure is picked up and faded in.
    const bool pending = restructurePending.load();
    auto& s = settings;
    if (! pending || ! haveSettings)
        readSettings();
    s.soloMask = soloMask.load();
    s.holdDry = pending;

    const int numCh = jmin (2, buffer.getNumChannels());
    const int total = buffer.getNumSamples();
    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = jmin (maxBlock, total - start);

        // analyzer input (the engine applies the input gain, so add it here)
        const float inG = Decibels::decibelsToGain (s.inputDb);
        FloatVectorOperations::copy (monoScratch.data(), buffer.getReadPointer (0, start), n);
        if (numCh > 1)
        {
            FloatVectorOperations::add (monoScratch.data(), buffer.getReadPointer (1, start), n);
            FloatVectorOperations::multiply (monoScratch.data(), 0.5f * inG, n);
        }
        else
            FloatVectorOperations::multiply (monoScratch.data(), inG, n);
        preRing.push (monoScratch.data(), n);

        engine.process (buffer, start, n, s);

        storeMax (inPeak, jmax (engine.lastInPeak[0], numCh > 1 ? engine.lastInPeak[1] : 0.0f));
        storeMax (outPeakL, engine.lastOutPeak[0]);
        storeMax (outPeakR, numCh > 1 ? engine.lastOutPeak[1] : engine.lastOutPeak[0]);

        FloatVectorOperations::copy (monoScratch.data(), buffer.getReadPointer (0, start), n);
        if (numCh > 1)
        {
            FloatVectorOperations::add (monoScratch.data(), buffer.getReadPointer (1, start), n);
            FloatVectorOperations::multiply (monoScratch.data(), 0.5f, n);
        }
        postRing.push (monoScratch.data(), n);
    }

    if (pending && engine.isFullyDry())
        restructurePending = false;

    if (const int lat = engine.getLatency(); lat != lastLatency)
    {
        lastLatency = lat;
        setLatencySamples (lat);
    }
}

void EmberProcessor::readSettings()
{
    auto& s = settings;
    haveSettings = true;
    s.inputDb  = pInput->load();
    s.outputDb = pOutput->load();
    s.mix      = pMix->load() * 0.01f;
    s.quality  = jlimit (0, 2, (int) pQuality->load());
    s.autoGain = pAutoGain->load() > 0.5f;
    s.numBands = getNumBands();
    for (int i = 0; i < maxSplits; ++i)
        s.splits[(size_t) i] = splitPtrs[(size_t) i]->load();
    for (int b = 0; b < maxBands; ++b)
    {
        auto& bp = bandPtrs[(size_t) b];
        auto& d = s.bands[(size_t) b];
        d.style    = jlimit (0, numStyles - 1, (int) bp.p[iStyle]->load());
        d.drive    = bp.p[iDrive]->load() * 0.01f;
        d.dynamics = bp.p[iDyn]->load() * 0.01f;
        d.tone     = bp.p[iTone]->load() * 0.01f;
        d.feedback = bp.p[iFb]->load() * 0.01f;
        d.fbFreq   = bp.p[iFbFreq]->load();
        d.mix      = bp.p[iMix]->load() * 0.01f;
        d.levelDb  = bp.p[iLevel]->load();
        d.on       = bp.p[iOn]->load() > 0.5f;
        d.mute     = bp.p[iMute]->load() > 0.5f;
    }
}

//==============================================================================
float EmberProcessor::bandValue (int band, const char* name) const
{
    if (auto* p = bandParam (band, name))
        return p->convertFrom0to1 (p->getValue());
    return 0.0f;
}

void EmberProcessor::setPlain (const String& id, float plainValue)
{
    if (auto* p = param (id))
    {
        const float norm = p->convertTo0to1 (plainValue);
        if (std::abs (norm - p->getValue()) < 1.0e-7f)
            return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }
}

void EmberProcessor::copyBand (int from, int to)
{
    for (auto* name : ParamIDs::bandParamNames)
        setPlain (ParamIDs::band (to, name), bandValue (from, name));
}

int EmberProcessor::insertSplit (float freq)
{
    const int n = getNumBands();
    if (n >= maxBands)
        return -1;

    const auto splits = getSplits();
    int k = 0; // band containing freq
    while (k < n - 1 && freq > splits[(size_t) k])
        ++k;

    const float lo = k == 0 ? 20.0f : splits[(size_t) k - 1];
    const float hi = k == n - 1 ? 20000.0f : splits[(size_t) k];
    freq = jlimit (lo * 1.2f, hi / 1.2f, freq);
    if (lo * 1.2f > hi / 1.2f)
        return -1; // band too narrow to split

    restructurePending = true; // cleared by the audio thread once it has gone dry
    for (int j = n - 1; j > k; --j)
        copyBand (j, j + 1);
    copyBand (k, k + 1);

    std::array<float, maxSplits> ns {};
    int w = 0;
    for (int i = 0; i < n - 1 && w < maxSplits; ++i)
    {
        if (i == k) ns[(size_t) w++] = freq;
        ns[(size_t) w++] = splits[(size_t) i];
    }
    if (k == n - 1 && w < maxSplits)
        ns[(size_t) w++] = freq;
    for (int i = 0; i < w; ++i)
        setPlain (ParamIDs::split (i), ns[(size_t) i]);

    setPlain (ParamIDs::bands, (float) (n + 1));
    soloMask = 0;
    return k + 1;
}

void EmberProcessor::deleteBand (int band)
{
    const int n = getNumBands();
    if (n <= 1 || ! isPositiveAndBelow (band, n))
        return;

    const auto splits = getSplits();
    const int removeSplit = band > 0 ? band - 1 : 0;

    restructurePending = true; // cleared by the audio thread once it has gone dry
    for (int j = band; j < n - 1; ++j)
        copyBand (j + 1, j);

    int w = 0;
    for (int i = 0; i < n - 1; ++i)
        if (i != removeSplit)
            setPlain (ParamIDs::split (w++), splits[(size_t) i]);

    setPlain (ParamIDs::bands, (float) (n - 1));
    soloMask = 0;
}

//==============================================================================
int EmberProcessor::getNumPrograms()                { return jmax (1, presets->getNumFactoryPresets()); }
const String EmberProcessor::getProgramName (int i) { return presets->getFactoryPresetName (i); }

void EmberProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, presets->getNumFactoryPresets()))
        return;
    currentProgram = i;
    presets->loadFactoryPreset (i);
}

void EmberProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void EmberProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* EmberProcessor::createEditor() { return new EmberEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EmberProcessor(); }
