#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetManager.h"

using namespace juce;
using namespace prism;

namespace
{
    enum BandParamIndex { pUsed = 0, pOn, pType, pFreq, pGain, pQ, pSlope, pPlace, pDyn, pRange, pThr, pAtt, pRel, pSc };

    NormalisableRange<float> logRange (float lo, float hi)
    {
        return { lo, hi,
                 [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                 [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                 [] (float a, float b, float v) { return jlimit (a, b, v); } };
    }

    String freqText (float f, int)
    {
        if (f >= 1000.0f)
            return String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz";
        return String (f, f < 100.0f ? 1 : 0) + " Hz";
    }

    float freqFromText (const String& s)
    {
        auto t = s.trim().toLowerCase();
        float v = t.getFloatValue();
        if (t.containsChar ('k'))
            v *= 1000.0f;
        return v;
    }

    String dbText (float v, int)
    {
        if (std::abs (v) < 0.05f) v = 0.0f;
        return (v > 0.0f ? "+" : "") + String (v, 1) + " dB";
    }
    String msText (float v, int)   { return v < 10.0f ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms"; }
    String qText (float v, int)    { return String (v, v < 10.0f ? 2 : 1); }

    float kneeOver (float over, float width)
    {
        if (over <= -width * 0.5f) return 0.0f;
        if (over >= width * 0.5f)  return over;
        const float t = over + width * 0.5f;
        return t * t / (2.0f * width);
    }
}

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout PrismEQProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    using APF = AudioParameterFloat;
    using APC = AudioParameterChoice;
    using APB = AudioParameterBool;
    using Attr = AudioParameterFloatAttributes;

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::output, 1 }, "Output Gain",
                                       NormalisableRange<float> (-36.0f, 36.0f, 0.01f), 0.0f,
                                       Attr().withStringFromValueFunction (dbText).withLabel ("dB")));
    layout.add (std::make_unique<APB> (ParameterID { ParamIDs::autoGain, 1 }, "Auto Gain", false));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::gainScale, 1 }, "Gain Scale",
                                       NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f,
                                       Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; })
                                             .withLabel ("%")));
    layout.add (std::make_unique<APC> (ParameterID { ParamIDs::mode, 1 }, "Processing Mode",
                                       StringArray { "Zero Latency", "Linear Phase" }, 0));
    layout.add (std::make_unique<APC> (ParameterID { ParamIDs::character, 1 }, "Character",
                                       StringArray { "Clean", "Subtle", "Warm" }, 0));
    layout.add (std::make_unique<APB> (ParameterID { ParamIDs::invert, 1 }, "Phase Invert", false));

    for (int b = 0; b < numBands; ++b)
    {
        const String n = "Band " + String (b + 1) + " ";
        auto id = [b] (const char* name) { return ParameterID { ParamIDs::band (b, name), 1 }; };
        auto group = std::make_unique<AudioProcessorParameterGroup> ("band" + String (b + 1), "Band " + String (b + 1), "|");

        group->addChild (std::make_unique<APB> (id ("used"), n + "Used", false));
        group->addChild (std::make_unique<APB> (id ("on"), n + "Enabled", true));
        group->addChild (std::make_unique<APC> (id ("type"), n + "Shape", typeNames, Bell));
        group->addChild (std::make_unique<APF> (id ("freq"), n + "Frequency", logRange (10.0f, 30000.0f), 1000.0f,
                                                Attr().withStringFromValueFunction (freqText)
                                                      .withValueFromStringFunction (freqFromText).withLabel ("Hz")));
        group->addChild (std::make_unique<APF> (id ("gain"), n + "Gain", NormalisableRange<float> (-30.0f, 30.0f, 0.01f), 0.0f,
                                                Attr().withStringFromValueFunction (dbText).withLabel ("dB")));
        group->addChild (std::make_unique<APF> (id ("q"), n + "Q", logRange (0.025f, 40.0f), 1.0f,
                                                Attr().withStringFromValueFunction (qText)));
        group->addChild (std::make_unique<APC> (id ("slope"), n + "Slope", slopeNames, 3));
        group->addChild (std::make_unique<APC> (id ("place"), n + "Stereo Placement", placementNames, Stereo));
        group->addChild (std::make_unique<APB> (id ("dyn"), n + "Dynamic", false));
        group->addChild (std::make_unique<APF> (id ("range"), n + "Dynamic Range", NormalisableRange<float> (-30.0f, 30.0f, 0.01f), -6.0f,
                                                Attr().withStringFromValueFunction (dbText).withLabel ("dB")));
        group->addChild (std::make_unique<APF> (id ("thr"), n + "Threshold", NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -30.0f,
                                                Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })
                                                      .withLabel ("dB")));
        auto attR = logRange (0.1f, 200.0f);
        group->addChild (std::make_unique<APF> (id ("att"), n + "Attack", attR, 5.0f,
                                                Attr().withStringFromValueFunction (msText).withLabel ("ms")));
        group->addChild (std::make_unique<APF> (id ("rel"), n + "Release", logRange (5.0f, 2000.0f), 80.0f,
                                                Attr().withStringFromValueFunction (msText).withLabel ("ms")));
        group->addChild (std::make_unique<APB> (id ("sc"), n + "External Side Chain", false));
        layout.add (std::move (group));
    }
    return layout;
}

