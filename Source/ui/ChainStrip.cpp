#include "ChainStrip.h"
#include "LayShitLookAndFeel.h"

namespace layshit::ui
{
namespace
{
constexpr float gap = 26.0f;
constexpr float sideLabel = 44.0f;
}

ChainStrip::ChainStrip()
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    startTimerHz (30);
}

juce::String ChainStrip::moduleName (int id)
{
    switch (id)
    {
        case modClog: return "Clog";
        case modShift: return "Shift";
        case modCrystals: return "Crystals";
        case modPlunge: return "Plunge";
        default: return {};
    }
}

void ChainStrip::setSelected (int id)
{
    selected = id;
    repaint();
}

int ChainStrip::positionOf (int id) const
{
    for (int k = 0; k < numModules; ++k)
        if (order[(size_t) k] == id) return k + 1;
    return 0;
}

juce::Rectangle<float> ChainStrip::cardsArea() const
{
    return getLocalBounds().toFloat().reduced (sideLabel, 10.0f);
}

juce::Rectangle<float> ChainStrip::slotBounds (int slot) const
{
    const auto a = cardsArea();
    const float w = (a.getWidth() - gap * (float) (numModules - 1)) / (float) numModules;
    return { a.getX() + (w + gap) * (float) slot, a.getY(), w, a.getHeight() };
}

int ChainStrip::slotAt (float x) const
{
    for (int s = 0; s < numModules; ++s)
        if (x < slotBounds (s).getRight() + gap * 0.5f) return s;
    return numModules - 1;
}

void ChainStrip::timerCallback()
{
    flowPhase += 0.025f;
    if (flowPhase > 1.0f) flowPhase -= 1.0f;
    if (draggingSlot < 0 && getOrder) order = getOrder();
    repaint();
}

void ChainStrip::paint (juce::Graphics& g)
{
    using namespace juce;
    using LnF = LayShitLookAndFeel;
    const Colour accent = getAccent ? getAccent() : colours::accent;
    const auto a = cardsArea();
    const float cy = a.getCentreY();

    // hairline signal path with flowing dots
    g.setColour (colours::track);
    g.fillRect (Rectangle<float> (6.0f, cy - 0.5f, (float) getWidth() - 12.0f, 1.0f));
    const float spacing = 22.0f;
    for (float x = 6.0f + flowPhase * spacing; x < (float) getWidth() - 6.0f; x += spacing)
    {
        g.setColour (accent.withAlpha (0.35f));
        g.fillEllipse (x - 1.5f, cy - 1.5f, 3.0f, 3.0f);
    }

    g.setFont (LnF::font (10.0f, "Semibold").withExtraKerningFactor (0.15f));
    g.setColour (colours::textDim);
    g.drawText ("IN", Rectangle<float> (0.0f, cy - 16.0f, sideLabel, 12.0f), Justification::centred);
    g.drawText ("LOOP", Rectangle<float> ((float) getWidth() - sideLabel, cy - 16.0f, sideLabel, 12.0f), Justification::centred);

    // chevrons between cards
    for (int s = 0; s + 1 < numModules; ++s)
    {
        const float x = slotBounds (s).getRight() + gap * 0.5f;
        Path chev;
        chev.startNewSubPath (x - 2.5f, cy - 4.0f);
        chev.lineTo (x + 2.5f, cy);
        chev.lineTo (x - 2.5f, cy + 4.0f);
        g.setColour (colours::textDim);
        g.strokePath (chev, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    auto drawCard = [&] (int slot, int moduleId, Rectangle<float> r, bool lifted)
    {
        const bool sel = moduleId == selected;
        const float act = getActivity ? jlimit (0.0f, 1.0f, getActivity (moduleId)) : 0.0f;
        if (lifted) r = r.translated (0.0f, -3.0f);

        g.setColour (Colour (0xe0181b21));
        g.fillRoundedRectangle (r, 12.0f);
        if (sel)
        {
            g.setColour (accent.withAlpha (0.1f));
            g.fillRoundedRectangle (r, 12.0f);
        }
        g.setColour (sel ? accent.withAlpha (0.85f) : (lifted ? Colour (0x40ffffff) : colours::glassStroke));
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, sel ? 1.4f : 1.0f);

        auto inner = r.reduced (14.0f, 9.0f);
        g.setFont (LnF::font (10.0f, "Semibold").withExtraKerningFactor (0.1f));
        g.setColour (sel ? accent : colours::textFaint);
        g.drawText (String (slot + 1).paddedLeft ('0', 2), inner.removeFromLeft (22.0f), Justification::centredLeft);
        g.setFont (LnF::font (15.0f, "Semibold"));
        g.setColour (sel ? colours::text : colours::text.withAlpha (0.85f));
        g.drawText (moduleName (moduleId), inner, Justification::centredLeft);

        // activity meter
        auto meter = r.reduced (14.0f, 0.0f).removeFromBottom (6.0f).withHeight (2.0f);
        g.setColour (colours::track);
        g.fillRoundedRectangle (meter, 1.0f);
        if (act > 0.005f)
        {
            g.setGradientFill (ColourGradient (colours::accent2, meter.getX(), 0, accent, meter.getRight(), 0, false));
            g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * act), 1.0f);
        }

        // grip dots
        g.setColour (Colour (0x30ffffff));
        for (int i = 0; i < 3; ++i)
            for (int k = 0; k < 2; ++k)
                g.fillEllipse (r.getRight() - 16.0f + (float) k * 4.0f, r.getCentreY() - 5.0f + (float) i * 4.0f, 1.8f, 1.8f);
    };

    for (int s = 0; s < numModules; ++s)
        if (s != draggingSlot)
            drawCard (s, order[(size_t) s], slotBounds (s), s == hoverSlot);

    if (draggingSlot >= 0)
    {
        auto r = slotBounds (draggingSlot);
        r.setX (dragX - dragOffset);
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r.translated (0.0f, 6.0f), 12.0f);
        drawCard (draggingSlot, order[(size_t) draggingSlot], r, true);
    }
}

void ChainStrip::mouseDown (const juce::MouseEvent& e)
{
    if (! cardsArea().contains (e.position)) return;
    draggingSlot = slotAt (e.position.x);
    dragOffset = e.position.x - slotBounds (draggingSlot).getX();
    dragX = e.position.x;
    moved = false;
}

void ChainStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingSlot < 0) return;
    dragX = e.position.x;
    if (e.getDistanceFromDragStart() > 4) moved = true;
    const int target = slotAt (dragX - dragOffset + slotBounds (0).getWidth() * 0.5f);
    if (target != draggingSlot)
    {
        std::swap (order[(size_t) target], order[(size_t) draggingSlot]);
        draggingSlot = target;
        if (setOrder) setOrder (order);
    }
    repaint();
}

void ChainStrip::mouseUp (const juce::MouseEvent&)
{
    if (draggingSlot >= 0 && ! moved)
    {
        selected = order[(size_t) draggingSlot];
        if (onSelect) onSelect (selected);
    }
    else if (draggingSlot >= 0 && setOrder)
        setOrder (order);
    draggingSlot = -1;
    repaint();
}

void ChainStrip::mouseMove (const juce::MouseEvent& e)
{
    hoverSlot = cardsArea().contains (e.position) ? slotAt (e.position.x) : -1;
}

void ChainStrip::mouseExit (const juce::MouseEvent&) { hoverSlot = -1; }
}
