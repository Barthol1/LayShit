#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace layshit
{
struct Preset
{
    const char* name;
    struct Value
    {
        Value (const char* paramId, double v) : id (paramId), value ((float) v) {}
        const char* id;
        float value;
    };
    std::vector<Value> values; // params not listed go to default
    const char* order = "0123";
};

const std::vector<Preset>& factoryPresets();

/** Sets every parameter to the preset (or its default). Returns the preset's order string. */
juce::String applyPreset (juce::AudioProcessorValueTreeState& apvts, int index);
}