//==============================================================================
PrismEQProcessor::PrismEQProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PrismEQState", createLayout())
{
    pOutput    = apvts.getRawParameterValue (ParamIDs::output);
    pAutoGain  = apvts.getRawParameterValue (ParamIDs::autoGain);
    pGainScale = apvts.getRawParameterValue (ParamIDs::gainScale);
    pMode      = apvts.getRawParameterValue (ParamIDs::mode);
    pCharacter = apvts.getRawParameterValue (ParamIDs::character);
    pInvert    = apvts.getRawParameterValue (ParamIDs::invert);

    for (int b = 0; b < numBands; ++b)
        for (int i = 0; i < 14; ++i)
            bandPtrs[(size_t) b].p[i] = apvts.getRawParameterValue (ParamIDs::band (b, ParamIDs::bandParamNames[i]));

    for (auto& d : dynGainDb)
        d = 0.0f;

    presets = std::make_unique<PresetManager> (*this);
    startTimerHz (15);
}

PrismEQProcessor::~PrismEQProcessor()
{
    stopTimer();
}

bool PrismEQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    if (in != out)
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto& sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != AudioChannelSet::mono() && sc != AudioChannelSet::stereo())
            return false;
    }
    return true;
}

//==============================================================================
int PrismEQProcessor::linearFirLength() const
{
    const double fs = getSampleRateSafe();
    return fs <= 50000.0 ? 8192 : (fs <= 100000.0 ? 16384 : 32768);
}

void PrismEQProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    maxBlock = jmax (1, samplesPerBlock);
    const int nCh = 2;

    dryBuffer.setSize (nCh, maxBlock);
    scBuffer.setSize (1, maxBlock);
    monoScratch.assign ((size_t) maxBlock, 0.0f);

    linearLatency = linearFirLength() / 2;
    delayBuf.setSize (nCh, linearLatency);
    delayBuf.clear();
    delayWrite = 0;

    for (auto& r : rt)
    {
        r.init = false;
        r.mix = 0.0f;
        r.env = 0.0f;
        r.dynDb = 0.0f;
    }
    soloSt = {};
    soloMix = 0.0f;
    dcX = {}; dcY = {};

    outGainSm.reset (sampleRate, 0.05);
    outGainSm.setCurrentAndTargetValue (Decibels::decibelsToGain (pOutput->load()));

    convolution.prepare ({ sampleRate, (uint32) maxBlock, (uint32) nCh });
    convolution.reset();
    lastLinearHash = 0;
    linearReady = false;
    rebuildLinearPhase();

    const int lat = pMode->load() > 0.5f ? linearLatency : 0;
    setLatencySamples (lat);
    lastReportedLatency = lat;
}

