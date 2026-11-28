#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace juce;

namespace
{
    String dbText (float v, int)   { return String (v, 1) + " dB"; }
    String pctText (float v, int)  { return String (roundToInt (v)) + " %"; }
    String hzText (float v, int)   { return v < 1000.0f ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, 2) + " kHz"; }
    String msText (float v, int)   { return String (v, 2) + " ms"; }
    String secText (float v, int)  { return String (v, 2) + " s"; }
    String qText (float v, int)    { return String (v, 1); }

    NormalisableRange<float> skewed (float lo, float hi, float interval, float centre)
    {
        NormalisableRange<float> r (lo, hi, interval);
        r.setSkewForCentre (centre);
        return r;
    }

    const Identifier profileId ("noiseProfile");
    const Identifier profileBinsId ("noiseProfileBins");
}

AudioProcessorValueTreeState::ParameterLayout RenskeProcessor::createLayout()
{
    using namespace ParamIDs;
    using F = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto fl = [&ps] (const char* id, const char* name, NormalisableRange<float> r, float def,
                     String (*txt) (float, int), const char* label = "")
    {
        ps.push_back (std::make_unique<F> (ParameterID { id, 1 }, String (CharPointer_UTF8 (name)), r, def,
                                           A().withLabel (label).withStringFromValueFunction (txt)));
    };
    auto onOff = [&ps] (const char* id, const char* name, bool def = false)
    {
        ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, String (CharPointer_UTF8 (name)), def));
    };
    auto choice = [&ps] (const char* id, const char* name, StringArray items, int def)
    {
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, String (CharPointer_UTF8 (name)), items, def));
    };
    const NormalisableRange<float> pct (0.0f, 100.0f, 0.1f);

    // global
    fl (out, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, dbText, "dB");
    fl (mix, "Mix", pct, 100.0f, pctText, "%");
    onOff (delta, "Kun fjernet lyd");

    // de-clip
    onOff (dcOn, "De-clip");
    choice (dcMode, "De-clip modus", { "Auto", "Manuell" }, 0);
    fl (dcThr, "De-clip terskel", NormalisableRange<float> (-20.0f, 0.0f, 0.1f), -0.5f, dbText, "dB");
    fl (dcGain, "De-clip gain", NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -3.0f, dbText, "dB");

    // click stage
    onOff (ckOn, "De-click");
    fl (ckSens, "De-click følsomhet", pct, 50.0f, pctText, "%");
    fl (ckWidth, "De-click bredde", skewed (0.2f, 10.0f, 0.01f, 2.0f), 2.0f, msText, "ms");
    onOff (crOn, "De-crackle");
    fl (crAmt, "De-crackle mengde", pct, 40.0f, pctText, "%");
    onOff (mcOn, "Munnklikk");
    fl (mcSens, "Munnklikk følsomhet", pct, 50.0f, pctText, "%");
    fl (mcWidth, "Munnklikk bredde", skewed (0.2f, 5.0f, 0.01f, 1.5f), 1.5f, msText, "ms");

    // de-hum
    onOff (hmOn, "De-hum");
    choice (hmBase, "De-hum grunntone", { "Auto", "50 Hz", "60 Hz" }, 0);
    ps.push_back (std::make_unique<AudioParameterInt> (ParameterID { hmHarm, 1 }, "De-hum overtoner", 1, 16, 6));
    fl (hmQ, "De-hum Q", skewed (4.0f, 100.0f, 0.1f, 20.0f), 20.0f, qText);
    fl (hmDepth, "De-hum dybde", NormalisableRange<float> (0.0f, 60.0f, 0.1f), 30.0f, dbText, "dB");
    fl (hmSlope, "De-hum diskant-slakk", pct, 30.0f, pctText, "%");
    onOff (hmHpf, "De-hum lavkutt");

    // de-plosive
    onOff (plOn, "De-plosive");
    fl (plSens, "De-plosive følsomhet", pct, 50.0f, pctText, "%");
    fl (plFreq, "De-plosive frekvens", skewed (50.0f, 400.0f, 1.0f, 150.0f), 150.0f, hzText, "Hz");
    fl (plMax, "De-plosive maks demping", NormalisableRange<float> (0.0f, 36.0f, 0.1f), 18.0f, dbText, "dB");

    // de-bleed
    onOff (blOn, "De-bleed");
    fl (blAmt, "De-bleed mengde", pct, 60.0f, pctText, "%");
    fl (blMax, "De-bleed maks demping", NormalisableRange<float> (0.0f, 40.0f, 0.1f), 20.0f, dbText, "dB");

    // de-noise
    onOff (dnOn, "De-noise");
    choice (dnSrc, "De-noise kilde", { "Adaptiv", "Lært profil" }, 0);
    choice (dnType, "De-noise type", { "Generell", "Tale", "Gitar" }, 0);
    fl (dnRed, "De-noise demping", NormalisableRange<float> (0.0f, 40.0f, 0.1f), 12.0f, dbText, "dB");
    fl (dnThr, "De-noise terskel", NormalisableRange<float> (-10.0f, 20.0f, 0.1f), 0.0f, dbText, "dB");
    fl (dnSmooth, "De-noise glatting", pct, 40.0f, pctText, "%");

    // dialogue isolate
    onOff (isOn, "Dialog-isolering");
    fl (isAmt, "Isolering separasjon", pct, 60.0f, pctText, "%");
    fl (isBg, "Isolering bakgrunn", NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -30.0f, dbText, "dB");

    // de-reverb
    onOff (rvOn, "De-reverb");
    fl (rvAmt, "De-reverb mengde", pct, 50.0f, pctText, "%");
    fl (rvTail, "De-reverb etterklang", skewed (0.2f, 5.0f, 0.01f, 1.0f), 1.0f, secText, "s");

    // breath control
    onOff (brOn, "Pustekontroll");
    fl (brSens, "Pust følsomhet", pct, 50.0f, pctText, "%");
    fl (brGain, "Pust nivå", NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -15.0f, dbText, "dB");

    // de-ess
    onOff (dsOn, "De-ess");
    choice (dsMode, "De-ess modus", { "Klassisk", "Spektral" }, 1);
    fl (dsThr, "De-ess terskel", NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -30.0f, dbText, "dB");
    fl (dsFreq, "De-ess frekvens", skewed (2000.0f, 12000.0f, 1.0f, 5500.0f), 5500.0f, hzText, "Hz");
    fl (dsMax, "De-ess maks demping", NormalisableRange<float> (0.0f, 24.0f, 0.1f), 12.0f, dbText, "dB");

    // rebalance
    onOff (rbOn, "Rebalanse");
    const NormalisableRange<float> rbRange (-24.0f, 12.0f, 0.1f);
    fl (rbVox, "Rebalanse vokal", rbRange, 0.0f, dbText, "dB");
    fl (rbBass, "Rebalanse bass", rbRange, 0.0f, dbText, "dB");
    fl (rbPerc, "Rebalanse perkusjon", rbRange, 0.0f, dbText, "dB");
    fl (rbOther, "Rebalanse resten", rbRange, 0.0f, dbText, "dB");
    fl (rbSep, "Rebalanse separasjon", pct, 50.0f, pctText, "%");

    return { ps.begin(), ps.end() };
}

