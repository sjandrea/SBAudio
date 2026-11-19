#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace juce;

namespace
{
    String msText (float v, int)   { return String (roundToInt (v)) + " ms"; }
    String secText (float v, int)  { return v < 10.0f ? String (v, 2) + " s" : String (v, 1) + " s"; }
    String pctText (float v, int)  { return String (roundToInt (v)) + " %"; }
    String hzText (float v, int)   { return String (roundToInt (v)) + " Hz"; }
    String toneText (float v, int)
    {
        const int i = roundToInt (v);
        return i == 0 ? String ("0") : (i > 0 ? "+" + String (i) : String (i));
    }

    NormalisableRange<float> skewed (float lo, float hi, float interval, float centre)
    {
        NormalisableRange<float> r (lo, hi, interval);
        r.setSkewForCentre (centre);
        return r;
    }
}

AudioProcessorValueTreeState::ParameterLayout AuraProcessor::createLayout()
{
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::algo, 1 }, "Type", aura::algoNames, (int) aura::Hall));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::preDelay, 1 }, "Pre-delay",
                                       skewed (0.0f, aura::Engine::maxPreDelayMs, 1.0f, 50.0f), 20.0f,
                                       A().withLabel ("ms").withStringFromValueFunction (msText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::decay, 1 }, "Decay",
                                       skewed (0.2f, 20.0f, 0.01f, 2.5f), 2.0f,
                                       A().withLabel ("s").withStringFromValueFunction (secText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::size, 1 }, "Size",
                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
                                       A().withLabel ("%").withStringFromValueFunction (pctText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::tone, 1 }, "Tone",
                                       NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
                                       A().withStringFromValueFunction (toneText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::lowCut, 1 }, "Low Cut",
                                       skewed (20.0f, 600.0f, 1.0f, 150.0f), 100.0f,
                                       A().withLabel ("Hz").withStringFromValueFunction (hzText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::mod, 1 }, "Modulation",
                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f,
                                       A().withLabel ("%").withStringFromValueFunction (pctText)));
    return { ps.begin(), ps.end() };
}

AuraProcessor::AuraProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Aura", createLayout())
{
    pAlgo     = apvts.getRawParameterValue (ParamIDs::algo);
    pPreDelay = apvts.getRawParameterValue (ParamIDs::preDelay);
    pDecay    = apvts.getRawParameterValue (ParamIDs::decay);
    pSize     = apvts.getRawParameterValue (ParamIDs::size);
    pTone     = apvts.getRawParameterValue (ParamIDs::tone);
    pLowCut   = apvts.getRawParameterValue (ParamIDs::lowCut);
    pMod      = apvts.getRawParameterValue (ParamIDs::mod);

    presets = std::make_unique<sbaudio::PresetManager> (apvts, "Aura", ".aurapreset", aura::factoryPresets());
}

bool AuraProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out == AudioChannelSet::stereo())
        return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
    return out == AudioChannelSet::mono() && in == AudioChannelSet::mono();
}

void AuraProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

double AuraProcessor::getTailLengthSeconds() const
{
    return (double) pPreDelay->load() * 0.001 + (double) pDecay->load() * 1.2 + 0.2;
}

aura::Settings AuraProcessor::readSettings() const
{
    aura::Settings s;
    s.algo       = jlimit (0, aura::numAlgos - 1, (int) pAlgo->load());
    s.preDelayMs = pPreDelay->load();
    s.decayS     = pDecay->load();
    s.size       = pSize->load() * 0.01f;
    s.tone       = pTone->load() * 0.01f;
    s.lowCutHz   = pLowCut->load();
    s.mod        = pMod->load() * 0.01f;
    return s;
}

void AuraProcessor::storeMax (std::atomic<float>& a, float v) noexcept
{
    float cur = a.load (std::memory_order_relaxed);
    while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
}

void AuraProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int numIn = jmin (2, getTotalNumInputChannels());
    const int numOut = jmin (2, getTotalNumOutputChannels());
    const int n = buffer.getNumSamples();
    for (int c = numIn; c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (numIn < 1 || numOut < 1 || n == 0)
        return;

    float pin = 0.0f;
    for (int c = 0; c < numIn; ++c)
        pin = jmax (pin, buffer.getMagnitude (c, 0, n));
    storeMax (inPeak, pin);

    engine.process (buffer.getArrayOfWritePointers(), numIn, numOut, n, readSettings());

    storeMax (outPeakL, buffer.getMagnitude (0, 0, n));
    storeMax (outPeakR, buffer.getMagnitude (numOut > 1 ? 1 : 0, 0, n));
}

//==============================================================================
int AuraProcessor::getNumPrograms()                { return jmax (1, presets->getNumFactoryPresets()); }
const String AuraProcessor::getProgramName (int i) { return presets->getFactoryPresetName (i); }

void AuraProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, presets->getNumFactoryPresets()))
        return;
    currentProgram = i;
    presets->loadFactoryPreset (i);
}

void AuraProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AuraProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* AuraProcessor::createEditor() { return new AuraEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AuraProcessor(); }