prism::BandState PrismEQProcessor::getBandState (int b) const
{
    const auto& p = bandPtrs[(size_t) b];
    BandState s;
    s.used  = p.get (pUsed) > 0.5f;
    s.on    = p.get (pOn) > 0.5f;
    s.type  = jlimit (0, numTypes - 1, roundToInt (p.get (pType)));
    s.freq  = p.get (pFreq);
    s.gain  = p.get (pGain);
    s.q     = p.get (pQ);
    s.slope = jlimit (0, 8, roundToInt (p.get (pSlope)));
    s.place = jlimit (0, 4, roundToInt (p.get (pPlace)));
    s.dyn   = p.get (pDyn) > 0.5f;
    s.range = p.get (pRange);
    s.thr   = p.get (pThr);
    s.att   = p.get (pAtt);
    s.rel   = p.get (pRel);
    s.sc    = p.get (pSc) > 0.5f;
    return s;
}

//==============================================================================
void PrismEQProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    auto main = getBusBuffer (buffer, true, 0);
    const int numCh = jmin (2, main.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0)
        return;

    if (n > maxBlock) // host broke its promise; grow (not realtime-safe, but rare)
    {
        maxBlock = n;
        dryBuffer.setSize (2, n, false, false, true);
        scBuffer.setSize (1, n, false, false, true);
        monoScratch.resize ((size_t) n);
    }

    const double fs = getSampleRateSafe();
    const bool stereo = numCh == 2;

    // ---- side chain ---------------------------------------------------------
    bool scOn = false;
    if (auto* scBus = getBus (true, 1))
    {
        if (scBus->isEnabled() && scBus->getNumberOfChannels() > 0)
        {
            auto sc = getBusBuffer (buffer, true, 1);
            const int sch = sc.getNumChannels();
            scBuffer.copyFrom (0, 0, sc, 0, 0, n);
            if (sch > 1)
            {
                scBuffer.addFrom (0, 0, sc, 1, 0, n);
                scBuffer.applyGain (0, 0, n, 0.5f);
            }
            scOn = scBuffer.getMagnitude (0, 0, n) > 1.0e-6f;
        }
    }
    sidechainActive = scOn;
    if (! scOn)
        scBuffer.clear (0, 0, n);

    // ---- analyzer (pre) and dry copy ------------------------------------------
    for (int i = 0; i < n; ++i)
        monoScratch[(size_t) i] = stereo ? 0.5f * (main.getSample (0, i) + main.getSample (1, i)) : main.getSample (0, i);
    preRing.push (monoScratch.data(), n);

    for (int ch = 0; ch < numCh; ++ch)
        dryBuffer.copyFrom (ch, 0, main, ch, 0, n);

    // ---- linear phase stage -------------------------------------------------------
    const bool linear = pMode->load() > 0.5f && linearReady.load();
    if (linear)
    {
        // Delay the dry copy so detection and solo line up with the delayed audio.
        const int L = delayBuf.getNumSamples();
        if (L > 0)
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = dryBuffer.getWritePointer (ch);
                auto* r = delayBuf.getWritePointer (ch);
                int w = delayWrite;
                for (int i = 0; i < n; ++i)
                {
                    const float t = r[w];
                    r[w] = d[i];
                    d[i] = t;
                    if (++w >= L) w = 0;
                }
            }
            delayWrite = (delayWrite + n) % L;
        }

        if (stereo)
        {
            auto* l = main.getWritePointer (0);
            auto* r = main.getWritePointer (1);
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (l[i] + r[i]), s = 0.5f * (l[i] - r[i]);
                l[i] = m; r[i] = s;
            }
        }
        dsp::AudioBlock<float> block (main.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
        convolution.process (dsp::ProcessContextReplacing<float> (block));
        if (stereo)
        {
            auto* l = main.getWritePointer (0);
            auto* r = main.getWritePointer (1);
            for (int i = 0; i < n; ++i)
            {
                const float m = l[i], s = r[i];
                l[i] = m + s; r[i] = m - s;
            }
        }
    }

    // ---- IIR bands, in sub-blocks ----------------------------------------------
    const float gs = pGainScale->load() / 100.0f;
    const int solo = soloBand.load();

    for (int start = 0; start < n; start += subBlock)
    {
        const int len = jmin (subBlock, n - start);
        const float smoothA = 1.0f - std::exp (-(float) len / (0.03f * (float) fs));
        const float mixStep = (float) (1.0 / (0.02 * fs));

        // Two passes: L/R-domain bands first, then mid/side bands (filters commute).
        for (int pass = 0; pass < 2; ++pass)
        {
            bool encoded = false;

            for (int b = 0; b < numBands; ++b)
            {
                auto& r = rt[(size_t) b];
                const auto bs = getBandState (b);

                if (! bs.used)
                {
                    r.init = false; r.mix = 0.0f;
                    if (pass == 0) dynGainDb[(size_t) b] = 0.0f;
                    continue;
                }

                const bool msBand = stereo && (bs.place == Mid || bs.place == Side);
                if ((pass == 1) != msBand)
                    continue;

                const bool dynOn = bs.dyn && typeCanDynamic (bs.type);
                const bool inLinear = linear && ! dynOn && (bs.place == Stereo || bs.place == Mid || bs.place == Side);
                const float targetMix = (bs.on && ! inLinear) ? 1.0f : 0.0f;

                if (r.mix <= 0.0f && targetMix <= 0.0f)
                {
                    r.init = false;
                    dynGainDb[(size_t) b] = 0.0f;
                    continue;
                }

                if (! r.init)
                {
                    r.sFreq = bs.freq; r.sGain = bs.gain; r.sQ = bs.q;
                    r.st = {}; r.detectSt = {};
                    r.env = 0.0f; r.dynDb = 0.0f;
                    r.lastType = -1;
                    r.init = true;
                }
                else
                {
                    r.sFreq = std::exp (std::log (r.sFreq) + smoothA * (std::log (bs.freq) - std::log (r.sFreq)));
                    r.sQ    = std::exp (std::log (r.sQ) + smoothA * (std::log (bs.q) - std::log (r.sQ)));
                    r.sGain += smoothA * (bs.gain - r.sGain);
                }

                // ---- dynamics ----
                float dyn = 0.0f;
                if (dynOn)
                {
                    designListenFilter (r.detect, bs.type, r.sFreq, r.sQ, fs);
                    const float ca = std::exp (-1.0f / (jmax (0.05f, bs.att) * 0.001f * (float) fs));
                    const float cr = std::exp (-1.0f / (jmax (1.0f, bs.rel) * 0.001f * (float) fs));
                    const bool useSc = bs.sc && scOn;
                    const float* d0 = dryBuffer.getReadPointer (0, start);
                    const float* d1 = stereo ? dryBuffer.getReadPointer (1, start) : d0;
                    const float* scp = scBuffer.getReadPointer (0, start);
                    float env = r.env;
                    for (int i = 0; i < len; ++i)
                    {
                        float x;
                        if (useSc)                     x = scp[i];
                        else if (! stereo)             x = d0[i];
                        else switch (bs.place)
                        {
                            case Left:  x = d0[i]; break;
                            case Right: x = d1[i]; break;
                            case Side:  x = 0.5f * (d0[i] - d1[i]); break;
                            default:    x = 0.5f * (d0[i] + d1[i]); break;
                        }
                        const float v = std::abs (processSection (r.detect, r.detectSt, x));
                        env = v > env ? v + (env - v) * ca : v + (env - v) * cr;
                    }
                    r.env = env;
                    const float levelDb = Decibels::gainToDecibels (env, -120.0f);
                    const float over = kneeOver (levelDb - bs.thr, 6.0f);
                    const float amount = jmin (std::abs (bs.range), over * 0.7f);
                    dyn = bs.range < 0.0f ? -amount : amount;
                }
                r.dynDb += (dyn - r.dynDb) * jmin (1.0f, (float) len / (0.002f * (float) fs));
                if (! dynOn) r.dynDb = 0.0f;
                dynGainDb[(size_t) b] = r.dynDb * r.mix;

                // ---- coefficients ----
                const double gEff = typeHasGain (bs.type) ? (double) r.sGain * gs + r.dynDb : 0.0;
                if (bs.type != r.lastType || bs.slope != r.lastSlope
                    || std::abs (r.sFreq - r.lastF) > 1.0e-4 * r.lastF
                    || std::abs (gEff - r.lastG) > 1.0e-4 || std::abs (r.sQ - r.lastQ) > 1.0e-5 * r.lastQ)
                {
                    if (bs.type != r.lastType || bs.slope != r.lastSlope)
                        r.st = {};
                    designBand (r.design, bs.type, r.sFreq, gEff, r.sQ, bs.slope, fs);
                    r.lastType = bs.type; r.lastSlope = bs.slope;
                    r.lastF = r.sFreq; r.lastG = gEff; r.lastQ = r.sQ;
                }

                // ---- channel routing ----
                if (msBand && ! encoded)
                {
                    auto* l = main.getWritePointer (0, start);
                    auto* rr = main.getWritePointer (1, start);
                    for (int i = 0; i < len; ++i)
                    {
                        const float m = 0.5f * (l[i] + rr[i]), s = 0.5f * (l[i] - rr[i]);
                        l[i] = m; rr[i] = s;
                    }
                    encoded = true;
                }

                bool chanOn[2] = { false, false };
                if (! stereo)
                    chanOn[0] = bs.place != Side;
                else switch (bs.place)
                {
                    case Left:  chanOn[0] = true; break;
                    case Right: chanOn[1] = true; break;
                    case Mid:   chanOn[0] = true; break;
                    case Side:  chanOn[1] = true; break;
                    default:    chanOn[0] = chanOn[1] = true; break;
                }

                const auto& des = r.design;
                const float startMix = r.mix;
                float endMix = startMix;
                for (int ch = 0; ch < numCh; ++ch)
                {
                    if (! chanOn[ch])
                        continue;
                    auto* x = main.getWritePointer (ch, start);
                    auto& st = r.st[(size_t) ch];
                    float m = startMix;
                    for (int i = 0; i < len; ++i)
                    {
                        float y = x[i];
                        for (int s = 0; s < des.num; ++s)
                            y = processSection (des.sec[(size_t) s], st[(size_t) s], y);
                        y *= (float) des.outGain;
                        if (m != targetMix)
                            m = targetMix > m ? jmin (targetMix, m + mixStep) : jmax (targetMix, m - mixStep);
                        x[i] = m >= 1.0f ? y : x[i] + m * (y - x[i]);
                    }
                    endMix = m;
                }
                r.mix = endMix;
                if (! chanOn[0] && ! chanOn[1])
                    r.mix = targetMix;
            }

            if (encoded)
            {
                auto* l = main.getWritePointer (0, start);
                auto* rr = main.getWritePointer (1, start);
                for (int i = 0; i < len; ++i)
                {
                    const float m = l[i], s = rr[i];
                    l[i] = m + s; rr[i] = m - s;
                }
            }
        }

        // ---- solo (listen to the region a band works on) ----
        const bool soloValid = solo >= 0 && solo < numBands && getBandState (solo).used;
        if (soloValid || soloMix > 0.0f)
        {
            if (soloValid)
            {
                const auto sb = getBandState (solo);
                if (solo != lastSolo)
                {
                    soloSt = {};
                    lastSolo = solo;
                }
                designListenFilter (soloFilter, sb.type, sb.freq, sb.q, fs);
            }
            const float target = soloValid ? 1.0f : 0.0f;
            float m = soloMix;
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* x = main.getWritePointer (ch, start);
                const auto* d = dryBuffer.getReadPointer (ch, start);
                m = soloMix;
                for (int i = 0; i < len; ++i)
                {
                    const float s = processSection (soloFilter, soloSt[(size_t) ch], d[i]);
                    m = target > m ? jmin (1.0f, m + mixStep) : jmax (0.0f, m - mixStep);
                    x[i] += m * (s - x[i]);
                }
            }
            soloMix = m;
            if (! soloValid && soloMix <= 0.0f)
                lastSolo = -1;
        }
    }

    // ---- output stage: character, gain, polarity --------------------------------
    const int character = roundToInt (pCharacter->load());
    if (character > 0)
    {
        const float drive = character == 1 ? 0.7f : 1.6f;
        const float bias  = character == 1 ? 0.08f : 0.22f;
        const float tb = std::tanh (bias);
        const float norm = 1.0f / (drive * (1.0f - tb * tb));
        const float R = 1.0f - (float) (MathConstants<double>::twoPi * 8.0 / fs);
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* x = main.getWritePointer (ch);
            float x1 = dcX[(size_t) ch], y1 = dcY[(size_t) ch];
            for (int i = 0; i < n; ++i)
            {
                const float sat = (std::tanh (drive * x[i] + bias) - tb) * norm;
                const float y = sat - x1 + R * y1;  // DC blocker
                x1 = sat; y1 = y;
                x[i] = y;
            }
            dcX[(size_t) ch] = x1; dcY[(size_t) ch] = y1;
        }
    }

    float outDb = pOutput->load();
    if (pAutoGain->load() > 0.5f)
        outDb += autoGainDb.load();
    float gainTarget = Decibels::decibelsToGain (outDb);
    if (pInvert->load() > 0.5f)
        gainTarget = -gainTarget;
    outGainSm.setTargetValue (gainTarget);

    if (outGainSm.isSmoothing())
    {
        for (int i = 0; i < n; ++i)
        {
            const float g = outGainSm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
                main.getWritePointer (ch)[i] *= g;
        }
    }
    else
    {
        main.applyGain (outGainSm.getTargetValue());
    }

    // ---- meters & analyzer (post) --------------------------------------------------
    const float pkL = main.getMagnitude (0, 0, n);
    const float pkR = stereo ? main.getMagnitude (1, 0, n) : pkL;
    outPeakL = jmax (outPeakL.load(), pkL);
    outPeakR = jmax (outPeakR.load(), pkR);

    for (int i = 0; i < n; ++i)
        monoScratch[(size_t) i] = stereo ? 0.5f * (main.getSample (0, i) + main.getSample (1, i)) : main.getSample (0, i);
    postRing.push (monoScratch.data(), n);

    // Clear any extra output channels.
    for (int ch = numCh; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
}