RenskeProcessor::RenskeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "Renske", createLayout())
{
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
            params[r->getParameterID()] = apvts.getRawParameterValue (r->getParameterID());

    presets = std::make_unique<sbaudio::PresetManager> (apvts, "Renske", ".renskepreset",
                                                        renske::factoryPresets(),
                                                        Array<Identifier> { profileId, profileBinsId });
}

bool RenskeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out || (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()))
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != AudioChannelSet::mono() && sc != AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void RenskeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int nch = jmax (1, getMainBusNumOutputChannels());
    clickStage.prepare (sr, nch);
    humStage.prepare (sr, nch);
    plosiveStage.prepare (sr, nch);
    spectral.prepare (sr, nch);
    latency = clickStage.getLatency() + spectral.getLatency();
    dryDelay.prepare (latency, nch);
    dry.setSize (2, jmax (1, samplesPerBlock), false, false, true);
    setLatencySamples (latency);

    outGain.reset (sr, 0.05);   outGain.setCurrentAndTargetValue (renske::dbToGain (p (ParamIDs::out)));
    mixGain.reset (sr, 0.05);   mixGain.setCurrentAndTargetValue (p (ParamIDs::mix) * 0.01f);
    deltaGain.reset (sr, 0.03); deltaGain.setCurrentAndTargetValue (p (ParamIDs::delta) > 0.5f ? 1.0f : 0.0f);

    // a learned profile is only valid for the FFT size it was learned at
    {
        const SpinLock::ScopedLockType sl (profileLock);
        if (! pendingProfile.empty() && (int) pendingProfile.size() == spectral.getFftSize() / 2 + 1)
            spectral.setProfile (pendingProfile);
        else if (! savedProfile.empty() && (int) savedProfile.size() == spectral.getFftSize() / 2 + 1)
            spectral.setProfile (savedProfile);
        pendingProfileFlag = false;
        profileAvailable = spectral.hasProfile();
    }
    seenProfileVersion = shared.profileVersion.load();
}

