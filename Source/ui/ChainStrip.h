#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "dsp/LayShitEngine.h"
#include <functional>

namespace layshit::ui
{
/** Signal-chain strip: four module cards between IN and LOOP. Drag a card to reorder the
    feedback-loop modules, click one to edit it in the right-hand panel. */
class ChainStrip final : public juce::Component, private juce::Timer
{
public:
    ChainStrip();

    std::function<ModuleOrder()> getOrder;
    std::function<void (const ModuleOrder&)> setOrder;
    std::function<void (int moduleId)> onSelect;
    std::function<float (int moduleId)> getActivity;
    std::function<juce::Colour()> getAccent;

    void setSelected (int moduleId);
    int getSelected() const { return selected; }
    /** 1-based position of a module in the current chain. */
    int positionOf (int moduleId) const;

    static juce::String moduleName (int moduleId);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> cardsArea() const;
    juce::Rectangle<float> slotBounds (int slot) const;
    int slotAt (float x) const;

    ModuleOrder order = defaultOrder();
    int selected = modCrystals;
    int draggingSlot = -1, hoverSlot = -1;
    float dragOffset = 0, dragX = 0, flowPhase = 0;
    bool moved = false;
};
}
