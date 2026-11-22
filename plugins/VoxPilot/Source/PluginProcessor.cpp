#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;

static juce::String dbString (float v, int)  { return juce::String (v, 1) + " dB"; }
static juce::String pctString (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

juce::AudioProcessorValueTreeState::ParameterLayout VoxPilotProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto dbAttr  = AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbString);
    auto pctAttr = AudioParameterFloatAttributes().withStringFromValueFunction (pctString);

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::target, 1 }, "Target",
                                       NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -18.0f, dbAttr));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::detect, 1 }, "Vocal Detect",
                                       NormalisableRange<float> (-70.0f, -20.0f, 0.1f), -45.0f, dbAttr));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::bleed, 1 }, "Bleed Guard",
                                       NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f, pctAttr));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::musicFollow, 1 }, "Music Follow",
                                       NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f, pctAttr));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::rangeLow, 1 }, "Range Low",
                                       NormalisableRange<float> (-gainLimitDb, 0.0f, 0.1f), -6.0f, dbAttr));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::rangeHigh, 1 }, "Range High",
                                       NormalisableRange<float> (0.0f, gainLimitDb, 0.1f), 6.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::fast, 1 }, "Fast", false,
                                       AudioParameterBoolAttributes().withStringFromValueFunction (
                                           [] (bool b, int) { return b ? String ("Fast") : String ("Slow"); })));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::rider, 1 }, "Rider Gain",
                                       NormalisableRange<float> (-gainLimitDb, gainLimitDb, 0.01f), 0.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::autoMode, 1 }, "Automation",
                                       StringArray { "Off", "Write", "Read" }, 0));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::output, 1 }, "Output",
                                       NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::lookahead, 1 }, "Lookahead",
                                       StringArray { "Off", "20 ms", "50 ms", "100 ms" }, 0,
                                       AudioParameterChoiceAttributes().withAutomatable (false)));
    return layout;
}

VoxPilotProcessor::VoxPilotProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "VoxPilotState", createLayout())
{
    auto f = [this] (const char* id) { return dynamic_cast<APF*> (apvts.getParameter (id)); };
    pTarget    = f (ParamIDs::target);
    pDetect    = f (ParamIDs::detect);
    pBleed     = f (ParamIDs::bleed);
    pMusic     = f (ParamIDs::musicFollow);
    pRangeLow  = f (ParamIDs::rangeLow);
    pRangeHigh = f (ParamIDs::rangeHigh);
    pRider     = f (ParamIDs::rider);
    pOutput    = f (ParamIDs::output);
    pFast      = dynamic_cast<juce::AudioParameterBool*>   (apvts.getParameter (ParamIDs::fast));
    pAutoMode  = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::autoMode));
    pLookahead = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::lookahead));
    jassert (pTarget && pDetect && pBleed && pMusic && pRangeLow && pRangeHigh && pRider && pOutput && pFast && pAutoMode && pLookahead);
}

bool VoxPilotProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != mainOut)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto& sc = layouts.inputBuses[1];
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void VoxPilotProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate);

    appliedGainDb = 0.0f;
    readSmoothCoef = (float) std::exp (-1.0 / (0.010 * sampleRate));
    outputGain.reset (sampleRate, 0.03);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->get()));

    writeInterval   = juce::jmax (1, (int) (sampleRate / 100.0)); // 100 automation points / s max
    historyInterval = juce::jmax (1, (int) (sampleRate / 60.0));  // 60 display frames / s
    learnInterval   = juce::jmax (1, (int) (sampleRate / 100.0)); // learn frames every 10 ms
    writeCounter = historyCounter = learnCounter = 0;
    outCoef = (float) std::exp (-1.0 / (0.05 * sampleRate));
    outMs = 0.0f;
    lastWrittenDb = 1000.0f;

    // delay line sized for the longest choice
    const int maxDelay = (int) std::ceil (sampleRate * 0.001 * lookaheadChoicesMs[3]) + 1;
    delayBuffer.setSize (juce::jmax (1, getMainBusNumInputChannels()), maxDelay + 1);
    delayBuffer.clear();
    delayWritePos = 0;
    lookaheadIndex = pLookahead->getIndex();
    lookaheadSamples = lookaheadSamplesFor (lookaheadIndex);
    reportedLatency.store (lookaheadSamples);
    setLatencySamples (lookaheadSamples);
}