renske::Settings RenskeProcessor::readSettings() const
{
    using namespace ParamIDs;
    auto on = [this] (const char* id) { return p (id) > 0.5f; };
    renske::Settings s;
    s.declipOn = on (dcOn);  s.declipMode = (int) p (dcMode);  s.declipThrDb = p (dcThr);  s.declipGainDb = p (dcGain);
    s.clickOn = on (ckOn);   s.clickSens = p (ckSens);  s.clickWidthMs = p (ckWidth);
    s.crackleOn = on (crOn); s.crackleAmount = p (crAmt);
    s.mouthOn = on (mcOn);   s.mouthSens = p (mcSens);  s.mouthWidthMs = p (mcWidth);
    s.humOn = on (hmOn);     s.humBase = (int) p (hmBase);  s.humHarmonics = (int) p (hmHarm);  s.humQ = p (hmQ);
    s.humDepthDb = p (hmDepth);  s.humSlope = p (hmSlope);  s.humHpf = on (hmHpf);
    s.plosiveOn = on (plOn); s.plosiveSens = p (plSens);  s.plosiveFreq = p (plFreq);  s.plosiveMaxDb = p (plMax);
    s.bleedOn = on (blOn);   s.bleedAmount = p (blAmt);  s.bleedMaxDb = p (blMax);
    s.denoiseOn = on (dnOn); s.denoiseSource = (int) p (dnSrc);  s.denoiseType = (int) p (dnType);
    s.denoiseRedDb = p (dnRed);  s.denoiseThrDb = p (dnThr);  s.denoiseSmooth = p (dnSmooth);
    s.isolateOn = on (isOn); s.isolateAmount = p (isAmt);  s.isolateBgDb = p (isBg);
    s.reverbOn = on (rvOn);  s.reverbAmount = p (rvAmt);  s.reverbTail = p (rvTail);
    s.breathOn = on (brOn);  s.breathSens = p (brSens);  s.breathGainDb = p (brGain);
    s.essOn = on (dsOn);     s.essMode = (int) p (dsMode);  s.essThrDb = p (dsThr);  s.essFreq = p (dsFreq);  s.essMaxDb = p (dsMax);
    s.rebalOn = on (rbOn);   s.rebalVox = p (rbVox);  s.rebalBass = p (rbBass);  s.rebalPerc = p (rbPerc);
    s.rebalOther = p (rbOther);  s.rebalSep = p (rbSep);
    return s;
}

void RenskeProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    auto main = getBusBuffer (buffer, false, 0);
    const int n = main.getNumSamples(), nch = jmin (2, main.getNumChannels());
    if (n == 0 || nch == 0) return;

    // sidechain (De-bleed reference)
    const float* side[2] = { nullptr, nullptr };
    int numSide = 0;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        numSide = jmin (2, sc.getNumChannels());
        for (int c = 0; c < numSide; ++c) side[c] = sc.getReadPointer (c);
        float scPeak = 0.0f;
        for (int c = 0; c < numSide; ++c) scPeak = jmax (scPeak, sc.getMagnitude (c, 0, n));
        sidechainActive = scPeak > 1.0e-5f;
    }
    else sidechainActive = false;

    // learned profile from a loaded project/preset
    if (pendingProfileFlag.load())
    {
        const SpinLock::ScopedTryLockType sl (profileLock);
        if (sl.isLocked())
        {
            if ((int) pendingProfile.size() == spectral.getFftSize() / 2 + 1)
                spectral.setProfile (pendingProfile);
            pendingProfileFlag = false;
            profileAvailable = spectral.hasProfile();
        }
    }

    const auto s = readSettings();
    const bool analysing = shared.analyse.load() != 0;

    float inPk = 0.0f;
    for (int c = 0; c < nch; ++c) inPk = jmax (inPk, main.getMagnitude (c, 0, n));
    renske::storeMax (meterIn, inPk);
    if (analysing)
    {
        shared.anSeconds.store (shared.anSeconds.load() + (float) n / (float) sr);
        renske::storeMax (shared.anPeak, inPk);
        if (shared.anSeconds.load() >= analysisSeconds) shared.analyse = 0;
    }

    // dry copy (delayed by the full latency)
    if (dry.getNumSamples() < n) dry.setSize (2, n, false, false, true);
    for (int c = 0; c < nch; ++c) dry.copyFrom (c, 0, main, c, 0, n);
    for (int i = 0; i < n; ++i)
    {
        for (int c = 0; c < nch; ++c) dry.setSample (c, i, dryDelay.process (c, dry.getSample (c, i)));
        dryDelay.advance();
    }

    // ---- chain ----
    clickStage.process (main, s, shared);
    if (s.declipOn)
        main.applyGain (renske::dbToGain (s.declipGainDb));
    humStage.process (main, s, shared);
    plosiveStage.process (main, s, shared);
    spectral.process (main, side, numSide, s, shared);

    // new learned profile -> keep a copy for the state
    if (const int v = shared.profileVersion.load(); v != seenProfileVersion)
    {
        const SpinLock::ScopedTryLockType sl (profileLock);
        if (sl.isLocked())
        {
            seenProfileVersion = v;
            savedProfile = spectral.getProfile();
            profileAvailable = spectral.hasProfile();
        }
    }

    // ---- mix / delta / output ----
    outGain.setTargetValue (renske::dbToGain (p (ParamIDs::out)));
    mixGain.setTargetValue (p (ParamIDs::mix) * 0.01f);
    deltaGain.setTargetValue (p (ParamIDs::delta) > 0.5f ? 1.0f : 0.0f);
    float outPk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue(), m = mixGain.getNextValue(), dl = deltaGain.getNextValue();
        for (int c = 0; c < nch; ++c)
        {
            const float d = dry.getSample (c, i), w = main.getSample (c, i);
            const float mixed = d + m * (w - d);
            const float removed = d - w;
            const float y = (mixed + dl * (removed - mixed)) * g;
            main.setSample (c, i, y);
            outPk = jmax (outPk, std::abs (y));
        }
    }
    renske::storeMax (meterOut, outPk);
}

