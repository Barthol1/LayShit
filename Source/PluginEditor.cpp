#include "PluginEditor.h"
#include "Presets.h"

using namespace layshit;
using LnF = ui::LayShitLookAndFeel;

namespace
{
constexpr int editorW = 1100, editorH = 800;
constexpr int cellW = 76, cellH = 100;

const juce::Rectangle<int> leftCard { 16, 84, 260, 396 };
const juce::Rectangle<int> presetArea { 370, 20, 360, 44 };
const juce::Rectangle<int> bristolArea { 760, 20, 324, 44 };
const juce::Rectangle<int> chainBar { 16, 704, 1068, 80 };
const juce::Rectangle<int> toastArea { 470, 76, 160, 30 };
constexpr int cardRadius = 20;
}

// ------------------------------------------------------------------ ModernKnob
ModernKnob::ModernKnob (juce::AudioProcessorValueTreeState& s, const char* paramId, const juce::String& n) : name (n)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);

    caption.setJustificationType (juce::Justification::centred);
    caption.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (caption);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
    if (auto* p = s.getParameter (paramId))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64));
    }
    showingValue = true; // force first refresh
    refreshCaption();
}

void ModernKnob::refreshCaption()
{
    const bool hot = slider.isMouseOverOrDragging();
    if (hot)
    {
        caption.setText (slider.getTextFromValue (slider.getValue()), juce::dontSendNotification);
        caption.setFont (LnF::font (12.0f, "Semibold"));
        caption.setColour (juce::Label::textColourId, ui::colours::accent);
    }
    else if (showingValue)
    {
        caption.setText (name.toUpperCase(), juce::dontSendNotification);
        caption.setFont (LnF::font (10.5f, "Semibold").withExtraKerningFactor (0.12f));
        caption.setColour (juce::Label::textColourId, ui::colours::textDim);
    }
    showingValue = hot;
}

void ModernKnob::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromBottom (22));
    slider.setBounds (r.withSizeKeepingCentre (64, 64));
}

