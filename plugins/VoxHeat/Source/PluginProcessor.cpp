#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

namespace
{
    // ---- Saturation curves -------------------------------------------------
    // "TG" : solid-state console style. Harder knee, mostly odd harmonics,
    //        a touch of asymmetry. Forward and present.
    constexpr float tgBias = 0.06f;
    inline float tgCurve (float v) noexcept
    {
        const float a = std::abs (v);
        return v / std::cbrt (1.0f + a * a * a);
    }
    inline float tgSlope (float v) noexcept
    {
        const float a = std::abs (v);
        return std::pow (1.0f + a * a * a, -4.0f / 3.0f);
    }

    // "Tube" : valve stage style. Rounder knee, strong asymmetry -> even
    //          harmonics, thicker and warmer.
    constexpr float tubeBias = 0.30f;
    inline float tubeCurve (float v) noexcept { return std::tanh (v); }
    inline float tubeSlope (float v) noexcept { const float t = std::tanh (v); return 1.0f - t * t; }

    inline float driveToK (float drive01) noexcept
    {
        return 0.5f * std::pow (10.0f, (drive01 * 33.0f) / 20.0f);
    }
}

//==============================================================================
VoxHeatProcessor::VoxHeatProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pInput  = apvts.getRawParameterValue ("input");
    pDrive  = apvts.getRawParameterValue ("drive");
    pChar   = apvts.getRawParameterValue ("character");
    pDens   = apvts.getRawParameterValue ("density");
    pTone   = apvts.getRawParameterValue ("tone");
    pMix    = apvts.getRawParameterValue ("mix");
    pOutput = apvts.getRawParameterValue ("output");
    pAuto   = apvts.getRawParameterValue ("autogain");

    presets = std::make_unique<sbaudio::PresetManager> (apvts, "VoxHeat", ".voxheatpreset",
                                                        voxheat::factoryPresets());
}

juce::AudioProcessorValueTreeState::ParameterLayout VoxHeatProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto dB  = AudioParameterFloatAttributes().withLabel ("dB")
                   .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });
    auto pct = AudioParameterFloatAttributes().withLabel ("%")
                   .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "input", 1 }, "Input",
                    NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, dB));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 }, "Drive",
                    NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f, pct));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "character", 1 }, "Character",
                    StringArray { "TG", "Tube" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "density", 1 }, "Density",
                    NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tone", 1 }, "Tone",
                    NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, dB));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                    NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                    NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, dB));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "autogain", 1 }, "Auto Gain", true));

    return { p.begin(), p.end() };
}

bool VoxHeatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void VoxHeatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int numCh = juce::jmax (1, getTotalNumOutputChannels());

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, (size_t) osOrder,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampler->initProcessing ((size_t) samplesPerBlock);
    oversampler->reset();
    latency = (int) std::lround (oversampler->getLatencyInSamples());
    setLatencySamples (latency);

    const double osRate = sampleRate * osFactor;
    lowCoeff = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 100.0 / sampleRate));
    dcCoeff  = (float) (1.0 - juce::MathConstants<double>::twoPi * 15.0 / sampleRate);
    envAtt   = (float) (1.0 - std::exp (-1.0 / (0.003 * osRate)));
    envRel   = (float) (1.0 - std::exp (-1.0 / (0.060 * osRate)));

    for (auto& c : ch)
    {
        c.lowState = 0; c.dcX = c.dcY = 0; c.env = 0; c.writePos = 0;
        c.preEmph.setShelf (true, sampleRate, 5000.0, -6.0);
        c.deEmph.setShelf  (true, sampleRate, 5000.0, +6.0);
        c.preEmph.reset(); c.deEmph.reset();
        c.toneLow.reset(); c.toneHigh.reset();
        c.lowDelay.assign ((size_t) latency + 1, 0.0f);
        c.dryDelay.assign ((size_t) latency + 1, 0.0f);
    }
    lastTone = 1000.0f; // force tone update

    const double smooth = 0.05;
    inGainS.reset (sampleRate, smooth);  outGainS.reset (sampleRate, smooth);
    driveS.reset (sampleRate, smooth);   densityS.reset (sampleRate, smooth);
    mixS.reset (sampleRate, smooth);     charS.reset (sampleRate, 0.03);
    autoS.reset (sampleRate, 0.05);

    inGainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInput->load()));
    outGainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    driveS.setCurrentAndTargetValue (pDrive->load() * 0.01f);
    densityS.setCurrentAndTargetValue (pDens->load() * 0.01f);
    mixS.setCurrentAndTargetValue (pMix->load() * 0.01f);
    charS.setCurrentAndTargetValue (pChar->load() > 0.5f ? 1.0f : 0.0f);
    autoS.setCurrentAndTargetValue (pAuto->load() > 0.5f ? 1.0f : 0.0f);

    dryBuf.setSize (numCh, samplesPerBlock);
    lowBuf.setSize (numCh, samplesPerBlock);
    for (auto* v : { &kArr, &densArr, &charArr, &autoArr, &mixArr, &inArr, &outArr })
        v->assign ((size_t) samplesPerBlock, 0.0f);
}