//==============================================================================
void RenskeProcessor::startAnalysis()
{
    shared.resetAnalysis();
    shared.analyse = 1;
}

std::vector<renske::Suggestion> RenskeProcessor::makeSuggestions() const
{
    using namespace ParamIDs;
    std::vector<renske::Suggestion> list;
    const auto& a = shared;
    const float secs = jmax (0.5f, a.anSeconds.load());
    const int frames = jmax (1, a.anFrames.load());
    auto txt = [] (const char* t) { return String (CharPointer_UTF8 (t)); };

    if (a.anPeak.load() < 1.0e-4f)
    {
        list.push_back ({ txt ("Fant nesten ikke lyd. Spill av et parti med typisk innhold mens analysen går."), {} });
        return list;
    }

    const float clipsPerS = (float) a.anClips.load() / secs;
    if (clipsPerS > 0.3f)
        list.push_back ({ txt ("Klipping: ") + String (clipsPerS, 1) + txt ("/s avkuttede topper. Slår på De-clip (auto)."),
                         { { dcOn, 1 }, { dcMode, 0 }, { dcGain, clipsPerS > 5 ? -6.0f : -3.0f } } });

    const float clicksPerS = (float) a.anClicks.load() / secs;
    const float cracklePerS = (float) a.anCrackle.load() / secs;
    const float mouthPerS = (float) a.anMouth.load() / secs;
    if (cracklePerS > 40.0f)
        list.push_back ({ txt ("Knitring: mange små klikk (") + String (roundToInt (cracklePerS)) + txt ("/s). Slår på De-crackle."),
                         { { crOn, 1 }, { crAmt, cracklePerS > 150 ? 60.0f : 40.0f } } });
    if (clicksPerS > 0.5f)
        list.push_back ({ txt ("Klikk: ") + String (clicksPerS, 1) + txt ("/s. Slår på De-click."),
                         { { ckOn, 1 }, { ckSens, clicksPerS > 5 ? 60.0f : 45.0f }, { ckWidth, 2.0f } } });
    if (mouthPerS > 1.0f && clicksPerS < 8.0f)
        list.push_back ({ txt ("Munnlyder: ") + String (mouthPerS, 1) + txt ("/s korte, lyse klikk. Slår på Munnklikk."),
                         { { mcOn, 1 }, { mcSens, 45.0f } } });

    const float humDb = a.anHumDb.load();
    if (humDb > 9.0f)
    {
        const float f = a.anHumFreq.load();
        list.push_back ({ txt ("Brumm rundt ") + String (f, 1) + txt (" Hz (") + String (roundToInt (humDb)) + txt (" dB over omgivelsene). Slår på De-hum."),
                         { { hmOn, 1 }, { hmBase, 0 }, { hmHarm, humDb > 20 ? 10.0f : 6.0f },
                           { hmDepth, jlimit (15.0f, 50.0f, humDb + 6.0f) } } });
    }

    const float plosPerS = (float) a.anPlosives.load() / secs;
    if (plosPerS > 0.08f)
        list.push_back ({ txt ("Popp/vind: ") + String (a.anPlosives.load()) + txt (" basstøt. Slår på De-plosive."),
                         { { plOn, 1 }, { plSens, 50.0f } } });

    const float noiseDb = a.anNoiseDb.load(), sigDb = a.anSignalDb.load();
    const float snr = sigDb - noiseDb;
    if (noiseDb > -78.0f && snr < 55.0f)
    {
        const float red = jlimit (6.0f, 20.0f, (55.0f - snr) * 0.45f + 6.0f);
        list.push_back ({ txt ("Støy: grunnstøy ca. ") + String (roundToInt (noiseDb)) + txt (" dBFS, ") + String (roundToInt (snr))
                             + txt (" dB under signalet. Slår på De-noise (adaptiv)."),
                         { { dnOn, 1 }, { dnSrc, 0 }, { dnRed, red }, { dnThr, 0.0f } } });
    }

    const float rev = a.anReverb.load();
    if (rev > 0.45f)
        list.push_back ({ txt ("Romklang: tydelig etterklang i opptaket. Slår på De-reverb."),
                         { { rvOn, 1 }, { rvAmt, jlimit (30.0f, 70.0f, rev * 90.0f) }, { rvTail, rev > 0.7f ? 1.6f : 0.9f } } });

    const float breathShare = (float) a.anBreaths.load() / (float) frames;
    if (breathShare > 0.02f)
        list.push_back ({ txt ("Pust: hørbare innpust mellom frasene. Slår på Pustekontroll (-12 dB)."),
                         { { brOn, 1 }, { brSens, 50.0f }, { brGain, -12.0f } } });

    const float sibShare = (float) a.anSibFrames.load() / (float) frames;
    if (sibShare > 0.04f)
        list.push_back ({ txt ("Sibilanter: skarpe S-lyder. Slår på De-ess."),
                         { { dsOn, 1 }, { dsMode, 1 }, { dsThr, jlimit (-50.0f, -10.0f, a.anSibDb.load() - 8.0f) } } });

    if (list.empty())
        list.push_back ({ txt ("Fant ingen tydelige problemer. Opptaket ser rent ut."), {} });
    return list;
}

