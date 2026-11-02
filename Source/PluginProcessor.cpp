#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace layshit;

namespace
{
uint32_t packOrder (const ModuleOrder& o)
{
    uint32_t v = 0;
    for (int k = 0; k < numModules; ++k)
        v |= (uint32_t) o[(size_t) k] << (4 * k);
    return v;
}

ModuleOrder unpackOrder (uint32_t v)
{
    ModuleOrder o {};
    for (int k = 0; k < numModules; ++k)
        o[(size_t) k] = (int) ((v >> (4 * k)) & 0xF);
    return isValidOrder (o) ? o : defaultOrder();
}
}

LayShitProcessor::LayShitProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LayShit", createParameterLayout())
{
}

bool LayShitProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void LayShitProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate);
    engine.setParams (readParams());
    engine.setOrder (unpackOrder (packedOrder.load()));
    engine.reset();
    scratch.setSize (2, samplesPerBlock);
}

EngineParams LayShitProcessor::readParams() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    EngineParams p;
    p.timeMs = get (pid::time);
    if (get (pid::sync) > 0.5f)
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    bpm = juce::jlimit (20.0, 400.0, *b);
        const int div = juce::jlimit (0, (int) std::size (syncDivBeats) - 1, (int) get (pid::syncDiv));
        p.timeMs = (float) (60000.0 / bpm * syncDivBeats[div]);
    }
    p.feedback = get (pid::feedback);
    p.mix = get (pid::mix);
    p.pingPong = get (pid::pingPong) > 0.5f;
    p.wobbleRate = get (pid::wobbleRate);
    p.wobbleDepth = get (pid::wobbleDepth);
    p.hpHz = get (pid::fiberHP);
    p.lpHz = get (pid::fiberLP);
    p.clogDrive = get (pid::clogDrive);
    p.clogCrush = get (pid::clogCrush);
    p.clogMix = get (pid::clogMix);
    p.shiftHz = get (pid::shiftHz);
    p.shiftMix = get (pid::shiftMix);
    p.crystals.amount = get (pid::crysAmount);
    p.crystals.sizeMs = get (pid::crysSize);
    p.crystals.density = get (pid::crysDensity);
    p.crystals.sprayMs = get (pid::crysSpray);
    p.crystals.pitchSemis = get (pid::crysPitch);
    p.crystals.reverse = get (pid::crysReverse);
    p.crystals.freeze = get (pid::crysFreeze) > 0.5f;
    p.crystals.shimmer = get (pid::shimAmt);
    p.crystals.shimmerSemis = shimIntervalSemis[juce::jlimit (0, 2, (int) get (pid::shimInterval))];
    p.plungeDiffuse = get (pid::plungeDiffuse);
    p.plungeSize = get (pid::plungeSize);
    p.duckAmount = get (pid::duckAmount);
    p.duckReleaseMs = get (pid::duckRelease);
    p.bristol = get (pid::bristol);
    return p;
}

void LayShitProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    if (n == 0 || numOut == 0)
        return;

    const auto params = readParams();
    engine.setParams (params);
    engine.setOrder (unpackOrder (packedOrder.load()));

    const bool flushParam = apvts.getRawParameterValue (pid::flush)->load() > 0.5f;
    if ((flushParam && ! lastFlushParam) || flushRequested.exchange (false))
        engine.triggerFlush();
    lastFlushParam = flushParam;

    if (testPulses.load())
    {
        const int period = (int) (0.8 * currentSampleRate), hitLen = (int) (0.06 * currentSampleRate);
        for (int i = 0; i < n; ++i, ++pulseCounter)
        {
            if (pulseCounter >= period) pulseCounter = 0;
            const float s = pulseCounter < hitLen ? 0.8f * std::exp (-(float) pulseCounter / (0.012f * (float) currentSampleRate))
                                                        * std::sin (0.05f * (float) pulseCounter) : 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[i] += s;
        }
    }

    float* l = buffer.getWritePointer (0);
    float* r = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    if (r == nullptr)
    {
        // mono out: run the stereo engine on a duplicated channel and fold back down
        scratch.setSize (1, n, false, false, true);
        float* tmp = scratch.getWritePointer (0);
        juce::FloatVectorOperations::copy (tmp, l, n);
        engine.process (l, tmp, l, tmp, n);
        juce::FloatVectorOperations::add (l, tmp, n);
        juce::FloatVectorOperations::multiply (l, 0.5f, n);
    }
    else
    {
        if (numIn == 1)
            juce::FloatVectorOperations::copy (r, l, n);
        engine.process (l, r, l, r, n);
    }

    visual.inputLevel.store (engine.getInputLevel());
    visual.wetLevel.store (engine.getWetLevel());
    visual.duckGain.store (engine.getDuckGain());
    visual.flushPhase.store (engine.getFlushPhase());
    visual.delaySeconds.store (params.timeMs * 0.001f);
    const int sc = engine.splashCount.load (std::memory_order_relaxed);
    if (sc != lastSplashSeen)
    {
        lastSplashSeen = sc;
        visual.splashAmp.store (engine.splashAmp.load (std::memory_order_relaxed));
        visual.splashCount.store (sc);
    }
}

ModuleOrder LayShitProcessor::getModuleOrder() const noexcept { return unpackOrder (packedOrder.load()); }

void LayShitProcessor::setModuleOrder (const ModuleOrder& o)
{
    if (isValidOrder (o))
        packedOrder.store (packOrder (o));
}

juce::String LayShitProcessor::orderToString (const ModuleOrder& o)
{
    juce::String s;
    for (int m : o) s << m;
    return s;
}

bool LayShitProcessor::orderFromString (const juce::String& s, ModuleOrder& o)
{
    if (s.length() != numModules) return false;
    for (int k = 0; k < numModules; ++k)
        o[(size_t) k] = s[k] - '0';
    return isValidOrder (o);
}

int LayShitProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

void LayShitProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms()) return;
    currentProgram = index;
    ModuleOrder o;
    if (orderFromString (applyPreset (apvts, index), o))
        setModuleOrder (o);
}

const juce::String LayShitProcessor::getProgramName (int index)
{
    if (index < 0 || index >= getNumPrograms()) return {};
    return factoryPresets()[(size_t) index].name;
}

void LayShitProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("order", orderToString (getModuleOrder()), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LayShitProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (! xml->hasTagName (apvts.state.getType()))
            return;
        auto tree = juce::ValueTree::fromXml (*xml);
        ModuleOrder o;
        if (orderFromString (tree.getProperty ("order").toString(), o))
            setModuleOrder (o);
        currentProgram = juce::jlimit (0, juce::jmax (0, getNumPrograms() - 1), (int) tree.getProperty ("program", 0));
        tree.removeProperty ("order", nullptr);
        tree.removeProperty ("program", nullptr);
        apvts.replaceState (tree);
    }
}

juce::AudioProcessorEditor* LayShitProcessor::createEditor() { return new LayShitEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LayShitProcessor(); }
