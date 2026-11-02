#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "dsp/LayShitEngine.h"
#include "gfx/VisualState.h"

class LayShitProcessor final : public juce::AudioProcessor
{
public:
    LayShitProcessor();
    ~LayShitProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LayShit"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- GUI-facing API
    void requestFlush() noexcept { flushRequested.store (true); }
    /** Dev aid: injects a short drum-like hit every 0.8 s (used by the snapshot mode). */
    std::atomic<bool> testPulses { false };
    layshit::ModuleOrder getModuleOrder() const noexcept;
    void setModuleOrder (const layshit::ModuleOrder& o);

    juce::AudioProcessorValueTreeState apvts;
    layshit::VisualState visual;

private:
    layshit::EngineParams readParams() const;
    static juce::String orderToString (const layshit::ModuleOrder&);
    static bool orderFromString (const juce::String&, layshit::ModuleOrder&);

    layshit::LayShitEngine engine;
    std::atomic<uint32_t> packedOrder { 0x3210 }; // nibble k = module in slot k
    std::atomic<bool> flushRequested { false };
    bool lastFlushParam = false;
    int lastSplashSeen = 0;
    int currentProgram = 0;
    int pulseCounter = 0;
    double currentSampleRate = 44100.0;

    juce::AudioBuffer<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LayShitProcessor)
};