//==============================================================================
double PrismEQProcessor::computeAutoGain() const
{
    // Weighted average of the static curve, centred on the mid range where loudness is judged.
    const double fs = getSampleRateSafe();
    const float gs = getGainScale();
    std::vector<BandDesign> designs;
    for (int b = 0; b < numBands; ++b)
    {
        const auto s = getBandState (b);
        if (! s.used || ! s.on)
            continue;
        BandDesign d;
        designBand (d, s.type, s.freq, typeHasGain (s.type) ? s.gain * gs : 0.0f, s.q, s.slope, fs);
        designs.push_back (d);
    }
    if (designs.empty())
        return 0.0;

    double sum = 0.0, wsum = 0.0;
    for (int i = 0; i < 96; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, i / 95.0);
        const double o = std::log2 (f / 1500.0) / 2.2;
        const double w = std::exp (-0.5 * o * o);
        double db = 0.0;
        for (auto& d : designs)
            db += d.magnitudeDb (f, fs);
        sum += w * jlimit (-40.0, 40.0, db);
        wsum += w;
    }
    return jlimit (-24.0, 24.0, -sum / wsum);
}

void PrismEQProcessor::rebuildLinearPhase()
{
    if (pMode->load() < 0.5f || sampleRateHz <= 0.0)
        return;

    const double fs = sampleRateHz;
    const float gs = getGainScale();

    // Hash of everything that affects the linear-phase part of the curve.
    juce::int64 hash = (juce::int64) fs * 31 + (juce::int64) (gs * 1000.0f);
    std::vector<std::pair<BandDesign, int>> lin; // design + placement
    for (int b = 0; b < numBands; ++b)
    {
        const auto s = getBandState (b);
        const bool dynOn = s.dyn && typeCanDynamic (s.type);
        if (! s.used || ! s.on || dynOn || s.place == Left || s.place == Right)
            continue;
        BandDesign d;
        designBand (d, s.type, s.freq, typeHasGain (s.type) ? s.gain * gs : 0.0f, s.q, s.slope, fs);
        lin.push_back ({ d, s.place });
        for (float v : { (float) b, (float) s.type, s.freq, s.gain, s.q, (float) s.slope, (float) s.place })
            hash = hash * 1000003 + (juce::int64) (v * 1000.0f);
    }

    if (hash == lastLinearHash && linearReady)
        return;
    lastLinearHash = hash;

    const int N = linearFirLength();
    const int order = roundToInt (std::log2 ((double) N));
    dsp::FFT fft (order);

    // Detect the inverse-FFT scaling of this backend once.
    std::vector<dsp::Complex<float>> in ((size_t) N), out ((size_t) N);
    std::fill (in.begin(), in.end(), dsp::Complex<float> (1.0f, 0.0f));
    fft.perform (in.data(), out.data(), true);
    const float scale = std::abs (out[0].real()) > 0.0f ? 1.0f / out[0].real() : 1.0f;

    AudioBuffer<float> ir (2, N);
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int k = 0; k <= N / 2; ++k)
        {
            const double f = (double) k * fs / N;
            double mag = 1.0;
            for (auto& [d, place] : lin)
            {
                const bool applies = place == Stereo || (ch == 0 && place == Mid) || (ch == 1 && place == Side);
                if (applies)
                    mag *= std::abs (d.response (jmax (f, 1.0), fs));
            }
            in[(size_t) k] = { (float) mag, 0.0f };
            if (k > 0 && k < N / 2)
                in[(size_t) (N - k)] = { (float) mag, 0.0f };
        }
        fft.perform (in.data(), out.data(), true);

        auto* dst = ir.getWritePointer (ch);
        for (int i = 0; i < N; ++i)
        {
            const int src = (i + N / 2) % N; // centre the zero-phase response
            const double t = (double) i / (N - 1);
            const double win = 0.35875 - 0.48829 * std::cos (2.0 * MathConstants<double>::pi * t)
                             + 0.14128 * std::cos (4.0 * MathConstants<double>::pi * t)
                             - 0.01168 * std::cos (6.0 * MathConstants<double>::pi * t);
            dst[i] = out[(size_t) src].real() * scale * (float) win;
        }
    }

    convolution.loadImpulseResponse (std::move (ir), fs, dsp::Convolution::Stereo::yes,
                                     dsp::Convolution::Trim::no, dsp::Convolution::Normalise::no);
    linearReady = true;
}