int VoxPilotProcessor::lookaheadSamplesFor (int choiceIndex) const
{
    return (int) std::round (currentSampleRate * 0.001 * lookaheadChoicesMs[juce::jlimit (0, 3, choiceIndex)]);
}

void VoxPilotProcessor::handleAsyncUpdate()
{
    // tell the host (Logic) about the new plug-in delay so it can compensate
    setLatencySamples (reportedLatency.load());

    if (learnState.load() == 2)
        finishLearn();
}

//==============================================================================
void VoxPilotProcessor::startLearn()
{
    learnState.store (0);
    for (auto& b : learnHist) b.store (0, std::memory_order_relaxed);
    learnSignalFrames.store (0);
    learnHadMusic.store (false);
    learnMessage = {};
    learnState.store (1);
}

void VoxPilotProcessor::cancelLearn()
{
    learnState.store (0);
}

bool VoxPilotProcessor::finishLearn()
{
    learnState.store (0);
    learnMessageTime = juce::Time::getMillisecondCounter();

    std::array<double, learnBins> h {};
    for (int i = 0; i < learnBins; ++i)
        h[(size_t) i] = (double) learnHist[(size_t) i].load (std::memory_order_relaxed);

    auto binDb = [] (int i) { return -110.0f + 0.5f * (float) i + 0.25f; };
    auto binOf = [] (float db) { return juce::jlimit (0, learnBins - 1, (int) ((db + 110.0f) * 2.0f)); };

    // level below which `fraction` of the frames in [fromBin, end) lie
    auto percentile = [&] (int fromBin, double fraction)
    {
        double total = 0.0;
        for (int i = fromBin; i < learnBins; ++i) total += h[(size_t) i];
        double acc = 0.0;
        for (int i = fromBin; i < learnBins; ++i)
            if ((acc += h[(size_t) i]) >= fraction * total)
                return binDb (i);
        return binDb (learnBins - 1);
    };
    auto count = [&] (int fromBin) { double n = 0.0; for (int i = fromBin; i < learnBins; ++i) n += h[(size_t) i]; return n; };

    const int signalBin = binOf (-70.0f);
    if (count (signalBin) < 300.0)   // < 3 s with signal
    {
        learnMessage = "LEARN: too little vocal - play a vocal passage and try again";
        return false;
    }

    // noise / breath floor: the quietest 10 % of the frames with any signal at all
    const float floorDb = percentile (binOf (-100.0f), 0.10);

    // voiced frames: clearly above that floor
    const int voicedBin = binOf (juce::jmax (floorDb + 10.0f, -65.0f));
    if (count (voicedBin) < 200.0)
    {
        learnMessage = "LEARN: could not separate the vocal from the noise floor";
        return false;
    }
    const float p10 = percentile (voicedBin, 0.10);
    const float med = percentile (voicedBin, 0.50);
    const float p90 = percentile (voicedBin, 0.90);

    // Target = the vocal's typical level: the rider evens out phrases around it without changing the
    // overall loudness. Detect sits between the noise floor and the quietest phrases.
    const float target = juce::jlimit (-40.0f, 0.0f, std::round (med * 2.0f) / 2.0f);
    const float detect = juce::jlimit (-70.0f, -20.0f, std::round (juce::jmin (p10 - 4.0f, juce::jmax (floorDb + 6.0f, p10 - 12.0f))));
    const float boost  = juce::jlimit (3.0f, gainLimitDb, std::ceil (med - p10) + 1.0f);
    const float cut    = juce::jlimit (3.0f, gainLimitDb, std::ceil (p90 - med) + 1.0f);
    // a high floor means bleed or room noise between phrases
    const float bleed  = learnHadMusic.load() ? 0.3f
                                              : juce::jlimit (0.0f, 0.5f, std::round ((floorDb + 60.0f) / 40.0f * 20.0f) / 20.0f);

    auto set = [] (juce::RangedAudioParameter* p, float v)
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    };
    set (pTarget, target);
    set (pDetect, detect);
    set (pRangeLow, -cut);
    set (pRangeHigh, boost);
    set (pBleed, bleed);

    learnMessage = "LEARNED: target " + juce::String (target, 1) + " dB  -  detect " + juce::String (detect, 0)
                 + " dB  -  range -" + juce::String (cut, 0) + " / +" + juce::String (boost, 0) + " dB";
    return true;
}

void VoxPilotProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto main = getBusBuffer (buffer, true, 0);
    const int numSamples = buffer.getNumSamples();
    const int numCh = main.getNumChannels();

    // clear any output channels without input
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // ---- side-chain -------------------------------------------------------------
    juce::AudioBuffer<float> sc;
    if (auto* scBus = getBus (true, 1); scBus != nullptr && scBus->isEnabled() && scBus->getNumberOfChannels() > 0)
        sc = getBusBuffer (buffer, true, 1);
    const int scCh = sc.getNumChannels();
    const bool hasMusic = scCh > 0;
    sidechainConnected.store (hasMusic, std::memory_order_relaxed);

    // ---- settings ---------------------------------------------------------------
    vp::RiderEngine::Settings s;
    s.targetDb    = pTarget->get();
    s.detectDb    = pDetect->get();
    s.bleedGuard  = pBleed->get();
    s.musicFollow = pMusic->get();
    s.rangeLowDb  = pRangeLow->get();
    s.rangeHighDb = pRangeHigh->get();
    s.fast        = pFast->get();

    // lookahead change: new delay, clear the line (avoids a click from stale audio), report latency
    if (const int idx = pLookahead->getIndex(); idx != lookaheadIndex)
    {
        lookaheadIndex = idx;
        lookaheadSamples = lookaheadSamplesFor (idx);
        delayBuffer.clear();
        reportedLatency.store (lookaheadSamples);
        triggerAsyncUpdate();
    }
    s.lookaheadMs = lookaheadChoicesMs[lookaheadIndex];
    engine.setSettings (s);

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (pOutput->get()));

    const auto mode = (AutoMode) pAutoMode->getIndex();

    // open / close the host gesture that lets Logic record our "Rider Gain" moves
    if (mode == AutoMode::write && ! writeGestureOpen)
    {
        pRider->beginChangeGesture();
        writeGestureOpen = true;
        lastWrittenDb = 1000.0f;
    }
    else if (mode != AutoMode::write && writeGestureOpen)
    {
        pRider->endChangeGesture();
        writeGestureOpen = false;
    }

    const float readTarget = pRider->get();
    const float invCh = numCh > 0 ? 1.0f / (float) numCh : 0.0f;
    const float invSc = scCh > 0 ? 1.0f / (float) scCh : 0.0f;

    float* const* chans = main.getArrayOfWritePointers();
    float* const* delayChans = delayBuffer.getArrayOfWritePointers();
    const int delayLen = delayBuffer.getNumSamples();
    const int delayCh = juce::jmin (numCh, delayBuffer.getNumChannels());
    const bool useDelay = lookaheadSamples > 0 && delayCh == numCh;
    const float* const* scChans = hasMusic ? sc.getArrayOfReadPointers() : nullptr;

    bool learning = learnState.load() == 1;
    if (learning && hasMusic)
        learnHadMusic.store (true, std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        float vocal = 0.0f;
        for (int ch = 0; ch < numCh; ++ch) vocal += chans[ch][i];
        vocal *= invCh;

        float music = 0.0f;
        if (hasMusic)
        {
            for (int ch = 0; ch < scCh; ++ch) music += scChans[ch][i];
            music *= invSc;
        }

        const float computed = engine.processSample (vocal, music, hasMusic);

        if (learning && ++learnCounter >= learnInterval)
        {
            learnCounter = 0;
            const float db = engine.getShortVocalDb();
            learnHist[(size_t) juce::jlimit (0, learnBins - 1, (int) ((db + 110.0f) * 2.0f))].fetch_add (1, std::memory_order_relaxed);
            if (db > -55.0f && learnSignalFrames.fetch_add (1) + 1 >= learnFramesNeeded)
            {
                learning = false;
                learnState.store (2);
                triggerAsyncUpdate();
            }
        }

        if (mode == AutoMode::read)
            appliedGainDb = readTarget + (appliedGainDb - readTarget) * readSmoothCoef;
        else
            appliedGainDb = computed;

        const float g = std::exp (appliedGainDb * 0.11512925464970229f) * outputGain.getNextValue();

        if (useDelay)
        {
            // the detector has already seen this sample; play the audio from `lookaheadSamples` ago
            int readPos = delayWritePos - lookaheadSamples;
            if (readPos < 0) readPos += delayLen;
            for (int ch = 0; ch < numCh; ++ch)
            {
                delayChans[ch][delayWritePos] = chans[ch][i];
                chans[ch][i] = delayChans[ch][readPos];
            }
            if (++delayWritePos >= delayLen) delayWritePos = 0;
        }

        float outSum = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            chans[ch][i] *= g;
            outSum += chans[ch][i];
        }
        outSum *= invCh;
        outMs = outCoef * outMs + (1.0f - outCoef) * outSum * outSum;

        // ---- automation write -------------------------------------------------
        if (mode == AutoMode::write && ++writeCounter >= writeInterval)
        {
            writeCounter = 0;
            if (std::abs (computed - lastWrittenDb) > 0.05f)
            {
                pRider->setValueNotifyingHost (pRider->convertTo0to1 (computed));
                lastWrittenDb = computed;
            }
        }

        // ---- display history --------------------------------------------------
        if (++historyCounter >= historyInterval)
        {
            historyCounter = 0;
            const float outDb = vp::RiderEngine::toDb (outMs);
            pushHistory ({ engine.getVocalDb(), outDb, appliedGainDb,
                           engine.getEffectiveTargetDb(), engine.getEffectiveThresholdDb(), engine.isActive() });
        }
    }

    meterVocalDb.store (engine.getVocalDb(), std::memory_order_relaxed);
    meterMusicDb.store (engine.getMusicDb(), std::memory_order_relaxed);
    meterOutDb.store (vp::RiderEngine::toDb (outMs), std::memory_order_relaxed);
    meterGainDb.store (appliedGainDb, std::memory_order_relaxed);
    meterTargetDb.store (engine.getEffectiveTargetDb(), std::memory_order_relaxed);
    meterThresholdDb.store (engine.getEffectiveThresholdDb(), std::memory_order_relaxed);
    meterActive.store (engine.isActive() && mode != AutoMode::read, std::memory_order_relaxed);
}

void VoxPilotProcessor::pushHistory (const HistoryFrame& f)
{
    const auto scope = historyFifo.write (1);
    if (scope.blockSize1 > 0) historyBuffer[(size_t) scope.startIndex1] = f;
    else if (scope.blockSize2 > 0) historyBuffer[(size_t) scope.startIndex2] = f;
    // if the FIFO is full (editor closed) the frame is simply dropped
}

int VoxPilotProcessor::popHistory (HistoryFrame* dest, int maxFrames)
{
    const auto scope = historyFifo.read (juce::jmin (maxFrames, historyFifo.getNumReady()));
    int n = 0;
    for (int i = 0; i < scope.blockSize1; ++i) dest[n++] = historyBuffer[(size_t) (scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i) dest[n++] = historyBuffer[(size_t) (scope.startIndex2 + i)];
    return n;
}

void VoxPilotProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VoxPilotProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VoxPilotProcessor::createEditor()
{
    return new VoxPilotEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxPilotProcessor();
}
