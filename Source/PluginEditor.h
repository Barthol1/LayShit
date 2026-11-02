#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "gfx/BowlRenderer.h"
#include "ui/LayShitLookAndFeel.h"
#include "ui/ChainStrip.h"
#include "ui/PresetBar.h"
#include "ui/FlushButton.h"

/** Minimal ring knob; the caption swaps to the value while hovered or dragged. */
class ModernKnob final : public juce::Component
{
public:
    ModernKnob (juce::AudioProcessorValueTreeState& s, const char* paramId, const juce::String& name);
    void resized() override;
    void refreshCaption();

    juce::Slider slider;
    juce::Label caption;

private:
    juce::String name;
    bool showingValue = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class LayShitEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LayShitEditor (LayShitProcessor&);
    ~LayShitEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPage (int moduleId);
    ModernKnob& addKnob (std::vector<juce::Component*>& page, const char* id, const juce::String& name);
    void layoutGrid (const std::vector<juce::Component*>& items, juce::Point<int> origin);
    juce::Rectangle<int> rightCardBounds() const;
    void pushGlass();
    void setupSnapshotMode();

    LayShitProcessor& processor;
    layshit::ui::LayShitLookAndFeel lnf;

    juce::OpenGLContext glContext;
    layshit::gfx::BowlRenderer renderer;

    std::vector<std::unique_ptr<ModernKnob>> knobs;
    std::vector<juce::Component*> corePage;
    std::array<std::vector<juce::Component*>, layshit::numModules> modulePages;
    int currentPage = layshit::modCrystals;

    ModernKnob* timeKnob = nullptr;
    juce::Slider bristolSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bristolAtt;
    juce::ToggleButton syncButton { "SYNC" }, pingPongButton { "PING-PONG" }, freezeButton { "FREEZE" };
    juce::ComboBox syncDivBox, shimIntervalBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAtt, pingPongAtt, freezeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncDivAtt, shimIntervalAtt;

    layshit::ui::ChainStrip chain;
    layshit::ui::PresetBar presetBar;
    layshit::ui::FlushButton flushButton;
    juce::TooltipWindow tooltips { this, 600 };

    float holy = 0.0f;
    bool occupied = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LayShitEditor)
};
