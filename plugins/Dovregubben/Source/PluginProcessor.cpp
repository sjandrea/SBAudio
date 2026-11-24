#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace juce;

namespace
{
    String msText (float v, int)   { return v < 1000.0f ? String (v, v < 100.0f ? 1 : 0) + " ms" : String (v * 0.001f, 2) + " s"; }
    String pctText (float v, int)  { return String (roundToInt (v)) + " %"; }
    String hzText (float v, int)   { return v < 1000.0f ? String (roundToInt (v)) + " Hz" : String (v * 0.001f, 1) + " kHz"; }
    String rateText (float v, int) { return String (v, v < 1.0f ? 2 : 1) + " Hz"; }
    String stText (float v, int)
    {
        const int i = roundToInt (v);
        return (i > 0 ? "+" : "") + String (i) + " st";
    }
    String centText (float v, int)
    {
        const int i = roundToInt (v);
        return (i > 0 ? "+" : "") + String (i) + " ct";
    }

    NormalisableRange<float> skewed (float lo, float hi, float interval, float centre)
    {
        NormalisableRange<float> r (lo, hi, interval);
        r.setSkewForCentre (centre);
        return r;
    }

    // order of the raw parameter pointers
    const char* const ids[] { ParamIDs::mode, ParamIDs::style, ParamIDs::mix, ParamIDs::sync, ParamIDs::delay, ParamIDs::note,
                              ParamIDs::ratio, ParamIDs::feedback, ParamIDs::width, ParamIDs::age, ParamIDs::duck,
                              ParamIDs::diffusion, ParamIDs::diffSize, ParamIDs::modRate, ParamIDs::modDepth,
                              ParamIDs::lowCut, ParamIDs::highCut, ParamIDs::pitch, ParamIDs::detune };
    enum { iMode, iStyle, iMix, iSync, iDelay, iNote, iRatio, iFb, iWidth, iAge, iDuck, iDiff, iDiffSize,
           iRate, iDepth, iLow, iHigh, iPitch, iDetune, numIds };
}

AudioProcessorValueTreeState::ParameterLayout DovregubbenProcessor::createLayout()
{
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto pct = [&] (const char* id, const char* name, float def)
    {
        ps.push_back (std::make_unique<P> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
                                           A().withLabel ("%").withStringFromValueFunction (pctText)));
    };

    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Mode", dovre::modeNames, (int) dovre::Tape));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::style, 1 }, "Style", dovre::styleNames, (int) dovre::Dual));
    pct (ParamIDs::mix, "Mix", 25.0f);
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::sync, 1 }, "Sync", false));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::delay, 1 }, "Delay", skewed (1.0f, dovre::maxDelayMs, 0.1f, 300.0f), 375.0f,
                                       A().withLabel ("ms").withStringFromValueFunction (msText)));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::note, 1 }, "Note", dovre::noteNames, 10));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::ratio, 1 }, "Delay R", NormalisableRange<float> (10.0f, 200.0f, 0.1f), 100.0f,
                                       A().withLabel ("%").withStringFromValueFunction (pctText)));
    pct (ParamIDs::feedback, "Feedback", 30.0f);
    pct (ParamIDs::width, "Width", 100.0f);
    pct (ParamIDs::age, "Age", 25.0f);
    pct (ParamIDs::duck, "Ducking", 0.0f);
    pct (ParamIDs::diffusion, "Diffusion", 0.0f);
    pct (ParamIDs::diffSize, "Diffusion Size", 50.0f);
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::modRate, 1 }, "Mod Rate", skewed (0.05f, 10.0f, 0.01f, 1.0f), 0.5f,
                                       A().withLabel ("Hz").withStringFromValueFunction (rateText)));
    pct (ParamIDs::modDepth, "Mod Depth", 20.0f);
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::lowCut, 1 }, "Low Cut", skewed (10.0f, 2000.0f, 1.0f, 200.0f), 100.0f,
                                       A().withLabel ("Hz").withStringFromValueFunction (hzText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::highCut, 1 }, "High Cut", skewed (500.0f, 20000.0f, 1.0f, 5000.0f), 8000.0f,
                                       A().withLabel ("Hz").withStringFromValueFunction (hzText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::pitch, 1 }, "Pitch", NormalisableRange<float> (-12.0f, 12.0f, 1.0f), 0.0f,
                                       A().withLabel ("st").withStringFromValueFunction (stText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::detune, 1 }, "Detune", NormalisableRange<float> (-50.0f, 50.0f, 0.1f), 0.0f,
                                       A().withLabel ("ct").withStringFromValueFunction (centText)));
    return { ps.begin(), ps.end() };
}

