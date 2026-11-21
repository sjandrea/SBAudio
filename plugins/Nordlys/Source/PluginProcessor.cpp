#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace juce;
using namespace nordlys;

namespace
{
    String dbText (float v, int)    { return String (v, 1) + " dB"; }
    String pctText (float v, int)   { return String (roundToInt (v)) + " %"; }
    String bipText (float v, int)   { const int i = roundToInt (v); return (i > 0 ? "+" : "") + String (i) + " %"; }
    String centText (float v, int)  { const int i = roundToInt (v); return (i > 0 ? "+" : "") + String (i) + " ct"; }
    String semiText (float v, int)  { const int i = roundToInt (v); return (i > 0 ? "+" : "") + String (i) + " st"; }
    String octText (float v, int)   { const int i = roundToInt (v); return (i > 0 ? "+" : "") + String (i) + " oct"; }
    String panText (float v, int)   { const int i = roundToInt (v); return i == 0 ? String ("C") : (i < 0 ? "L" + String (-i) : "R" + String (i)); }
    String msText (float v, int)    { return v < 10.0f ? String (v, 1) + " ms" : v < 1000.0f ? String (roundToInt (v)) + " ms"
                                                                                           : String (v / 1000.0f, 2) + " s"; }
    String hzText (float v, int)    { return v < 1000.0f ? String (v, v < 10.0f ? 2 : (v < 100.0f ? 1 : 0)) + " Hz"
                                                         : String (v / 1000.0f, 2) + " kHz"; }
    String secText (float v, int)   { return String (v, 1) + " s"; }
    String voicesText (float v, int){ return String (roundToInt (v)); }
    String grainText (float v, int) { return String (v, 1) + "x"; }

    NormalisableRange<float> skewed (float lo, float hi, float interval, float centre)
    {
        NormalisableRange<float> r (lo, hi, interval);
        r.setSkewForCentre (centre);
        return r;
    }
}

