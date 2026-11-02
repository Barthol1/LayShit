#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace layshit::ui
{
/** Preset pill: prev / name (opens a list) / next. Still a roll of toilet paper at heart. */
class PresetBar final : public juce::Component
{
public:
    PresetBar();

    juce::StringArray names;
    std::function<int()> getCurrent;
    std::function<void (int)> onChoose;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    enum Zone { none, prev, name, next };
    Zone zoneAt (juce::Point<float>) const;
    void step (int delta);
    Zone hover = none;
};
}
