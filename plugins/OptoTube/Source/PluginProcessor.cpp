#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace juce;

namespace
{
    String dbText (float v, int)   { return String (v, 1) + " dB"; }
    String msText (float v, int)   { return v < 10.0f ? String (v, 1) + " ms" : v < 1000.0f ? String (roundToInt (v)) + " ms"
                                                                                           : String (v / 1000.0f, 2) + " s"; }
    String pctText (float v, int)  { return String (roundToInt (v)) + " %"; }
    String ratioText (float v, int){ return String (v, v < 10.0f ? 1 : 0) + ":1"; }

    NormalisableRange<float> skewed (float lo, float hi, float interval, float centre)
    {
        NormalisableRange<float> r (lo, hi, interval);
        r.setSkewForCentre (centre);
        return r;
    }
}

AudioProcessorValueTreeState::ParameterLayout OptoTubeProcessor::createLayout()
{
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::threshold, 1 }, "Threshold",
                                       NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -20.0f,
                                       A().withLabel ("dB").withStringFromValueFunction (dbText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::ratio, 1 }, "Ratio",
                                       skewed (1.5f, 10.0f, 0.01f, 3.5f), 3.0f,
                                       A().withStringFromValueFunction (ratioText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::attack, 1 }, "Attack",
                                       skewed (0.5f, 300.0f, 0.01f, 15.0f), 10.0f,
                                       A().withLabel ("ms").withStringFromValueFunction (msText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::release, 1 }, "Release",
                                       skewed (50.0f, 10000.0f, 1.0f, 500.0f), 300.0f,
                                       A().withLabel ("ms").withStringFromValueFunction (msText)));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Mode", optotube::modeNames, 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::hpf, 1 }, "Detector HPF", optotube::hpfNames, 0));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::tube, 1 }, "Tube",
                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f,
                                       A().withLabel ("%").withStringFromValueFunction (pctText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::gain, 1 }, "Gain",
                                       NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f,
                                       A().withLabel ("dB").withStringFromValueFunction (dbText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::mix, 1 }, "Mix",
                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                       A().withLabel ("%").withStringFromValueFunction (pctText)));
    return { ps.begin(), ps.end() };
}

OptoTubeProcessor::OptoTubeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OptoTube", createLayout())
{
    pThreshold = apvts.getRawParameterValue (ParamIDs::threshold);
    pRatio     = apvts.getRawParameterValue (ParamIDs::ratio);
    pAttack    = apvts.getRawParameterValue (ParamIDs::attack);
    pRelease   = apvts.getRawParameterValue (ParamIDs::release);
    pMode      = apvts.getRawParameterValue (ParamIDs::mode);
    pHpf       = apvts.getRawParameterValue (ParamIDs::hpf);
    pTube      = apvts.getRawParameterValue (ParamIDs::tube);
    pGain      = apvts.getRawParameterValue (ParamIDs::gain);
    pMix       = apvts.getRawParameterValue (ParamIDs::mix);

    presets = std::make_unique<sbaudio::PresetManager> (apvts, "OptoTube", ".optopreset",
                                                        optotube::factoryPresets(),
                                                        Array<Identifier> { Identifier ("meterMode") });
}

bool OptoTubeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo());
}

void OptoTubeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

void OptoTubeProcessor::storeMax (std::atomic<float>& a, float v) noexcept
{
    float cur = a.load (std::memory_order_relaxed);
    while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
}

float OptoTubeProcessor::blockMeanSquare (const AudioBuffer<float>& b) noexcept
{
    float best = 0.0f;
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        const float r = b.getRMSLevel (c, 0, b.getNumSamples());
        best = jmax (best, r * r);
    }
    return best;
}

void OptoTubeProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    storeMax (meterInMs, blockMeanSquare (buffer));

    optotube::Settings s;
    s.thresholdDb = pThreshold->load();
    s.ratio       = pRatio->load();
    s.attackMs    = pAttack->load();
    s.releaseMs   = pRelease->load();
    s.mode        = jlimit (0, 2, (int) pMode->load());
    s.hpfHz       = optotube::hpfFreqs[jlimit (0, 3, (int) pHpf->load())];
    s.tube        = pTube->load() * 0.01f;
    s.gainDb      = pGain->load();
    s.mix         = pMix->load() * 0.01f;

    engine.process (buffer, s);

    storeMax (meterGrDb, engine.getBlockMaxGrDb());
    storeMax (meterOutMs, blockMeanSquare (buffer));
}

//==============================================================================
int OptoTubeProcessor::getNumPrograms()                { return jmax (1, presets->getNumFactoryPresets()); }
const String OptoTubeProcessor::getProgramName (int i) { return presets->getFactoryPresetName (i); }

void OptoTubeProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, presets->getNumFactoryPresets()))
        return;
    currentProgram = i;
    presets->loadFactoryPreset (i);
}

void OptoTubeProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void OptoTubeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* OptoTubeProcessor::createEditor() { return new OptoTubeEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OptoTubeProcessor(); }