AudioProcessorValueTreeState::ParameterLayout NordlysProcessor::createLayout()
{
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto flt = [&ps] (const String& id, const String& name, NormalisableRange<float> r, float def,
                      String (*fmt) (float, int), const char* label = "")
    {
        ps.push_back (std::make_unique<P> (ParameterID { id, 1 }, name, r, def, A().withLabel (label).withStringFromValueFunction (fmt)));
    };
    auto choice = [&ps] (const String& id, const String& name, const StringArray& items, int def)
    {
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto boolean = [&ps] (const String& id, const String& name, bool def)
    {
        ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto integer = [&ps] (const String& id, const String& name, int lo, int hi, int def, String (*fmt) (float, int))
    {
        ps.push_back (std::make_unique<P> (ParameterID { id, 1 }, name, NormalisableRange<float> ((float) lo, (float) hi, 1.0f), (float) def,
                                           A().withStringFromValueFunction (fmt)));
    };

    // ---- Global ----
    flt (ParamIDs::master, "Master", NormalisableRange<float> (-48.0f, 6.0f, 0.1f), -6.0f, dbText, "dB");
    choice (ParamIDs::polyMode, "Voice Mode", polyModeNames, modePoly);
    flt (ParamIDs::glide, "Glide", skewed (0.0f, 2000.0f, 1.0f, 200.0f), 0.0f, msText, "ms");
    integer (ParamIDs::bend, "Bend Range", 0, 24, 2, semiText);
    flt (ParamIDs::velSens, "Velocity", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 60.0f, pctText, "%");

    flt (ParamIDs::orbX, "Orb X", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, bipText, "%");
    flt (ParamIDs::orbY, "Orb Y", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, bipText, "%");
    flt (ParamIDs::orbRate, "Orb Speed", skewed (0.0f, 4.0f, 0.001f, 0.3f), 0.0f, hzText, "Hz");
    flt (ParamIDs::orbDepth, "Orb Radius", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pctText, "%");

    boolean (ParamIDs::arpOn, "Arp On", false);
    choice (ParamIDs::arpMode, "Arp Mode", arpModeNames, arpUp);
    choice (ParamIDs::arpRate, "Arp Rate", arpRateNames, 5);
    integer (ParamIDs::arpOct, "Arp Octaves", 1, 4, 1, voicesText);
    flt (ParamIDs::arpGate, "Arp Gate", NormalisableRange<float> (5.0f, 100.0f, 0.1f), 60.0f, pctText, "%");

    flt (ParamIDs::choMix, "Chorus Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pctText, "%");
    flt (ParamIDs::choRate, "Chorus Rate", skewed (0.05f, 5.0f, 0.01f, 0.6f), 0.4f, hzText, "Hz");
    flt (ParamIDs::choDepth, "Chorus Depth", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, pctText, "%");
    flt (ParamIDs::dlyMix, "Delay Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pctText, "%");
    choice (ParamIDs::dlyTime, "Delay Time", delayTimeNames, 7);
    flt (ParamIDs::dlyFb, "Delay Feedback", NormalisableRange<float> (0.0f, 95.0f, 0.1f), 35.0f, pctText, "%");
    flt (ParamIDs::dlyTone, "Delay Tone", skewed (500.0f, 18000.0f, 1.0f, 4000.0f), 6000.0f, hzText, "Hz");
    flt (ParamIDs::revMix, "Reverb Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f, pctText, "%");
    choice (ParamIDs::revType, "Reverb Type", reverbTypeNames, 3);
    flt (ParamIDs::revDecay, "Reverb Decay", skewed (0.3f, 20.0f, 0.01f, 3.0f), 3.5f, secText, "s");
    flt (ParamIDs::revTone, "Reverb Tone", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, bipText, "%");

    // ---- Layers ----
    for (int l = 0; l < numLayers; ++l)
    {
        const String pre = layerLetter (l) + " ";
        auto id = [l] (const char* n) { return layerId (l, n); };

        boolean (id (lp::on), pre + "On", l == 0);
        flt (id (lp::level), pre + "Level", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 80.0f, pctText, "%");
        flt (id (lp::pan), pre + "Pan", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, panText);
        integer (id (lp::oct), pre + "Octave", -3, 3, 0, octText);
        integer (id (lp::coarse), pre + "Coarse", -12, 12, 0, semiText);
        flt (id (lp::fine), pre + "Fine", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, centText, "ct");

        choice (id (lp::osc), pre + "Oscillator", oscTypeNames, oscAnalog);
        choice (id (lp::wave), pre + "Wave", waveNames, 0);
        flt (id (lp::shape), pre + "Shape", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 66.7f, pctText, "%");
        flt (id (lp::sym), pre + "Symmetry", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pctText, "%");
        integer (id (lp::uni), pre + "Unison", 1, maxUnison, 1, voicesText);
        flt (id (lp::detune), pre + "Detune", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 15.0f, centText, "ct");
        flt (id (lp::spread), pre + "Spread", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pctText, "%");

        flt (id (lp::gsize), pre + "Grain Size", skewed (10.0f, 1000.0f, 0.1f, 150.0f), 120.0f, msText, "ms");
        flt (id (lp::gdens), pre + "Grain Density", NormalisableRange<float> (1.0f, 16.0f, 0.1f), 6.0f, grainText);
        flt (id (lp::gspray), pre + "Grain Spray", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 20.0f, pctText, "%");
        flt (id (lp::gpitch), pre + "Grain Pitch", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 8.0f, centText, "ct");
        flt (id (lp::gshim), pre + "Shimmer", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pctText, "%");

        choice (id (lp::ftype), pre + "Filter Type", filterTypeNames, fltLP24);
        flt (id (lp::cutoff), pre + "Cutoff", skewed (20.0f, 20000.0f, 0.1f, 1000.0f), 8000.0f, hzText, "Hz");
        flt (id (lp::reso), pre + "Resonance", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 10.0f, pctText, "%");
        flt (id (lp::fenv), pre + "Filter Env", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, bipText, "%");
        flt (id (lp::fkey), pre + "Key Track", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pctText, "%");
        flt (id (lp::drive), pre + "Drive", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pctText, "%");

        flt (id (lp::aA), pre + "Amp Attack", skewed (0.5f, 10000.0f, 0.1f, 300.0f), 5.0f, msText, "ms");
        flt (id (lp::aD), pre + "Amp Decay", skewed (1.0f, 10000.0f, 0.1f, 600.0f), 600.0f, msText, "ms");
        flt (id (lp::aS), pre + "Amp Sustain", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 80.0f, pctText, "%");
        flt (id (lp::aR), pre + "Amp Release", skewed (5.0f, 15000.0f, 0.1f, 800.0f), 400.0f, msText, "ms");
        flt (id (lp::fA), pre + "Filter Attack", skewed (0.5f, 10000.0f, 0.1f, 300.0f), 5.0f, msText, "ms");
        flt (id (lp::fD), pre + "Filter Decay", skewed (1.0f, 10000.0f, 0.1f, 600.0f), 800.0f, msText, "ms");
        flt (id (lp::fS), pre + "Filter Sustain", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, pctText, "%");
        flt (id (lp::fR), pre + "Filter Release", skewed (5.0f, 15000.0f, 0.1f, 800.0f), 600.0f, msText, "ms");

        flt (id (lp::l1rate), pre + "LFO 1 Rate", skewed (0.02f, 20.0f, 0.001f, 1.5f), 4.0f, hzText, "Hz");
        choice (id (lp::l1shape), pre + "LFO 1 Shape", lfoShapeNames, lfoSine);
        flt (id (lp::l1fade), pre + "LFO 1 Fade", skewed (0.0f, 5000.0f, 1.0f, 600.0f), 0.0f, msText, "ms");
        flt (id (lp::l2rate), pre + "LFO 2 Rate", skewed (0.02f, 20.0f, 0.001f, 1.5f), 0.3f, hzText, "Hz");
        choice (id (lp::l2shape), pre + "LFO 2 Shape", lfoShapeNames, lfoTriangle);
        flt (id (lp::l2fade), pre + "LFO 2 Fade", skewed (0.0f, 5000.0f, 1.0f, 600.0f), 0.0f, msText, "ms");
    }

    // ---- Modulation matrix ----
    for (int m = 0; m < numSlots; ++m)
    {
        const String pre = "Mod " + String (m + 1) + " ";
        choice (slotId (m, mp::src), pre + "Source", modSrcNames, srcOff);
        choice (slotId (m, mp::via), pre + "Via", modSrcNames, srcOff);
        choice (slotId (m, mp::dst), pre + "Target", modDstNames, dstOff);
        choice (slotId (m, mp::layer), pre + "Layer", layerTargetNames, 0);
        flt (slotId (m, mp::amt), pre + "Amount", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, bipText, "%");
    }

    return { ps.begin(), ps.end() };
}

NordlysProcessor::NordlysProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Nordlys", createLayout())
{
    auto raw = [this] (const String& id)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    for (int l = 0; l < numLayers; ++l)
    {
        auto r = [&] (const char* n) { return raw (layerId (l, n)); };
        auto& p = layerPtrs[(size_t) l];
        p.on = r (lp::on); p.level = r (lp::level); p.pan = r (lp::pan); p.oct = r (lp::oct); p.coarse = r (lp::coarse);
        p.fine = r (lp::fine); p.osc = r (lp::osc); p.wave = r (lp::wave); p.shape = r (lp::shape); p.sym = r (lp::sym);
        p.uni = r (lp::uni); p.detune = r (lp::detune); p.spread = r (lp::spread);
        p.gsize = r (lp::gsize); p.gdens = r (lp::gdens); p.gspray = r (lp::gspray); p.gpitch = r (lp::gpitch); p.gshim = r (lp::gshim);
        p.ftype = r (lp::ftype); p.cutoff = r (lp::cutoff); p.reso = r (lp::reso); p.fenv = r (lp::fenv); p.fkey = r (lp::fkey);
        p.drive = r (lp::drive);
        p.aA = r (lp::aA); p.aD = r (lp::aD); p.aS = r (lp::aS); p.aR = r (lp::aR);
        p.fA = r (lp::fA); p.fD = r (lp::fD); p.fS = r (lp::fS); p.fR = r (lp::fR);
        p.l1rate = r (lp::l1rate); p.l1shape = r (lp::l1shape); p.l1fade = r (lp::l1fade);
        p.l2rate = r (lp::l2rate); p.l2shape = r (lp::l2shape); p.l2fade = r (lp::l2fade);
    }
    for (int m = 0; m < numSlots; ++m)
    {
        auto& p = slotPtrs[(size_t) m];
        p.src = raw (slotId (m, mp::src)); p.via = raw (slotId (m, mp::via)); p.dst = raw (slotId (m, mp::dst));
        p.layer = raw (slotId (m, mp::layer)); p.amt = raw (slotId (m, mp::amt));
    }

    pMaster = raw (ParamIDs::master); pPolyMode = raw (ParamIDs::polyMode); pGlide = raw (ParamIDs::glide);
    pBend = raw (ParamIDs::bend); pVelSens = raw (ParamIDs::velSens);
    pOrbX = raw (ParamIDs::orbX); pOrbY = raw (ParamIDs::orbY); pOrbRate = raw (ParamIDs::orbRate); pOrbDepth = raw (ParamIDs::orbDepth);
    pArpOn = raw (ParamIDs::arpOn); pArpMode = raw (ParamIDs::arpMode); pArpRate = raw (ParamIDs::arpRate);
    pArpOct = raw (ParamIDs::arpOct); pArpGate = raw (ParamIDs::arpGate);
    pChoMix = raw (ParamIDs::choMix); pChoRate = raw (ParamIDs::choRate); pChoDepth = raw (ParamIDs::choDepth);
    pDlyMix = raw (ParamIDs::dlyMix); pDlyTime = raw (ParamIDs::dlyTime); pDlyFb = raw (ParamIDs::dlyFb); pDlyTone = raw (ParamIDs::dlyTone);
    pRevMix = raw (ParamIDs::revMix); pRevType = raw (ParamIDs::revType); pRevDecay = raw (ParamIDs::revDecay); pRevTone = raw (ParamIDs::revTone);

    WaveBank::get(); // build the wavetables now rather than on the audio thread

    presets = std::make_unique<sbaudio::PresetManager> (apvts, "Nordlys", ".nordlyspreset", nordlys::factoryPresets(),
                                                        Array<Identifier> { Identifier ("uiPage") });
    // Layer on/off switches are applied last when a preset loads, so no layer sounds with half-set values.
    presets->setGatingParameters ([] (const String& id) { return id.endsWith ("_on"); });
}

bool NordlysProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != AudioChannelSet::disabled())
        return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
}

void NordlysProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
}

double NordlysProcessor::getTailLengthSeconds() const
{
    return (double) pRevDecay->load() * 1.2 + 2.0;
}

void NordlysProcessor::readSettings (Settings& s) const
{
    for (int l = 0; l < numLayers; ++l)
    {
        const auto& p = layerPtrs[(size_t) l];
        auto& L = s.layers[(size_t) l];
        L.on = p.on->load() > 0.5f;
        L.level = p.level->load() * 0.01f;
        L.pan = p.pan->load() * 0.01f;
        L.pitch = 12.0f * std::round (p.oct->load()) + std::round (p.coarse->load()) + p.fine->load() * 0.01f;
        L.osc = jlimit (0, oscTypeNames.size() - 1, (int) p.osc->load());
        L.wave = jlimit (0, waveNames.size() - 1, (int) p.wave->load());
        L.shape = p.shape->load() * 0.01f;
        L.sym = p.sym->load() * 0.01f;
        L.uni = jlimit (1, maxUnison, roundToInt (p.uni->load()));
        L.detune = p.detune->load();
        L.spread = p.spread->load() * 0.01f;
        L.gsizeMs = p.gsize->load();
        L.gdens = p.gdens->load();
        L.gspray = p.gspray->load() * 0.01f;
        L.gpitch = p.gpitch->load();
        L.gshim = p.gshim->load() * 0.01f;
        L.ftype = jlimit (0, filterTypeNames.size() - 1, (int) p.ftype->load());
        L.cutoff = p.cutoff->load();
        L.reso = p.reso->load() * 0.01f;
        L.fenv = p.fenv->load() * 0.01f;
        L.fkey = p.fkey->load() * 0.01f;
        L.drive = p.drive->load() * 0.01f;
        L.amp = { p.aA->load(), p.aD->load(), p.aS->load() * 0.01f, p.aR->load() };
        L.filt = { p.fA->load(), p.fD->load(), p.fS->load() * 0.01f, p.fR->load() };
        L.lfoRate[0] = p.l1rate->load(); L.lfoShape[0] = jlimit (0, 5, (int) p.l1shape->load()); L.lfoFade[0] = p.l1fade->load();
        L.lfoRate[1] = p.l2rate->load(); L.lfoShape[1] = jlimit (0, 5, (int) p.l2shape->load()); L.lfoFade[1] = p.l2fade->load();
    }
    for (int m = 0; m < numSlots; ++m)
    {
        const auto& p = slotPtrs[(size_t) m];
        auto& S = s.slots[(size_t) m];
        S.src = jlimit (0, numSources - 1, (int) p.src->load());
        S.via = jlimit (0, numSources - 1, (int) p.via->load());
        S.dst = jlimit (0, numDests - 1, (int) p.dst->load());
        S.layer = jlimit (0, numLayers, (int) p.layer->load());
        S.amt = p.amt->load() * 0.01f;
    }
    s.masterDb = pMaster->load();
    s.polyMode = jlimit (0, 2, (int) pPolyMode->load());
    s.glideMs = pGlide->load();
    s.bendRange = std::round (pBend->load());
    s.velSens = pVelSens->load() * 0.01f;
    s.orbX = pOrbX->load() * 0.01f;
    s.orbY = pOrbY->load() * 0.01f;
    s.orbRate = pOrbRate->load();
    s.orbDepth = pOrbDepth->load() * 0.01f;
    s.arpOn = pArpOn->load() > 0.5f;
    s.arpMode = jlimit (0, arpModeNames.size() - 1, (int) pArpMode->load());
    s.arpRate = jlimit (0, arpRateNames.size() - 1, (int) pArpRate->load());
    s.arpOct = jlimit (1, 4, roundToInt (pArpOct->load()));
    s.arpGate = pArpGate->load() * 0.01f;
    s.choMix = pChoMix->load() * 0.01f;
    s.choRate = pChoRate->load();
    s.choDepth = pChoDepth->load() * 0.01f;
    s.dlyMix = pDlyMix->load() * 0.01f;
    s.dlyTime = jlimit (0, delayTimeNames.size() - 1, (int) pDlyTime->load());
    s.dlyFb = pDlyFb->load() * 0.01f;
    s.dlyTone = pDlyTone->load();
    s.revMix = pRevMix->load() * 0.01f;
    s.revType = jlimit (0, reverbTypeNames.size() - 1, (int) pRevType->load());
    s.revDecay = pRevDecay->load();
    s.revTone = pRevTone->load() * 0.01f;
}

void NordlysProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    HostTime host;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) host.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) { host.ppq = *ppq; host.hasPpq = true; }
            host.playing = pos->getIsPlaying();
        }

    readSettings (settings);
    engine.process (buffer, midi, settings, host);
}

//==============================================================================
int NordlysProcessor::getNumPrograms()                { return jmax (1, presets->getNumFactoryPresets()); }
const String NordlysProcessor::getProgramName (int i) { return presets->getFactoryPresetName (i); }

void NordlysProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, presets->getNumFactoryPresets()))
        return;
    currentProgram = i;
    presets->loadFactoryPreset (i);
}

void NordlysProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void NordlysProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* NordlysProcessor::createEditor() { return new NordlysEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NordlysProcessor(); }
