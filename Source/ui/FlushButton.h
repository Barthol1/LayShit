#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace layshit::ui
{
/** Round flush button with a swirl icon; spins while the buffer drains. */
class FlushButton final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    FlushButton();
    std::function<void()> onFlush;
    std::function<juce::Colour()> getAccent;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    void timerCallback() override;
    float spin = 0.0f, energy = 0.0f;
};
}