// ------------------------------------------------------------------ editor
LayShitEditor::LayShitEditor (LayShitProcessor& p)
    : AudioProcessorEditor (p), processor (p), renderer (glContext, p.apvts, p.visual)
{
    setLookAndFeel (&lnf);
    auto& s = processor.apvts;

    timeKnob = &addKnob (corePage, pid::time, "Time");
    addKnob (corePage, pid::feedback, "Feedback");
    addKnob (corePage, pid::mix, "Mix");
    addKnob (corePage, pid::fiberHP, "Low Cut");
    addKnob (corePage, pid::fiberLP, "High Cut");
    addKnob (corePage, pid::wobbleDepth, "Wobble");
    addKnob (corePage, pid::wobbleRate, "Rate");
    addKnob (corePage, pid::duckAmount, "Courtesy");
    addKnob (corePage, pid::duckRelease, "Release");

    addKnob (modulePages[modClog], pid::clogDrive, "Drive");
    addKnob (modulePages[modClog], pid::clogCrush, "Crush");
    addKnob (modulePages[modClog], pid::clogMix, "Mix");

    addKnob (modulePages[modShift], pid::shiftHz, "Shift");
    addKnob (modulePages[modShift], pid::shiftMix, "Mix");

    auto& cp = modulePages[modCrystals];
    addKnob (cp, pid::crysAmount, "Amount");
    addKnob (cp, pid::crysSize, "Size");
    addKnob (cp, pid::crysDensity, "Density");
    addKnob (cp, pid::crysSpray, "Spray");
    addKnob (cp, pid::crysPitch, "Pitch");
    addKnob (cp, pid::crysReverse, "Reverse");
    addKnob (cp, pid::shimAmt, "Shimmer");
    cp.push_back (&shimIntervalBox);
    cp.push_back (&freezeButton);

    addKnob (modulePages[modPlunge], pid::plungeDiffuse, "Diffuse");
    addKnob (modulePages[modPlunge], pid::plungeSize, "Size");

    bristolSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bristolSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    bristolSlider.setTooltip ("Bristol scale: 1 = hard gritty lumps, 7 = liquid and smeared");
    addAndMakeVisible (bristolSlider);
    bristolAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, pid::bristol, bristolSlider);
    bristolSlider.setDoubleClickReturnValue (true, 4.0);

    syncDivBox.addItemList (syncDivNames, 1);
    shimIntervalBox.addItemList (shimIntervalNames, 1);
    for (auto* c : std::initializer_list<juce::Component*> { &syncButton, &pingPongButton, &freezeButton, &syncDivBox, &shimIntervalBox })
        addAndMakeVisible (c);
    syncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, pid::sync, syncButton);
    pingPongAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, pid::pingPong, pingPongButton);
    freezeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, pid::crysFreeze, freezeButton);
    syncDivAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, pid::syncDiv, syncDivBox);
    shimIntervalAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, pid::shimInterval, shimIntervalBox);
    shimIntervalBox.setTooltip ("Shimmer interval");
    freezeButton.setTooltip ("Freeze the grain buffer");
    syncButton.setTooltip ("Sync delay time to host tempo");

    chain.getOrder = [this] { return processor.getModuleOrder(); };
    chain.setOrder = [this] (const ModuleOrder& o) { processor.setModuleOrder (o); repaint(); };
    chain.onSelect = [this] (int m) { showPage (m); };
    chain.getAccent = [this] { return lnf.accentA(); };
    chain.getActivity = [this] (int m)
    {
        auto v = [this] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
        switch (m)
        {
            case modClog: return std::max (v (pid::clogDrive), v (pid::clogCrush)) * v (pid::clogMix);
            case modShift: return std::min (1.0f, std::abs (v (pid::shiftHz)) / 30.0f) * v (pid::shiftMix);
            case modCrystals: return std::max (v (pid::crysAmount), v (pid::shimAmt));
            case modPlunge: return v (pid::plungeDiffuse);
            default: return 0.0f;
        }
    };
    addAndMakeVisible (chain);

    for (auto& pr : factoryPresets())
        presetBar.names.add (pr.name);
    presetBar.getCurrent = [this] { return processor.getCurrentProgram(); };
    presetBar.onChoose = [this] (int i) { processor.setCurrentProgram (i); presetBar.repaint(); };
    addAndMakeVisible (presetBar);

    flushButton.onFlush = [this] { processor.requestFlush(); };
    flushButton.getAccent = [this] { return lnf.accentA(); };
    addAndMakeVisible (flushButton);

    showPage (currentPage);
    chain.setSelected (currentPage);

    glContext.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    glContext.setRenderer (&renderer);
    glContext.setContinuousRepainting (true);
    glContext.setComponentPaintingEnabled (true);
    glContext.attachTo (*this);

    setupSnapshotMode();

    setSize (editorW, editorH);
    startTimerHz (30);
}

LayShitEditor::~LayShitEditor()
{
    stopTimer();
    glContext.detach();
    setLookAndFeel (nullptr);
}

ModernKnob& LayShitEditor::addKnob (std::vector<juce::Component*>& page, const char* id, const juce::String& name)
{
    auto k = std::make_unique<ModernKnob> (processor.apvts, id, name);
    auto* raw = k.get();
    addAndMakeVisible (*raw);
    page.push_back (raw);
    knobs.push_back (std::move (k));
    return *raw;
}

void LayShitEditor::showPage (int moduleId)
{
    currentPage = moduleId;
    for (int m = 0; m < numModules; ++m)
        for (auto* c : modulePages[(size_t) m])
            c->setVisible (m == moduleId);
    resized();
    repaint();
}

juce::Rectangle<int> LayShitEditor::rightCardBounds() const
{
    const int rows = ((int) modulePages[(size_t) currentPage].size() + 2) / 3;
    return { editorW - 16 - 260, 84, 260, 52 + rows * cellH + 12 };
}

void LayShitEditor::layoutGrid (const std::vector<juce::Component*>& items, juce::Point<int> origin)
{
    for (size_t i = 0; i < items.size(); ++i)
    {
        juce::Rectangle<int> r (origin.x + (int) (i % 3) * cellW, origin.y + (int) (i / 3) * cellH, cellW, cellH);
        auto* c = items[i];
        if (dynamic_cast<ModernKnob*> (c) == nullptr)
            r = r.withSizeKeepingCentre (cellW - 8, 26).translated (0, -11);
        c->setBounds (r);
    }
}

