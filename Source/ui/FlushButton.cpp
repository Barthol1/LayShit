#include "FlushButton.h"
#include "LayShitLookAndFeel.h"

namespace layshit::ui
{
FlushButton::FlushButton()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Flush: drain the delay buffer");
}

void FlushButton::paint (juce::Graphics& g)
{
    using namespace juce;
    const Colour accent = getAccent ? getAccent() : colours::accent;
    auto b = getLocalBounds().toFloat();
    auto circle = b.removeFromTop (b.getWidth()).reduced (3.0f);
    const auto c = circle.getCentre();
    const float r = circle.getWidth() * 0.5f;
    const bool hot = isMouseOver();

    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (circle.translated (0.0f, 2.0f));
    g.setGradientFill (ColourGradient (colours::knobFaceTop.brighter (hot ? 0.2f : 0.0f), c.x, circle.getY(),
                                       colours::knobFaceBottom, c.x, circle.getBottom(), false));
    g.fillEllipse (circle);
    g.setColour (energy > 0.01f || hot ? accent.withAlpha (0.4f + 0.6f * std::max (energy, hot ? 0.5f : 0.0f)) : colours::glassStroke);
    g.drawEllipse (circle.reduced (0.5f), 1.4f);

    // swirl icon
    Path swirl;
    const int n = 60;
    for (int i = 0; i <= n; ++i)
    {
        const float t = (float) i / (float) n;
        const float a = spin + t * 2.6f * MathConstants<float>::twoPi;
        const float rr = r * 0.55f * (1.0f - t * 0.85f);
        const Point<float> p (c.x + std::cos (a) * rr, c.y + std::sin (a) * rr);
        if (i == 0) swirl.startNewSubPath (p); else swirl.lineTo (p);
    }
    g.setColour (energy > 0.01f || hot ? accent : colours::text.withAlpha (0.8f));
    g.strokePath (swirl, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));

    g.setFont (LayShitLookAndFeel::font (10.0f, "Semibold").withExtraKerningFactor (0.15f));
    g.setColour (hot ? colours::text : colours::textDim);
    g.drawText ("FLUSH", b, Justification::centred);
}

void FlushButton::mouseDown (const juce::MouseEvent&)
{
    if (onFlush) onFlush();
    energy = 1.0f;
    startTimerHz (60);
}

void FlushButton::timerCallback()
{
    spin -= 0.08f + energy * 0.5f;
    energy = std::max (0.0f, energy - 1.0f / 90.0f);
    repaint();
    if (energy <= 0.0f) stopTimer();
}
}
