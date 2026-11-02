#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace layshit::ui
{
namespace colours
{
    inline const juce::Colour glass { 0x9a0d1014 };
    inline const juce::Colour glassStroke { 0x1cffffff };
    inline const juce::Colour text { 0xfff2f4f7 };
    inline const juce::Colour textDim { 0xff8a93a3 };
    inline const juce::Colour textFaint { 0xff5b6372 };
    inline const juce::Colour accent { 0xffffb547 };
    inline const juce::Colour accent2 { 0xffff7a45 };
    inline const juce::Colour gold { 0xffffe08a };
    inline const juce::Colour knobFaceTop { 0xff272c35 };
    inline const juce::Colour knobFaceBottom { 0xff15181d };
    inline const juce::Colour track { 0x1affffff };
    inline const juce::Colour chip { 0x0fffffff };
}

class LayShitLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LayShitLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle,
                           juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;

    /** Accent shifts toward gold as the toilet gets holy. */
    void setHoly (float h) { holy = h; }
    juce::Colour accentA() const { return colours::accent.interpolatedWith (colours::gold, holy); }
    juce::Colour accentB() const { return colours::accent2.interpolatedWith (colours::accent, holy); }

    static juce::Font font (float height, const char* style = "Regular");
    static void drawGlassOutline (juce::Graphics&, juce::Rectangle<float>, float radius);

private:
    float holy = 0.0f;
};
}