void LayShitEditor::resized()
{
    layoutGrid (corePage, { leftCard.getX() + 16, leftCard.getY() + 84 });
    const auto rc = rightCardBounds();
    layoutGrid (modulePages[(size_t) currentPage], { rc.getX() + 16, rc.getY() + 52 });

    syncButton.setBounds (leftCard.getX() + 16, leftCard.getY() + 50, 60, 26);
    syncDivBox.setBounds (leftCard.getX() + 82, leftCard.getY() + 50, 72, 26);
    pingPongButton.setBounds (leftCard.getX() + 160, leftCard.getY() + 50, 84, 26);

    presetBar.setBounds (presetArea);
    bristolSlider.setBounds (bristolArea.getX() + 84, bristolArea.getY() + 6, 168, bristolArea.getHeight() - 12);

    chain.setBounds (chainBar.getX() + 140, chainBar.getY() + 6, chainBar.getWidth() - 140 - 92, chainBar.getHeight() - 12);
    flushButton.setBounds (chainBar.getRight() - 78, chainBar.getY() + 6, 54, 70);

    pushGlass();
}

void LayShitEditor::pushGlass()
{
    std::vector<gfx::BowlRenderer::GlassRect> g {
        { leftCard.toFloat(), (float) cardRadius },
        { rightCardBounds().toFloat(), (float) cardRadius },
        { chainBar.toFloat(), (float) cardRadius },
        { presetArea.toFloat(), presetArea.getHeight() * 0.5f },
        { bristolArea.toFloat(), bristolArea.getHeight() * 0.5f },
    };
    if (occupied)
        g.push_back ({ toastArea.toFloat(), toastArea.getHeight() * 0.5f });
    renderer.setGlass (std::move (g));
}