void PrismEQProcessor::timerCallback()
{
    autoGainDb = (float) computeAutoGain();
    rebuildLinearPhase();

    const int lat = (pMode->load() > 0.5f && sampleRateHz > 0.0) ? linearLatency : 0;
    if (lat != lastReportedLatency)
    {
        lastReportedLatency = lat;
        setLatencySamples (lat);
    }
}

//==============================================================================
RangedAudioParameter* PrismEQProcessor::bandParam (int band, const char* name) const
{
    return apvts.getParameter (ParamIDs::band (band, name));
}

void PrismEQProcessor::setBandParam (int band, const char* name, float plainValue)
{
    if (auto* p = bandParam (band, name))
    {
        const float nv = p->convertTo0to1 (plainValue);
        if (std::abs (p->getValue() - nv) < 1.0e-7f)
            return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (nv);
        p->endChangeGesture();
    }
}

int PrismEQProcessor::createBand (int type, float freq, float gain, float q)
{
    for (int b = 0; b < numBands; ++b)
    {
        if (getBandState (b).used)
            continue;
        setBandParam (b, "type", (float) type);
        setBandParam (b, "freq", freq);
        setBandParam (b, "gain", gain);
        setBandParam (b, "q", q);
        setBandParam (b, "slope", 3.0f);
        setBandParam (b, "place", 0.0f);
        setBandParam (b, "dyn", 0.0f);
        setBandParam (b, "sc", 0.0f);
        setBandParam (b, "range", -6.0f);
        setBandParam (b, "thr", -30.0f);
        setBandParam (b, "att", 5.0f);
        setBandParam (b, "rel", 80.0f);
        setBandParam (b, "on", 1.0f);
        setBandParam (b, "used", 1.0f);
        return b;
    }
    return -1;
}