DovregubbenProcessor::DovregubbenProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Dovregubben", createLayout())
{
    static_assert (numIds == 19, "parameter pointer table");
    for (int i = 0; i < numIds; ++i)
        p[i] = apvts.getRawParameterValue (ids[i]);

    presets = std::make_unique<sbaudio::PresetManager> (apvts, "Dovregubben", ".dovrepreset", dovre::factoryPresets());
}

bool DovregubbenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out == AudioChannelSet::stereo())
        return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
    return out == AudioChannelSet::mono() && in == AudioChannelSet::mono();
}

void DovregubbenProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

dovre::Settings DovregubbenProcessor::readSettings() const
{
    dovre::Settings s;
    s.mode  = jlimit (0, dovre::numModes - 1, (int) p[iMode]->load());
    s.style = jlimit (0, dovre::numStyles - 1, (int) p[iStyle]->load());

    float d = p[iDelay]->load();
    if (p[iSync]->load() > 0.5f)
    {
        const int ni = jlimit (0, dovre::noteNames.size() - 1, (int) p[iNote]->load());
        d = 60000.0f / jmax (20.0f, bpm.load()) * dovre::noteBeats[ni];
    }
    s.delayMsL = jlimit (1.0f, dovre::maxDelayMs, d);
    s.delayMsR = jlimit (1.0f, dovre::maxDelayMs, d * dovre::delayRatioR (p[iRatio]->load()));

    s.mix         = p[iMix]->load() * 0.01f;
    s.feedback    = p[iFb]->load() * 0.01f;
    s.width       = p[iWidth]->load() * 0.01f;
    s.age         = p[iAge]->load() * 0.01f;
    s.duck        = p[iDuck]->load() * 0.01f;
    s.diffusion   = p[iDiff]->load() * 0.01f;
    s.diffSize    = p[iDiffSize]->load() * 0.01f;
    s.modRate     = p[iRate]->load();
    s.modDepth    = p[iDepth]->load() * 0.01f;
    s.lowCutHz    = p[iLow]->load();
    s.highCutHz   = p[iHigh]->load();
    s.pitchSt     = p[iPitch]->load();
    s.detuneCents = p[iDetune]->load();
    return s;
}

double DovregubbenProcessor::getTailLengthSeconds() const
{
    const auto s = readSettings();
    const double d = jmax (s.delayMsL, s.delayMsR) * 0.001;
    const double fb = jlimit (0.0, 0.999, (double) s.feedback);
    const double repeats = fb < 0.01 ? 1.0 : std::log (0.001) / std::log (fb) + 1.0;
    return jmin (30.0, d * 2.0 * repeats + 0.3);
}

void DovregubbenProcessor::storeMax (std::atomic<float>& a, float v) noexcept
{
    float cur = a.load (std::memory_order_relaxed);
    while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
}

void DovregubbenProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int numIn = jmin (2, getTotalNumInputChannels());
    const int numOut = jmin (2, getTotalNumOutputChannels());
    const int n = buffer.getNumSamples();
    for (int c = numIn; c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (numIn < 1 || numOut < 1 || n == 0)
        return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm.store ((float) jlimit (20.0, 400.0, *b));

    float pin = 0.0f;
    for (int c = 0; c < numIn; ++c)
        pin = jmax (pin, buffer.getMagnitude (c, 0, n));
    storeMax (inPeak, pin);

    engine.process (buffer.getArrayOfWritePointers(), numIn, numOut, n, readSettings());

    storeMax (outPeakL, buffer.getMagnitude (0, 0, n));
    storeMax (outPeakR, buffer.getMagnitude (numOut > 1 ? 1 : 0, 0, n));
}

//==============================================================================
int DovregubbenProcessor::getNumPrograms()                { return jmax (1, presets->getNumFactoryPresets()); }
const String DovregubbenProcessor::getProgramName (int i) { return presets->getFactoryPresetName (i); }

void DovregubbenProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, presets->getNumFactoryPresets()))
        return;
    currentProgram = i;
    presets->loadFactoryPreset (i);
}

void DovregubbenProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DovregubbenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* DovregubbenProcessor::createEditor() { return new DovregubbenEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DovregubbenProcessor(); }
