#include "PresetBar.h"
#include "LayShitLookAndFeel.h"

namespace layshit::ui
{
namespace
{
constexpr float arrowW = 38.0f;
}

PresetBar::PresetBar()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

PresetBar::Zone PresetBar::zoneAt (juce::Point<float> p) const
{
    if (! getLocalBounds().toFloat().contains (p)) return none;
    if (p.x < arrowW) return prev;
    if (p.x > (float) getWidth() - arrowW) return next;
    return name;
}

void PresetBar::step (int delta)
{
    if (names.isEmpty() || ! onChoose) return;
    const int cur = getCurrent ? getCurrent() : 0;
    onChoose ((cur + delta + names.size()) % names.size());
}

void PresetBar::paint (juce::Graphics& g)
{
    using namespace juce;
    using LnF = LayShitLookAndFeel;
    const auto b = getLocalBounds().toFloat();
    LnF::drawGlassOutline (g, b, b.getHeight() * 0.5f);

    auto drawArrow = [&] (Rectangle<float> r, bool left, bool hot)
    {
        if (hot)
        {
            g.setColour (colours::chip.withMultipliedAlpha (2.0f));
            g.fillEllipse (r.withSizeKeepingCentre (28.0f, 28.0f));
        }
        Path p;
        const auto c = r.getCentre();
        const float s = left ? 1.0f : -1.0f;
        p.startNewSubPath (c.x + 2.5f * s, c.y - 5.0f);
        p.lineTo (c.x - 2.5f * s, c.y);
        p.lineTo (c.x + 2.5f * s, c.y + 5.0f);
        g.setColour (hot ? colours::text : colours::textDim);
        g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    };
    drawArrow (b.withWidth (arrowW), true, hover == prev);
    drawArrow (b.withTrimmedLeft (b.getWidth() - arrowW), false, hover == next);

    // tiny toilet-paper roll icon
    auto mid = b.reduced (arrowW, 0.0f);
    const int cur = getCurrent ? getCurrent() : -1;
    const String label = isPositiveAndBelow (cur, names.size()) ? names[cur] : String ("Presets");
    g.setFont (LnF::font (14.0f, "Semibold"));
    const float textW = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label);
    const float groupW = 18.0f + 8.0f + textW;
    const float gx = mid.getCentreX() - groupW * 0.5f;
    const float iy = mid.getCentreY();
    g.setColour (Colour (0xffeae7df));
    g.fillRoundedRectangle (gx, iy - 7.0f, 14.0f, 14.0f, 3.0f);
    g.fillRect (gx + 7.0f, iy + 3.0f, 9.0f, 5.0f);
    g.setColour (Colour (0xff9a958a));
    g.fillEllipse (gx + 4.5f, iy - 2.5f, 5.0f, 5.0f);

    g.setColour (hover == name ? colours::accent : colours::text);
    g.drawText (label, Rectangle<float> (gx + 26.0f, mid.getY(), textW + 4.0f, mid.getHeight()), Justification::centredLeft);
}

void PresetBar::mouseUp (const juce::MouseEvent& e)
{
    switch (zoneAt (e.position))
    {
        case prev: step (-1); break;
        case next: step (1); break;
        case name:
        {
            juce::PopupMenu menu;
            const int cur = getCurrent ? getCurrent() : -1;
            for (int i = 0; i < names.size(); ++i)
                menu.addItem (i + 1, names[i], true, i == cur);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                                [sp = juce::Component::SafePointer<PresetBar> (this)] (int r)
                                {
                                    if (sp != nullptr && r > 0 && sp->onChoose) sp->onChoose (r - 1);
                                    if (sp != nullptr) sp->repaint();
                                });
            break;
        }
        case none: break;
    }
    repaint();
}

void PresetBar::mouseMove (const juce::MouseEvent& e)
{
    const auto z = zoneAt (e.position);
    if (z != hover) { hover = z; repaint(); }
}

void PresetBar::mouseExit (const juce::MouseEvent&)
{
    hover = none;
    repaint();
}
}