void PrismEQProcessor::deleteBand (int band)
{
    if (soloBand.load() == band)
        soloBand = -1;
    setBandParam (band, "used", 0.0f);
}

void PrismEQProcessor::resetAllParameters()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
}

//==============================================================================
std::vector<float> PrismEQProcessor::captureParams() const
{
    std::vector<float> v;
    for (auto* p : getParameters())
        v.push_back (p->getValue());
    return v;
}

void PrismEQProcessor::applyParams (const std::vector<float>& v)
{
    const auto& params = getParameters();
    if (v.size() != (size_t) params.size())
        return;
    // "used" flags last so a band never appears with stale settings
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < params.size(); ++i)
        {
            auto* p = params[i];
            const bool isUsed = dynamic_cast<AudioProcessorParameterWithID*> (p) != nullptr
                                && static_cast<AudioProcessorParameterWithID*> (p)->paramID.endsWith ("_used");
            if (isUsed != (pass == 1) || std::abs (p->getValue() - v[(size_t) i]) < 1.0e-7f)
                continue;
            p->beginChangeGesture();
            p->setValueNotifyingHost (v[(size_t) i]);
            p->endChangeGesture();
        }
}

void PrismEQProcessor::syncUndo (bool userEdit)
{
    auto cur = captureParams();
    if (committed.empty())
    {
        committed = std::move (cur);
        return;
    }
    if (cur == committed)
        return;
    if (userEdit)
    {
        undoStack.push_back (committed);
        if (undoStack.size() > 100)
            undoStack.erase (undoStack.begin());
        redoStack.clear();
    }
    committed = std::move (cur);
}

