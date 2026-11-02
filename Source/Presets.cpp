#include "Presets.h"
#include "Params.h"

namespace layshit
{
const std::vector<Preset>& factoryPresets()
{
    using namespace pid;
    static const std::vector<Preset> presets {
        { "Number Two Classic", { { time, 375 }, { feedback, 0.45f }, { mix, 0.35f } } },
        { "Morning Coffee",     { { time, 180 }, { feedback, 0.55f }, { mix, 0.4f }, { wobbleDepth, 0.35f }, { wobbleRate, 0.4f },
                                  { fiberLP, 5000 }, { fiberHP, 150 }, { duckAmount, 0.5f } } },
        { "Taco Tuesday",       { { time, 260 }, { feedback, 0.72f }, { mix, 0.45f }, { clogDrive, 0.75f }, { clogCrush, 0.25f },
                                  { fiberLP, 7000 }, { bristol, 2.0f }, { pingPong, 1 } } },
        { "Barber Spiral",      { { time, 140 }, { feedback, 0.85f }, { mix, 0.5f }, { shiftHz, 18 }, { plungeDiffuse, 0.3f },
                                  { pingPong, 1 } }, "1320" },
        { "Clogged Pipes",      { { time, 90 }, { feedback, 0.8f }, { mix, 0.5f }, { clogDrive, 1.0f }, { clogCrush, 0.7f },
                                  { fiberLP, 3500 }, { fiberHP, 300 }, { bristol, 1.0f } } },
        { "Rabbit Pellets",     { { time, 60 }, { feedback, 0.6f }, { mix, 0.4f }, { crysAmount, 0.7f }, { crysSize, 25 },
                                  { crysDensity, 40 }, { crysSpray, 30 }, { bristol, 1.5f } } },
        { "Courtesy Flush",     { { time, 420 }, { feedback, 0.6f }, { mix, 0.5f }, { duckAmount, 0.9f }, { duckRelease, 600 },
                                  { plungeDiffuse, 0.4f } } },
        { "Liquid Lunch",       { { time, 520 }, { feedback, 0.7f }, { mix, 0.45f }, { plungeDiffuse, 0.8f }, { plungeSize, 1.6f },
                                  { wobbleDepth, 0.5f }, { bristol, 6.5f } }, "0132" },
        { "Crystal Throne",     { { time, 350 }, { feedback, 0.65f }, { mix, 0.45f }, { crysAmount, 0.6f }, { crysSize, 180 },
                                  { crysDensity, 18 }, { crysPitch, 12 }, { crysReverse, 0.3f }, { plungeDiffuse, 0.5f } } },
        { "Ascension",          { { time, 600 }, { feedback, 0.82f }, { mix, 0.55f }, { crysAmount, 0.9f }, { crysSize, 220 },
                                  { crysDensity, 25 }, { crysSpray, 300 }, { shimAmt, 0.85f }, { plungeDiffuse, 0.7f },
                                  { plungeSize, 1.5f }, { fiberHP, 250 } }, "0132" },
        { "Overflowing",        { { time, 240 }, { feedback, 1.05f }, { mix, 0.5f }, { clogDrive, 0.3f }, { shiftHz, -6 },
                                  { fiberLP, 6000 } } },
    };
    return presets;
}

juce::String applyPreset (juce::AudioProcessorValueTreeState& apvts, int index)
{
    const auto& all = factoryPresets();
    if (index < 0 || index >= (int) all.size())
        return "0123";

    const auto& preset = all[(size_t) index];
    for (auto* p : apvts.processor.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr || rp->getParameterID() == pid::flush)
            continue;

        float norm = rp->getDefaultValue();
        for (auto& [id, value] : preset.values)
            if (rp->getParameterID() == id)
                norm = rp->convertTo0to1 (value);

        rp->setValueNotifyingHost (norm);
    }
    return preset.order;
}
}