//==============================================================================
void VoxHeatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels(), 2);
    const int n = buffer.getNumSamples();
    if (n == 0 || numCh == 0) return;

    // Hosts may occasionally exceed the announced block size: process in chunks
    if (n > (int) kArr.size())
    {
        for (int start = 0; start < n; start += (int) kArr.size())
        {
            const int len = juce::jmin ((int) kArr.size(), n - start);
            juce::AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), numCh, start, len);
            juce::MidiBuffer m;
            processBlock (sub, m);
        }
        return;
    }

    // ---- parameters ------------------------------------------------------
    inGainS.setTargetValue (juce::Decibels::decibelsToGain (pInput->load()));
    outGainS.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    driveS.setTargetValue (pDrive->load() * 0.01f);
    densityS.setTargetValue (pDens->load() * 0.01f);
    mixS.setTargetValue (pMix->load() * 0.01f);
    charS.setTargetValue (pChar->load() > 0.5f ? 1.0f : 0.0f);
    autoS.setTargetValue (pAuto->load() > 0.5f ? 1.0f : 0.0f);

    for (int i = 0; i < n; ++i)
    {
        inArr[(size_t) i]   = inGainS.getNextValue();
        outArr[(size_t) i]  = outGainS.getNextValue();
        kArr[(size_t) i]    = driveToK (driveS.getNextValue());
        densArr[(size_t) i] = densityS.getNextValue();
        mixArr[(size_t) i]  = mixS.getNextValue();
        charArr[(size_t) i] = charS.getNextValue();
        autoArr[(size_t) i] = autoS.getNextValue();
    }

    const float tone = pTone->load();
    if (std::abs (tone - lastTone) > 0.01f)
    {
        lastTone = tone;
        for (auto& c : ch)
        {
            c.toneLow.setShelf  (false, sr, 450.0,  -tone * 0.5);
            c.toneHigh.setShelf (true,  sr, 2800.0, +tone * 0.5);
        }
    }

    // ---- stage 1 (base rate): input gain, low split, sibilance pre-emphasis
    float inSq = 0.0f;
    for (int c = 0; c < numCh; ++c)
    {
        dryBuf.copyFrom (c, 0, buffer, c, 0, n);
        auto& s = ch[(size_t) c];
        auto* d = buffer.getWritePointer (c);
        auto* lo = lowBuf.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            const float x = d[i] * inArr[(size_t) i];
            inSq += x * x;
            s.lowState += lowCoeff * (x - s.lowState);
            lo[i] = s.lowState;                       // lows bypass the saturator
            d[i] = s.preEmph.process (x - s.lowState);
        }
    }

    // ---- stage 2 (4x oversampled): compander + saturation ------------------
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
    auto os = oversampler->processSamplesUp (block);
    const int osN = (int) os.getNumSamples();

    double dist = 0.0, lin = 0.0;
    for (int c = 0; c < numCh; ++c)
    {
        auto& s = ch[(size_t) c];
        auto* d = os.getChannelPointer ((size_t) c);
        float env = s.env;

        for (int j = 0; j < osN; ++j)
        {
            const size_t i = (size_t) (j / osFactor);
            const float x = d[j];
            const float k = kArr[i];
            const float cb = charArr[i];

            // Compander: compress into the saturator, expand back afterwards.
            // Quiet syllables get as much colour as loud ones, dynamics are kept.
            const float x2 = x * x;
            env += (x2 > env ? envAtt : envRel) * (x2 - env);
            const float rms = std::sqrt (env) + 1.0e-6f;
            const float ratio = 1.0f + 2.0f * densArr[i];
            const float g = juce::jlimit (0.25f, 4.0f, std::pow (rms / refLevel, 1.0f / ratio - 1.0f));
            const float u = x * g;

            // Curves (both computed so the character switch can crossfade without clicks)
            const float vt = k * u + tgBias,   vv = k * u + tubeBias;
            const float yt = tgCurve (vt) - tgCurve (tgBias);
            const float yv = tubeCurve (vv) - tubeCurve (tubeBias);

            // Gain normalisation: Auto Gain keeps a nominal-level vocal at the
            // same loudness; without it the level rises moderately with Drive.
            const float kn = k * autoArr[i] + (1.0f - autoArr[i]) * std::pow (k, 0.5f) * std::pow (driveToK (0.0f), 0.5f);
            const float kr = kn * refLevel;
            const float agT = 2.0f * refLevel / (std::abs (tgCurve (kr + tgBias) - tgCurve (tgBias))
                                               + std::abs (tgCurve (-kr + tgBias) - tgCurve (tgBias)));
            const float agV = 2.0f * refLevel / (std::abs (tubeCurve (kr + tubeBias) - tubeCurve (tubeBias))
                                               + std::abs (tubeCurve (-kr + tubeBias) - tubeCurve (tubeBias)));

            const float y = ((1.0f - cb) * yt * agT + cb * yv * agV) / g;
            const float l = k * x * ((1.0f - cb) * tgSlope (tgBias) * agT + cb * tubeSlope (tubeBias) * agV);
            dist += (double) ((y - l) * (y - l));
            lin  += (double) (l * l);
            d[j] = y;
        }
        s.env = env;
    }
    oversampler->processSamplesDown (block);

    // ---- stage 3 (base rate): de-emphasis, recombine, tone, mix, output ----
    float sumSq = 0.0f;
    for (int c = 0; c < numCh; ++c)
    {
        auto& s = ch[(size_t) c];
        auto* d = buffer.getWritePointer (c);
        const auto* dry = dryBuf.getReadPointer (c);
        const auto* lo = lowBuf.getReadPointer (c);
        const int size = latency + 1;
        int wp = s.writePos;
        for (int i = 0; i < n; ++i)
        {
            const int rp = (wp + 1) % size;
            s.dryDelay[(size_t) wp] = dry[i];
            s.lowDelay[(size_t) wp] = lo[i];
            const float dryD = s.dryDelay[(size_t) rp];
            const float lowD = s.lowDelay[(size_t) rp];

            float w = s.deEmph.process (d[i]);
            const float dc = w - s.dcX + dcCoeff * s.dcY;   // DC blocker (asymmetric curves)
            s.dcX = w; s.dcY = dc;
            w = dc + lowD;
            w = s.toneHigh.process (s.toneLow.process (w));

            const float m = mixArr[(size_t) i];
            const float out = (dryD * (1.0f - m) + w * m) * outArr[(size_t) i];
            d[i] = out;
            sumSq += out * out;
            wp = rp;
        }
        s.writePos = wp;
    }

    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    // ---- metering ----------------------------------------------------------
    meterAcc += sumSq / (float) numCh;
    inMeterAcc += inSq / (float) numCh;
    meterCount += n;
    distAcc += dist; linAcc += lin;
    if (meterCount >= (int) (sr * 0.03))
    {
        outRmsDb.store (juce::Decibels::gainToDecibels (std::sqrt (meterAcc / (float) meterCount), -100.0f));
        inRmsDb.store (juce::Decibels::gainToDecibels (std::sqrt (inMeterAcc / (float) meterCount), -100.0f));
        const float h = linAcc > 1.0e-9 ? (float) std::sqrt (distAcc / linAcc) : 0.0f;
        heat.store (juce::jlimit (0.0f, 1.0f, h * 3.0f));
        meterAcc = inMeterAcc = 0.0f; meterCount = 0; distAcc = linAcc = 0.0;
    }
}

//==============================================================================
int VoxHeatProcessor::getNumPrograms()                { return juce::jmax (1, presets->getNumFactoryPresets()); }
const juce::String VoxHeatProcessor::getProgramName (int i) { return presets->getFactoryPresetName (i); }

void VoxHeatProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, presets->getNumFactoryPresets()))
        return;
    currentProgram = i;
    presets->loadFactoryPreset (i);
}

void VoxHeatProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void VoxHeatProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VoxHeatProcessor::createEditor() { return new VoxHeatEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoxHeatProcessor(); }