void PrismEQProcessor::undo()
{
    if (undoStack.empty())
        return;
    redoStack.push_back (captureParams());
    committed = undoStack.back();
    undoStack.pop_back();
    applyParams (committed);
}

void PrismEQProcessor::redo()
{
    if (redoStack.empty())
        return;
    undoStack.push_back (captureParams());
    committed = redoStack.back();
    redoStack.pop_back();
    applyParams (committed);
}

void PrismEQProcessor::selectAB (int slot)
{
    if (slot == abIndex || (slot != 0 && slot != 1))
        return;
    syncUndo (true);
    abSlots[(size_t) abIndex] = captureParams();
    abIndex = slot;
    if (abSlots[(size_t) slot].empty())
        abSlots[(size_t) slot] = abSlots[(size_t) (1 - slot)];
    else
        applyParams (abSlots[(size_t) slot]);
    syncUndo (true);
}

void PrismEQProcessor::copyCurrentToOtherAB()
{
    abSlots[(size_t) (1 - abIndex)] = captureParams();
}

//==============================================================================
int PrismEQProcessor::getNumPrograms()                 { return jmax (1, presets->getNumFactoryPresets()); }
int PrismEQProcessor::getCurrentProgram()              { return currentProgram; }
const String PrismEQProcessor::getProgramName (int i)  { return presets->getFactoryPresetName (i); }

void PrismEQProcessor::setCurrentProgram (int i)
{
    if (i < 0 || i >= presets->getNumFactoryPresets())
        return;
    currentProgram = i;
    presets->loadFactoryPreset (i);
}

void PrismEQProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PrismEQProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* PrismEQProcessor::createEditor() { return new PrismEQEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PrismEQProcessor(); }
