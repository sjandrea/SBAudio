#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace juce;

namespace
{
    String dbText (float v, int)  { return String (v, 1) + " dB"; }
    String msText (float v, int)
    {
        if (v < 1.0f)    return String (roundToInt (v * 1000.0f)) + String (CharPointer_UTF8 (" µs"));
        if (v < 10.0f)   return String (v, 1) + " ms";
        if (v < 1000.0f) return String (roundToInt (v)) + " ms";
        return String (v / 1000.0f, 2) + " s";
    }
    String pctText (float v, int) { return String (roundToInt (v)) + " %"; }

    NormalisableRange<float> skewed (float lo, float hi, float interval, float centre)
    {
        NormalisableRange<float> r (lo, hi, interval);
        r.setSkewForCentre (centre);
        return r;
    }
}

AudioProcessorValueTreeState::ParameterLayout StressulfProcessor::createLayout()
{
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    using B = AudioParameterBool;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::input, 1 }, "Input",
                                       NormalisableRange<float> (-12.0f, 30.0f, 0.1f), 0.0f,
                                       A().withLabel ("dB").withStringFromValueFunction (dbText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::attack, 1 }, "Attack",
                                       skewed (0.05f, 30.0f, 0.01f, 3.0f), 5.0f,
                                       A().withLabel ("ms").withStringFromValueFunction (msText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::release, 1 }, "Release",
                                       skewed (50.0f, 3500.0f, 1.0f, 400.0f), 300.0f,
                                       A().withLabel ("ms").withStringFromValueFunction (msText)));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::ratio, 1 }, "Ratio", stressulf::ratioNames, stressulf::r4));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::dist, 1 }, "Distortion", stressulf::distNames, 0));
    ps.push_back (std::make_unique<B> (ParameterID { ParamIDs::detHp, 1 }, "Detector HP", false));
    ps.push_back (std::make_unique<B> (ParameterID { ParamIDs::detBand, 1 }, "Detector Band", false));
    ps.push_back (std::make_unique<B> (ParameterID { ParamIDs::audioHp, 1 }, "Audio HP", false));
    ps.push_back (std::make_unique<B> (ParameterID { ParamIDs::link, 1 }, "Stereo Link", true));
    ps.push_back (std::make_unique<B> (ParameterID { ParamIDs::smash, 1 }, "Smash", false));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::mix, 1 }, "Mix",
                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                       A().withLabel ("%").withStringFromValueFunction (pctText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::output, 1 }, "Output",
                                       NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                       A().withLabel ("dB").withStringFromValueFunction (dbText)));
    return { ps.begin(), ps.end() };
}

StressulfProcessor::StressulfProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Stressulf", createLayout())
{
    presets = std::make_unique<sbaudio::PresetManager> (apvts, "Stressulf", ".stressulfpreset",
                                                        stressulf::factoryPresets());
}

bool StressulfProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo());
}

void StressulfProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples (engine.getLatency());
}

void StressulfProcessor::storeMax (std::atomic<float>& a, float v) noexcept
{
    float cur = a.load (std::memory_order_relaxed);
    while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
}

float StressulfProcessor::blockMeanSquare (const AudioBuffer<float>& b) noexcept
{
    float best = 0.0f;
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        const float r = b.getRMSLevel (c, 0, b.getNumSamples());
        best = jmax (best, r * r);
    }
    return best;
}

stressulf::Settings StressulfProcessor::currentSettings() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    stressulf::Settings s;
    s.inputDb   = v (ParamIDs::input);
    s.outputDb  = v (ParamIDs::output);
    s.attackMs  = v (ParamIDs::attack);
    s.releaseMs = v (ParamIDs::release);
    s.ratio     = jlimit (0, stressulf::ratioNames.size() - 1, (int) v (ParamIDs::ratio));
    s.dist      = jlimit (0, 2, (int) v (ParamIDs::dist));
    s.detHp     = v (ParamIDs::detHp) > 0.5f;
    s.detBand   = v (ParamIDs::detBand) > 0.5f;
    s.audioHp   = v (ParamIDs::audioHp) > 0.5f;
    s.link      = v (ParamIDs::link) > 0.5f;
    s.smash     = v (ParamIDs::smash) > 0.5f;
    s.mix       = v (ParamIDs::mix) * 0.01f;
    return s;
}

void StressulfProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    storeMax (meterInMs, blockMeanSquare (buffer));
    engine.process (buffer, currentSettings());
    storeMax (meterGrDb, engine.getBlockMaxGrDb());
    storeMax (meterOutMs, blockMeanSquare (buffer));
}

//==============================================================================
int StressulfProcessor::getNumPrograms()                { return jmax (1, presets->getNumFactoryPresets()); }
const String StressulfProcessor::getProgramName (int i) { return presets->getFactoryPresetName (i); }

void StressulfProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, presets->getNumFactoryPresets()))
        return;
    currentProgram = i;
    presets->loadFactoryPreset (i);
}

void StressulfProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void StressulfProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* StressulfProcessor::createEditor() { return new StressulfEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new StressulfProcessor(); }