void LayShitEditor::paint (juce::Graphics& g)
{
    // the GL compositor already drew the frosted panels; here go the outlines and type
    using namespace juce;
    const auto accent = lnf.accentA();

    for (auto r : { leftCard, rightCardBounds(), chainBar })
        LnF::drawGlassOutline (g, r.toFloat(), (float) cardRadius);
    LnF::drawGlassOutline (g, bristolArea.toFloat(), bristolArea.getHeight() * 0.5f);

    // wordmark
    g.setFont (LnF::font (30.0f, "Bold").withExtraKerningFactor (-0.02f));
    g.setGradientFill (ColourGradient (lnf.accentB(), 28.0f, 30.0f, accent, 150.0f, 30.0f, false));
    g.drawText ("layshit", 26, 14, 220, 38, Justification::centredLeft);
    g.setFont (LnF::font (11.0f, "Semibold").withExtraKerningFactor (0.14f));
    g.setColour (ui::colours::textDim);
    g.drawText (holy > 0.5f ? "THE DELAY YOU LAY  /  ASCENDED" : "THE DELAY YOU LAY", 28, 50, 300, 16, Justification::centredLeft);

    auto cardTitle = [&] (Rectangle<int> card, const String& eyebrow, const String& title)
    {
        g.setFont (LnF::font (10.0f, "Semibold").withExtraKerningFactor (0.16f));
        g.setColour (accent);
        g.drawText (eyebrow, card.getX() + 18, card.getY() + 14, 220, 12, Justification::centredLeft);
        g.setFont (LnF::font (17.0f, "Semibold"));
        g.setColour (ui::colours::text);
        g.drawText (title, card.getX() + 18, card.getY() + 26, 220, 20, Justification::centredLeft);
    };
    cardTitle (leftCard, "CORE", "Delay");
    const auto rc = rightCardBounds();
    cardTitle (rc, "MODULE " + String (chain.positionOf (currentPage)).paddedLeft ('0', 2) + " IN THE LOOP",
               ui::ChainStrip::moduleName (currentPage));

    // Bristol pill captions
    g.setFont (LnF::font (10.0f, "Semibold").withExtraKerningFactor (0.16f));
    g.setColour (ui::colours::textDim);
    g.drawText ("BRISTOL", bristolArea.getX() + 20, bristolArea.getY(), 70, bristolArea.getHeight(), Justification::centredLeft);
    const float bristol = processor.apvts.getRawParameterValue (pid::bristol)->load();
    g.setFont (LnF::font (13.0f, "Semibold"));
    g.setColour (ui::colours::text);
    g.drawText ("Type " + String (bristol, 1), bristolArea.getRight() - 70, bristolArea.getY(), 56, bristolArea.getHeight(),
                Justification::centredRight);

    // chain bar caption
    g.setFont (LnF::font (10.0f, "Semibold").withExtraKerningFactor (0.16f));
    g.setColour (accent);
    g.drawText ("SIGNAL CHAIN", chainBar.getX() + 20, chainBar.getY() + 24, 120, 12, Justification::centredLeft);
    g.setFont (LnF::font (12.0f));
    g.setColour (ui::colours::textDim);
    g.drawText ("drag to reorder", chainBar.getX() + 20, chainBar.getY() + 40, 120, 16, Justification::centredLeft);

    // occupied toast
    if (occupied)
    {
        LnF::drawGlassOutline (g, toastArea.toFloat(), toastArea.getHeight() * 0.5f);
        g.setColour (Colour (0xffff5a4f));
        g.fillEllipse ((float) toastArea.getX() + 16.0f, (float) toastArea.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setFont (LnF::font (11.0f, "Semibold").withExtraKerningFactor (0.16f));
        g.setColour (ui::colours::text);
        g.drawText ("OCCUPIED", toastArea.withTrimmedLeft (32), Justification::centredLeft);
    }
}

void LayShitEditor::timerCallback()
{
    for (auto& k : knobs)
        k->refreshCaption();

    const bool synced = processor.apvts.getRawParameterValue (pid::sync)->load() > 0.5f;
    timeKnob->setEnabled (! synced);
    syncDivBox.setEnabled (synced);

    const float h = holyAmount (processor.apvts.getRawParameterValue (pid::crysAmount)->load(),
                                processor.apvts.getRawParameterValue (pid::shimAmt)->load());
    const bool occ = renderer.occupied.load();
    if (std::abs (h - holy) > 0.02f || occ != occupied)
    {
        holy = h;
        lnf.setHoly (h);
        if (occ != occupied)
        {
            occupied = occ;
            pushGlass();
        }
        repaint();
    }
    else
        repaint (bristolArea); // keeps "Type x.x" current
}

void LayShitEditor::setupSnapshotMode()
{
    // Dev aid: LAYSHIT_SNAPSHOT=out.png renders a few seconds, saves a screenshot, and quits the standalone app.
    const auto path = juce::SystemStats::getEnvironmentVariable ("LAYSHIT_SNAPSHOT", {});
    if (path.isEmpty())
        return;

    processor.testPulses.store (juce::SystemStats::getEnvironmentVariable ("LAYSHIT_PULSES", {}).isNotEmpty());
    const auto preset = juce::SystemStats::getEnvironmentVariable ("LAYSHIT_PRESET", {});
    if (preset.isNotEmpty())
        processor.setCurrentProgram (preset.getIntValue());
    const auto page = juce::SystemStats::getEnvironmentVariable ("LAYSHIT_PAGE", {});
    if (page.isNotEmpty())
    {
        showPage (juce::jlimit (0, numModules - 1, page.getIntValue()));
        chain.setSelected (currentPage);
    }
    const auto drive = juce::SystemStats::getEnvironmentVariable ("LAYSHIT_DRIVE", {});
    if (drive.isNotEmpty())
        if (auto* prm = processor.apvts.getParameter (pid::clogDrive))
            prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, drive.getFloatValue()));
    if (juce::SystemStats::getEnvironmentVariable ("LAYSHIT_FLUSH_AT", {}).isNotEmpty())
    {
        const int ms = juce::SystemStats::getEnvironmentVariable ("LAYSHIT_FLUSH_AT", "0").getIntValue();
        juce::Timer::callAfterDelay (ms, [sp = juce::Component::SafePointer<LayShitEditor> (this)]
                                     { if (sp != nullptr) sp->processor.requestFlush(); });
    }

    renderer.snapshotAtSeconds = juce::SystemStats::getEnvironmentVariable ("LAYSHIT_SECONDS", "3").getFloatValue();
    renderer.onSnapshot = [sp = juce::Component::SafePointer<LayShitEditor> (this), path] (juce::Image gl)
    {
        juce::MessageManager::callAsync ([sp, path, gl]
        {
            if (sp == nullptr) return;
            const auto uiImage = sp->createComponentSnapshot (sp->getLocalBounds(), true, 1.0f);
            juce::Image out (juce::Image::ARGB, sp->getWidth(), sp->getHeight(), true);
            {
                juce::Graphics g (out);
                g.drawImage (gl, out.getBounds().toFloat());
                g.drawImageAt (uiImage, 0, 0);
            }
            juce::File f (path);
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (out, os);
            os.flush();
            if (juce::JUCEApplicationBase::isStandaloneApp())
                juce::JUCEApplicationBase::quit();
        });
    };
}
