#include "LayShitLookAndFeel.h"

namespace layshit::ui
{
LayShitLookAndFeel::LayShitLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::textDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::chip);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, colours::glassStroke);
    setColour (juce::ComboBox::arrowColourId, colours::textDim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf4161a20));
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.16f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::accent);
    setColour (juce::TooltipWindow::textColourId, colours::text);
}

juce::Font LayShitLookAndFeel::font (float height, const char* style)
{
    return juce::Font (juce::FontOptions ("Segoe UI", style, height));
}

void LayShitLookAndFeel::drawGlassOutline (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    // the frosted fill comes from the GL compositor; this adds the crisp edge and a top sheen
    g.setColour (colours::glassStroke);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.06f), r.getX(), r.getY(), juce::Colours::transparentWhite,
                                r.getX(), r.getY() + 40.0f, false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r.reduced (1.0f), radius);
}

void LayShitLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle,
                                           float endAngle, juce::Slider& slider)
{
    using namespace juce;
    const float size = (float) jmin (w, h) - 4.0f;
    const auto c = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = size * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool hot = slider.isMouseOverOrDragging();
    const float enabledAlpha = slider.isEnabled() ? 1.0f : 0.35f;

    const float arcR = r - 2.5f;
    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::track);
    g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (from, angle), jmax (from, angle), true);
        const ColourGradient grad (accentB().withMultipliedAlpha (enabledAlpha), c.x - r, c.y + r,
                                   accentA().withMultipliedAlpha (enabledAlpha), c.x + r, c.y - r, false);
        g.setGradientFill (grad);
        g.setOpacity (0.22f);
        g.strokePath (value, PathStrokeType (9.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setOpacity (1.0f);
        g.setGradientFill (grad);
        g.strokePath (value, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // face
    const float faceR = r - 9.0f;
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - faceR, c.y - faceR + 2.0f, faceR * 2.0f, faceR * 2.0f);
    g.setGradientFill (ColourGradient (colours::knobFaceTop.brighter (hot ? 0.15f : 0.0f), c.x, c.y - faceR,
                                       colours::knobFaceBottom, c.x, c.y + faceR, false));
    g.fillEllipse (c.x - faceR, c.y - faceR, faceR * 2.0f, faceR * 2.0f);
    g.setColour (Colours::white.withAlpha (hot ? 0.14f : 0.07f));
    g.drawEllipse (c.x - faceR + 0.5f, c.y - faceR + 0.5f, faceR * 2.0f - 1.0f, faceR * 2.0f - 1.0f, 1.0f);

    // indicator
    const Point<float> a = c.getPointOnCircumference (faceR * 0.35f, angle);
    const Point<float> b = c.getPointOnCircumference (faceR * 0.78f, angle);
    g.setColour ((hot ? accentA() : colours::text).withMultipliedAlpha (enabledAlpha));
    g.drawLine ({ a, b }, 2.4f);
}

void LayShitLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                           juce::Slider::SliderStyle, juce::Slider& slider)
{
    using namespace juce;
    // stepped scale (used by the Bristol macro): 7 stops on a hairline
    const auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float cy = r.getCentreY();
    const float x0 = r.getX() + 8.0f, x1 = r.getRight() - 8.0f;
    g.setColour (colours::track);
    g.fillRoundedRectangle (x0, cy - 1.5f, x1 - x0, 3.0f, 1.5f);
    g.setGradientFill (ColourGradient (accentB(), x0, cy, accentA(), x1, cy, false));
    g.fillRoundedRectangle (x0, cy - 1.5f, pos - x0, 3.0f, 1.5f);

    const int stops = 7;
    for (int i = 0; i < stops; ++i)
    {
        const float sx = x0 + (x1 - x0) * (float) i / (float) (stops - 1);
        const bool passed = sx <= pos + 0.5f;
        g.setColour (passed ? accentA() : Colour (0x40ffffff));
        g.fillEllipse (sx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
    }

    const bool hot = slider.isMouseOverOrDragging();
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillEllipse (pos - 8.0f, cy - 7.0f, 16.0f, 16.0f);
    g.setColour (hot ? colours::text : Colour (0xffd9dde3));
    g.fillEllipse (pos - 7.5f, cy - 7.5f, 15.0f, 15.0f);
    g.setColour (accentB());
    g.fillEllipse (pos - 3.0f, cy - 3.0f, 6.0f, 6.0f);
}

void LayShitLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const float rad = r.getHeight() * 0.5f;
    g.setColour (on ? accentA().withAlpha (0.16f) : colours::chip.withMultipliedAlpha (highlighted ? 1.8f : 1.0f));
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? accentA().withAlpha (0.8f) : colours::glassStroke);
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    g.setColour (on ? accentA() : (highlighted ? colours::text : colours::textDim));
    g.setFont (font (11.5f, "Semibold").withExtraKerningFactor (0.08f));
    g.drawText (b.getButtonText(), r, Justification::centred);
}

void LayShitLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace juce;
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;
    const bool hot = box.isMouseOver (true);
    g.setColour (colours::chip.withMultipliedAlpha (hot ? 1.8f : 1.0f));
    g.fillRoundedRectangle (r, rad);
    g.setColour (colours::glassStroke);
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    Path chevron;
    const float ax = (float) w - 15.0f, ay = (float) h * 0.5f;
    chevron.startNewSubPath (ax - 3.5f, ay - 1.5f);
    chevron.lineTo (ax, ay + 2.0f);
    chevron.lineTo (ax + 3.5f, ay - 1.5f);
    g.setColour (colours::textDim.withAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (chevron, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
}

void LayShitLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 0, box.getWidth() - 24, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font LayShitLookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (12.5f, "Semibold"); }
juce::Font LayShitLookAndFeel::getPopupMenuFont() { return font (14.0f); }

void LayShitLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (colours::glassStroke);
    g.drawRect (0, 0, w, h, 1);
}

void LayShitLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                            bool isHighlighted, bool isTicked, bool, const juce::String& text,
                                            const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    using namespace juce;
    if (isSeparator)
    {
        g.setColour (colours::glassStroke);
        g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (4, 1).toFloat();
    if (isHighlighted && isActive)
    {
        g.setColour (accentA().withAlpha (0.14f));
        g.fillRoundedRectangle (r, 6.0f);
    }
    if (isTicked)
    {
        g.setColour (accentA());
        g.fillEllipse (r.getX() + 8.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
    }
    g.setColour (isTicked || (isHighlighted && isActive) ? accentA() : colours::text.withAlpha (isActive ? 1.0f : 0.4f));
    g.setFont (font (13.5f, isTicked ? "Semibold" : "Regular"));
    g.drawText (text, r.withTrimmedLeft (22.0f), Justification::centredLeft);
}

void LayShitLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (label.isBeingEdited())
        return;
    g.setFont (label.getFont());
    g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (label.isEnabled() ? 1.0f : 0.45f));
    g.drawText (label.getText(), label.getLocalBounds().toFloat(), label.getJustificationType(), true);
}

void LayShitLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (juce::Colour (0xf4161a20));
    g.setColour (colours::glassStroke);
    g.drawRect (0, 0, w, h, 1);
    g.setColour (colours::text);
    g.setFont (font (12.5f));
    g.drawFittedText (text, 8, 4, w - 16, h - 8, juce::Justification::centredLeft, 3);
}
}