void RenskeProcessor::setParam (const String& id, float plainValue)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
        prm->endChangeGesture();
    }
}

void RenskeProcessor::applySuggestions (const std::vector<renske::Suggestion>& list)
{
    // the assistant builds a complete chain: modules it does not suggest are switched off
    std::set<String> suggested;
    for (auto& s : list) for (auto& [id, v] : s.values) suggested.insert (id);
    for (auto& m : renske::modules())
        if (suggested.count (m.onId) == 0) setParam (m.onId, 0.0f);
    for (auto& s : list)
        for (auto& [id, v] : s.values) setParam (id, v);
}

//==============================================================================
int RenskeProcessor::getNumPrograms()                { return jmax (1, presets->getNumFactoryPresets()); }
const String RenskeProcessor::getProgramName (int i) { return presets->getFactoryPresetName (i); }

void RenskeProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, presets->getNumFactoryPresets()))
        return;
    currentProgram = i;
    presets->loadFactoryPreset (i);
}

void RenskeProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    {
        const SpinLock::ScopedLockType sl (profileLock);
        if (! savedProfile.empty())
        {
            MemoryBlock mb (savedProfile.data(), savedProfile.size() * sizeof (float));
            state.setProperty (profileId, mb.toBase64Encoding(), nullptr);
            state.setProperty (profileBinsId, (int) savedProfile.size(), nullptr);
        }
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RenskeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto tree = ValueTree::fromXml (*xml);
    apvts.replaceState (tree);

    if (tree.hasProperty (profileId))
    {
        MemoryBlock mb;
        const int bins = tree[profileBinsId];
        if (mb.fromBase64Encoding (tree[profileId].toString()) && bins > 0 && (int) mb.getSize() == bins * (int) sizeof (float))
        {
            const SpinLock::ScopedLockType sl (profileLock);
            pendingProfile.assign ((const float*) mb.getData(), (const float*) mb.getData() + bins);
            savedProfile = pendingProfile;
            pendingProfileFlag = true;
        }
    }
}

AudioProcessorEditor* RenskeProcessor::createEditor() { return new RenskeEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RenskeProcessor(); }
