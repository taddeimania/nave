#include "PluginEditor.h"
#include "Presets.h"

using namespace nave;
using namespace nave::ui;
using juce::Rectangle;
using juce::String;

namespace
{
// Section boxes (design space).
const Rectangle<float> kTypeBox   { 12.0f, 86.0f, 548.0f, 260.0f };
const Rectangle<float> kTailBox   { 572.0f, 86.0f, 916.0f, 260.0f };
const Rectangle<float> kSpaceBox  { 12.0f, 366.0f, 628.0f, 210.0f };
const Rectangle<float> kCharBox   { 652.0f, 366.0f, 500.0f, 210.0f };
const Rectangle<float> kMotionBox { 1164.0f, 366.0f, 324.0f, 210.0f };
const Rectangle<float> kToneBox   { 12.0f, 596.0f, 628.0f, 220.0f };
const Rectangle<float> kDynBox    { 652.0f, 596.0f, 360.0f, 220.0f };
const Rectangle<float> kOutBox    { 1024.0f, 596.0f, 464.0f, 220.0f };

const Rectangle<float> kBlurb      { 548.0f, 61.0f, 500.0f, 16.0f };
const Rectangle<float> kTypeText   { 28.0f, 226.0f, 516.0f, 36.0f };
const Rectangle<float> kHoldText   { 270.0f, 272.0f, 150.0f, 58.0f };
const Rectangle<float> kCharText   { 668.0f, 514.0f, 470.0f, 54.0f };
const Rectangle<float> kMotionText { 1180.0f, 514.0f, 296.0f, 54.0f };
const Rectangle<float> kToneText   { 28.0f, 748.0f, 596.0f, 54.0f };
const Rectangle<float> kDynText    { 668.0f, 748.0f, 330.0f, 54.0f };
const Rectangle<float> kOutText    { 1262.0f, 750.0f, 190.0f, 50.0f };

int knobX (int i) { return 28 + i * 120; }

const char* kCaptionDecay = "DECAY", *kCaptionSize = "SIZE", *kCaptionEarly = "EARLY", *kCaptionDiffusion = "DIFFUSION";
} // namespace

// ===========================================================================
NavePanel::NavePanel (NaveProcessor& p) : proc (p), browser (p)
{
    setLookAndFeel (&lnf);
    setSize (designW, designH);

    // --- header ------------------------------------------------------------------
    presetChoice.getText = [this]
    {
        const int n = proc.presetNumber();
        return (n > 0 ? String (n).paddedLeft ('0', 2) + "   " : String()) + proc.presetName().toUpperCase()
             + (proc.presetEdited() ? "  *" : "");
    };
    presetChoice.fontH = 18.0f;
    presetChoice.onOpen = [this] { openPresetMenu(); };
    presetChoice.setTooltip ("Presets. Click for the list, or step with < >. A * means you've changed it since loading.");
    addAndMakeVisible (presetChoice);
    presetChoice.setBounds (594, 12, 408, 46);
    choices.push_back (&presetChoice);

    addButton (prevBtn, { 548, 14, 40, 42 }, "Previous preset.", [this] (const juce::ModifierKeys&) { proc.stepPreset (-1); });
    addButton (nextBtn, { 1008, 14, 40, 42 }, "Next preset.", [this] (const juce::ModifierKeys&) { proc.stepPreset (1); });
    prevBtn.setFontHeight (18.0f);
    nextBtn.setFontHeight (18.0f);
    addButton (saveBtn, { 1062, 18, 66, 34 }, "Save these settings as a preset.",
               [this] (const juce::ModifierKeys&) { savePresetDialog(); });
    saveBtn.setFontHeight (11.0f);
    addButton (tipsBtn, { 1400, 18, 62, 34 }, "Show or hide these tips (remembered for every Nave).",
               [this] (const juce::ModifierKeys&)
               {
                   settings.setTooltipsEnabled (! settings.tooltipsEnabled());
                   syncTooltips();
               });
    tipsBtn.setFontHeight (11.0f);

    // --- type ----------------------------------------------------------------------
    addAndMakeVisible (typeLcd);
    typeLcd.setBounds (80, 106, 412, 112);
    typeLcd.onOpen = [this] { openBrowser (false); };
    typeLcd.setTooltip ("The reverb type. Click to search them all by name or by sound: HALL, DARK, DRUMS, 80S, OCTAVE...");
    addButton (typePrev, { 28, 106, 46, 112 }, "Previous type. Picking a type starts SPACE and CHARACTER where that type sounds best.",
               [this] (const juce::ModifierKeys&) { proc.stepType (-1); });
    addButton (typeNext, { 498, 106, 46, 112 }, "Next type. Picking a type starts SPACE and CHARACTER where that type sounds best.",
               [this] (const juce::ModifierKeys&) { proc.stepType (1); });
    typePrev.setFontHeight (20.0f);
    typeNext.setFontHeight (20.0f);

    addToggle (holdBtn, ids::hold, { 28, 272, 110, 58 },
               "HOLD: the tail stops decaying and plays on for ever. Automate it or map it to a footswitch.");
    holdBtn.setFontHeight (16.0f);
    addRadio (infiniteBtn, ids::holdMode, 0, { 148, 272, 110, 27 }, "INFINITE: what you play next still goes into the held reverb and adds to it.");
    addRadio (freezeBtn, ids::holdMode, 1, { 148, 303, 110, 27 }, "FREEZE: the held reverb is closed off. Play over it without adding to it.");
    infiniteBtn.setFontHeight (11.0f);
    freezeBtn.setFontHeight (11.0f);
    addButton (searchBtn, { 430, 272, 114, 58 }, "Search the types and presets.", [this] (const juce::ModifierKeys&) { openBrowser (false); });
    searchBtn.setFontHeight (13.0f);

    // --- tail ----------------------------------------------------------------------
    addAndMakeVisible (tail);
    tail.setBounds (588, 106, 884, 226);
    tail.setTooltip ("The actual tail of the current settings, measured whenever you change something. "
                     "Amber is the whole reverb, blue its lows, lavender its highs. The bar on the right is the reverb playing now.");

    // --- space ---------------------------------------------------------------------
    const int ky = 386;
    addKnob (decayKnob, ids::decay, kCaptionDecay, { knobX (0), ky, 104, 122 },
             "How long the tail lasts: the time to fall 60 dB. Each type keeps it inside its own range.");
    addKnob (sizeKnob, ids::size, kCaptionSize, { knobX (1), ky, 104, 122 },
             "How big the space is: longer gaps between reflections and a slower build. Separate from DECAY.");
    addKnob (predelayKnob, ids::predelay, "PRE-DELAY", { knobX (2), ky, 104, 122 },
             "A gap before the reverb starts, so the dry sound stays clear in front of it.");
    addChoice (divisionChoice, ids::division, "PRE-DELAY", { knobX (2) + 4, ky + 18, 96, 60 },
               "Pre-delay in note values at the song tempo.");
    divisionChoice.fontH = 18.0f;
    addToggle (syncBtn, ids::sync, { knobX (2) + 14, 518, 76, 28 }, "SYNC: pre-delay in note values at the song tempo.");
    syncBtn.setFontHeight (11.0f);
    addKnob (diffusionKnob, ids::diffusion, kCaptionDiffusion, { knobX (3), ky, 104, 122 },
             "How smeared the reflections are. Low is grainy and you can hear the echoes; high is smooth and dense.");
    addKnob (earlyKnob, ids::early, kCaptionEarly, { knobX (4), ky, 104, 122 },
             "The early reflections: the first bounces off the nearest walls, which tell you the shape of the room.");

    // --- character ----------------------------------------------------------------------
    for (int i = 0; i < 3; ++i)
    {
        static const char* idsList[] { ids::charA, ids::charB, ids::charC };
        addKnob (charKnobs[(size_t) i], idsList[i], "", { 668 + i * 112, ky, 104, 122 }, "");
        charKnobs[(size_t) i].format = [this, i] (double v) { return String (nave::charText (proc.geti (ids::type), i, (float) v)); };
    }
    modeChoice.getText = [this]
    {
        const auto& info = typeInfo (proc.geti (ids::type));
        if (info.modeNames.empty()) return String ("-");
        return String (info.modeNames[(size_t) juce::jlimit (0, (int) info.modeNames.size() - 1, proc.geti (ids::charMode))]);
    };
    modeChoice.fontH = 16.0f;
    modeChoice.onOpen = [this] { chooseMode(); };
    addAndMakeVisible (modeChoice);
    modeChoice.setBounds (1008, ky + 14, 128, 60);
    choices.push_back (&modeChoice);

    // --- motion ---------------------------------------------------------------------------
    addKnob (depthKnob, ids::modDepth, "DEPTH", { 1186, ky, 104, 122 },
             "How much the reverb's delay lines move. Low is natural drift; high is lush chorus.");
    addKnob (rateKnob, ids::modRate, "RATE", { 1300, ky, 104, 122 }, "How fast they move.");

    // --- tone -------------------------------------------------------------------------------
    const int ty = 616;
    addKnob (toneKnob, ids::tone, "TONE", { knobX (0), ty, 104, 122 },
             "How long the highs last. Down is dark and warm (carpets, curtains); up is bright and crisp (stone, tile). Middle is balanced.");
    addKnob (lowEndKnob, ids::lowEnd, "LOW END", { knobX (1), ty, 104, 122 },
             "How long the lows last. Up for a bigger, emptier space; down to keep the bass out of the reverb.");
    lowEndKnob.bipolar = true;
    addKnob (midKnob, ids::mid, "MID", { knobX (2), ty, 104, 122 }, "Boosts or cuts the reverb's midrange around 900 Hz.");
    midKnob.bipolar = true;
    addKnob (lowCutKnob, ids::lowCut, "LOW CUT", { knobX (3), ty, 104, 122 },
             "Takes the low end out of the reverb (not the dry sound). All the way down is off.");
    addKnob (highCutKnob, ids::highCut, "HIGH CUT", { knobX (4), ty, 104, 122 },
             "Takes the top off the reverb (not the dry sound). All the way up is off.");

    // --- dynamics ---------------------------------------------------------------------------
    addKnob (duckKnob, ids::duck, "DUCK", { 668, ty, 104, 122 },
             "Turns the reverb down while you play and lets it bloom in the gaps. Great on vocals.");
    addKnob (gateKnob, ids::gate, "GATE", { 780, ty, 104, 122 },
             "Cuts the reverb off when the input drops below this level. All the way down is off.");
    addKnob (releaseKnob, ids::release, "RELEASE", { 892, ty, 104, 122 },
             "How quickly DUCK lets the reverb come back, and how quickly GATE closes.");

    // --- output -----------------------------------------------------------------------------
    addAndMakeVisible (inMeter);
    inMeter.setBounds (1040, 622, 14, 170);
    inMeter.setInterceptsMouseClicks (false, false);
    addKnob (inputKnob, ids::input, "INPUT", { 1060, ty, 96, 122 }, "Level into Nave. Turn it down if the meter's top lights.");
    addKnob (mixKnob, ids::mix, "MIX", { 1160, ty, 96, 122 },
             "Dry and reverb. At 50% both are at full level; past it the dry sound fades. 100% for a return track.");
    addKnob (widthKnob, ids::width, "WIDTH", { 1260, ty, 96, 122 }, "How wide the reverb is. 0% is mono; past 100% is wider than life.");
    addKnob (outputKnob, ids::output, "OUTPUT", { 1360, ty, 96, 122 }, "Overall output level.");
    addAndMakeVisible (outMeter);
    outMeter.setBounds (1464, 622, 14, 170);
    outMeter.setInterceptsMouseClicks (false, false);
    addToggle (keepMixBtn, ids::mixLock, { 1162, 752, 92, 30 },
               "KEEP MIX: changing preset leaves MIX where it is. Turn it on when Nave is on a return track at 100%.");
    keepMixBtn.setFontHeight (10.5f);

    // --- browser (on top of everything) ----------------------------------------------------------
    addChildComponent (browser);
    browser.setBounds (12, 76, 1476, 746);
    browser.onClose = [this] { browser.setVisible (false); };

    startTimerHz (30);
    timerCallback();
}

NavePanel::~NavePanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NavePanel::openBrowser (bool presets)
{
    browser.open (presets);
}

// ---------------------------------------------------------------------------
void NavePanel::addKnob (Knob& k, const char* id, const String& caption, Rectangle<int> r, const String& tip)
{
    k.caption = caption;
    k.setTooltip (tip);
    if (auto* param = proc.apvts.getParameter (id))
        k.format = [param] (double v) { return param->getText (param->convertTo0to1 ((float) v), 32); };
    addAndMakeVisible (k);
    k.setBounds (r);
    attachments.push_back (std::make_unique<SliderAttachment> (proc.apvts, id, k));
}

void NavePanel::addToggle (PanelButton& b, const char* id, Rectangle<int> r, const String& tip)
{
    addButton (b, r, tip, [this, id] (const juce::ModifierKeys&)
    {
        proc.setParam (id, proc.getParam (id) > 0.5f ? 0.0f : 1.0f);
    });
    toggles.push_back ({ &b, id });
}

void NavePanel::addRadio (PanelButton& b, const char* id, int value, Rectangle<int> r, const String& tip)
{
    addButton (b, r, tip, [this, id, value] (const juce::ModifierKeys&) { proc.setParam (id, (float) value); });
    radios.push_back ({ &b, id, value });
}

void NavePanel::addButton (PanelButton& b, Rectangle<int> r, const String& tip,
                           std::function<void (const juce::ModifierKeys&)> click)
{
    b.setTooltip (tip);
    b.onClick = std::move (click);
    addAndMakeVisible (b);
    b.setBounds (r);
}

void NavePanel::addChoice (ChoiceDisplay& c, const char* id, const String& caption, Rectangle<int> r, const String& tip)
{
    c.caption = caption;
    c.setTooltip (tip);
    if (! c.getText)
        c.getText = [this, id]
        {
            if (auto* param = proc.apvts.getParameter (id))
                return param->getCurrentValueAsText();
            return String();
        };
    c.onOpen = [this, &c, id] { chooseParam (c, id); };
    addAndMakeVisible (c);
    c.setBounds (r);
    choices.push_back (&c);
}

void NavePanel::chooseParam (juce::Component& target, const char* id)
{
    auto* param = proc.apvts.getParameter (id);
    if (param == nullptr)
        return;
    const auto range = proc.apvts.getParameterRange (id);
    const int lo = juce::roundToInt (range.start), hi = juce::roundToInt (range.end);
    const int cur = proc.geti (id);

    juce::PopupMenu m;
    for (int v = lo; v <= hi; ++v)
        m.addItem (param->getText (param->convertTo0to1 ((float) v), 32), true, v == cur,
                   [this, id, v] { proc.setParam (id, (float) v); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target));
}

void NavePanel::chooseMode()
{
    const auto& info = typeInfo (proc.geti (ids::type));
    if (info.modeNames.empty())
        return;
    const int cur = proc.geti (ids::charMode);
    juce::PopupMenu m;
    m.addSectionHeader (info.modeLabel);
    for (int i = 0; i < (int) info.modeNames.size(); ++i)
        m.addItem (info.modeNames[(size_t) i], true, i == cur, [this, i] { proc.setParam (ids::charMode, (float) i); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&modeChoice));
}

// The captions, formats and hints that follow the type.
void NavePanel::refreshType (int type)
{
    shownType = type;
    const auto& info = typeInfo (type);
    auto pick = [] (const char* custom, const char* standard) { return String (*custom != 0 ? custom : standard); };
    decayKnob.caption = pick (info.decayCaption, kCaptionDecay);
    sizeKnob.caption = pick (info.sizeCaption, kCaptionSize);
    earlyKnob.caption = pick (info.earlyCaption, kCaptionEarly);
    diffusionKnob.caption = pick (info.diffusionCaption, kCaptionDiffusion);
    decayKnob.format = [this] (double v)
    {
        const int t = proc.geti (ids::type);
        const float eff = decayFor (t, (float) v);
        if (std::abs (eff - (float) v) < 1.0e-3f) return ids::seconds ((float) v);
        return ids::seconds (eff) + ((float) v > eff ? "  (MAX)" : "  (MIN)");
    };

    String hint;
    for (int i = 0; i < 3; ++i)
    {
        auto& k = charKnobs[(size_t) i];
        const bool used = *info.charLabel[i] != 0;
        k.caption = used ? String (info.charLabel[i]) : String ("-");
        k.setTooltip (used ? String (info.charTips[i]) : String ("Not used by ") + info.name + ".");
        k.setAlpha (used ? 1.0f : 0.3f);
        k.setEnabled (used);
        k.repaint();
    }
    modeChoice.caption = *info.modeLabel != 0 ? String (info.modeLabel) : String ("-");
    modeChoice.setTooltip (*info.modeTip != 0 ? String (info.modeTip) : String ("No choice for ") + info.name + ".");
    modeChoice.setAlpha (info.modeNames.empty() ? 0.3f : 1.0f);
    modeChoice.repaint();
    for (auto* k : { &decayKnob, &sizeKnob, &earlyKnob, &diffusionKnob })
        k->repaint();

    // EARLY means little to types without early reflections.
    const bool hasEarly = info.voicing.machine == Machine::Reflections || info.voicing.machine == Machine::Tape
                       || (info.voicing.machine == Machine::Space && info.voicing.erPattern > 0);
    earlyKnob.setAlpha (hasEarly ? 1.0f : 0.3f);
    earlyKnob.setEnabled (hasEarly);

    // The hint under CHARACTER: the first knob's tip, and the choice's.
    if (*info.charTips[0] != 0) hint << info.charTips[0];
    if (*info.modeTip != 0) hint << (hint.isNotEmpty() ? "  " : "") << info.modeTip;
    else if (*info.charTips[1] != 0) hint << "  " << info.charTips[1];
    charText = hint;
    typeBlurb = info.blurb;
    typeLcd.set (type);
    repaint (kCharText.toNearestInt());
    repaint (kTypeText.toNearestInt());
    repaint (kCharBox.toNearestInt().expanded (2, 10));
}

// ---------------------------------------------------------------------------
void NavePanel::openPresetMenu()
{
    juce::PopupMenu m;
    const int cur = proc.presetNumber();
    m.addItem ("Search presets...", [this] { openBrowser (true); });
    m.addSeparator();
    m.addSectionHeader ("FACTORY");
    const auto& f = factoryPresets();
    for (int i = 0; i < (int) f.size(); ++i)
        m.addItem (String (i + 1).paddedLeft ('0', 2) + "   " + f[(size_t) i].name, true, cur == i + 1,
                   [this, i] { proc.loadFactory (i); });

    const auto users = userPresets();
    m.addSectionHeader ("YOURS");
    if (users.isEmpty())
        m.addItem ("(none yet: press SAVE)", false, false, [] {});
    for (int i = 0; i < users.size(); ++i)
    {
        const auto file = users[i];
        const int n = (int) f.size() + i + 1;
        m.addItem (String (n).paddedLeft ('0', 2) + "   " + file.getFileNameWithoutExtension(), true, cur == n,
                   [this, file] { proc.loadUserPreset (file); });
    }
    m.addSeparator();
    m.addItem ("Save as...", [this] { savePresetDialog(); });
    m.addItem ("Show presets folder", []
    {
        const auto folder = userPresetFolder();
        folder.createDirectory();
        folder.revealToUser();
    });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetChoice));
}

void NavePanel::savePresetDialog()
{
    saveWindow = std::make_unique<juce::AlertWindow> ("Save preset", "Name it. Saving under an existing name replaces it.",
                                                      juce::MessageBoxIconType::NoIcon, this);
    saveWindow->setLookAndFeel (&lnf);
    saveWindow->addTextEditor ("name", proc.presetName());
    saveWindow->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveWindow != nullptr)
        {
            String error;
            if (! proc.saveUserPreset (saveWindow->getTextEditorContents ("name"), error))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save", error);
        }
        saveWindow.reset();
    }), false);
}

void NavePanel::syncTooltips()
{
    const bool on = settings.tooltipsEnabled();
    tipsBtn.setLit (on);
    if ((int) on != tooltipsApplied && onTooltips)
    {
        tooltipsApplied = (int) on;
        onTooltips (on);
    }
}

// ---------------------------------------------------------------------------
void NavePanel::timerCallback()
{
    const int type = proc.geti (ids::type);
    if (type != shownType)
        refreshType (type);

    inMeter.push (proc.takeInputPeak());
    outMeter.push (proc.takeOutputPeak());
    const auto s = proc.currentSettings();
    tail.push (s, proc.takeWetPeak(), s.hold, s.holdMode);

    for (auto* c : choices)
        c->refresh();
    for (auto& [b, id] : toggles)
        b->setLit (proc.getParam (id) > 0.5f);
    for (auto& rd : radios)
        rd.button->setLit (proc.geti (rd.id) == rd.value);

    const bool sync = proc.getParam (ids::sync) > 0.5f;
    predelayKnob.setVisible (! sync);
    divisionChoice.setVisible (sync);

    // Dynamics readout.
    String d;
    const float duckDb = proc.duckDb();
    const bool gateOn = proc.getParam (ids::gate) > -79.5f;
    const bool open = proc.gateOpen();
    if (proc.getParam (ids::duck) > 0.001f)
        d << "DUCKING " << String (juce::roundToInt (duckDb)) << " dB.  ";
    if (gateOn)
        d << (open ? "GATE OPEN." : "GATE SHUT: the input is below GATE.");
    if (d.isEmpty())
        d = "DUCK makes room for the dry sound while it plays. GATE cuts the tail when the playing stops.";
    if (d != dynText || open != gateLed || std::abs (duckDb - duckShown) > 0.5f)
    {
        dynText = d;
        gateLed = open;
        duckShown = duckDb;
        repaint (kDynText.toNearestInt());
        repaint (kDynBox.toNearestInt().removeFromTop (14).expanded (0, 10));
    }

    const String b = proc.presetBlurb();
    if (b != blurb)
    {
        blurb = b;
        repaint (kBlurb.toNearestInt());
    }
}

// ---------------------------------------------------------------------------
void NavePanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    drawText (g, "NAVE", { 24.0f, 14.0f, 140.0f, 46.0f }, 40.0f, colours::brand, true, juce::Justification::centredLeft);
    drawText (g, "reverb suite", { 134.0f, 22.0f, 200.0f, 34.0f }, 17.0f, colours::ink, true, juce::Justification::centredLeft);
    if (blurb.isNotEmpty())
        drawText (g, blurb, kBlurb, 11.0f, colours::inkDim, false);

    drawSection (g, kTypeBox, "TYPE");
    drawSection (g, kTailBox, "TAIL");
    drawSection (g, kSpaceBox, "SPACE");
    drawSection (g, kCharBox, "CHARACTER");
    drawSection (g, kMotionBox, "MOTION");
    drawSection (g, kToneBox, "TONE");
    drawSection (g, kDynBox, "DYNAMICS");
    drawSection (g, kOutBox, "MIX");

    // The type's name on the CHARACTER tab, in its category colour.
    if (shownType >= 0)
    {
        const auto& info = typeInfo (shownType);
        int cat = 0;
        for (int i = 0; i < (int) categories().size(); ++i)
            if (String (categories()[(size_t) i]) == info.category) cat = i;
        const auto f = panelFont (11.0f, true);
        const String n = String (info.name).toUpperCase();
        const float w = juce::GlyphArrangement::getStringWidth (f, n) + 14.0f;
        const auto tab = Rectangle<float> (kCharBox.getX() + 112.0f, kCharBox.getY() - 8.0f, w, 16.0f);
        g.setColour (colours::panel);
        g.fillRect (tab.expanded (3.0f, 0.0f));
        g.setColour (colours::category (cat));
        g.drawRect (tab, 1.0f);
        drawText (g, n, tab, 11.0f, colours::category (cat), true);
    }

    drawText (g, "IN", { 1032.0f, 606.0f, 30.0f, 12.0f }, 10.0f, colours::inkDim, true);
    drawText (g, "OUT", { 1456.0f, 606.0f, 30.0f, 12.0f }, 10.0f, colours::inkDim, true);

    // Gate light by the GATE knob.
    if (proc.getParam (ids::gate) > -79.5f)
        drawLed (g, { 872.0f, 624.0f }, 4.5f, colours::ledGreen, gateLed);

    g.setFont (panelFont (12.5f, false));
    g.setColour (colours::inkDim);
    g.drawFittedText (typeBlurb, kTypeText.toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
    g.drawFittedText ("Holds the tail for ever. INFINITE keeps adding what you play; FREEZE doesn't.",
                      kHoldText.toNearestInt(), juce::Justification::centredLeft, 4, 1.0f);
    g.drawFittedText (charText, kCharText.toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
    g.drawFittedText ("DEPTH and RATE move the reverb's delay lines: a little for natural air, a lot for chorus.",
                      kMotionText.toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
    g.drawFittedText ("TONE and LOW END shape how long highs and lows last inside the reverb. "
                      "MID, LOW CUT and HIGH CUT shape what comes out of it. None of them touch the dry sound.",
                      kToneText.toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
    g.drawFittedText (dynText, kDynText.toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
    g.drawFittedText ("On a return track: MIX 100% and KEEP MIX on.", kOutText.toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
}

// ===========================================================================
NaveEditor::NaveEditor (NaveProcessor& p)
    : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);

    panel.onTooltips = [this] (bool on)
    {
        if (on && tooltips == nullptr)
            tooltips = std::make_unique<juce::TooltipWindow> (this, 700);
        else if (! on)
            tooltips.reset();
    };
    panel.syncTooltips();

    setResizable (true, true);
    const double ratio = (double) NavePanel::designW / NavePanel::designH;
    setResizeLimits (900, (int) (900 / ratio), 2600, (int) (2600 / ratio));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (ratio);
    setSize (1200, (int) (1200 / ratio));
}

void NaveEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
}

void NaveEditor::resized()
{
    const float s = (float) getWidth() / (float) NavePanel::designW;
    panel.setTransform (juce::AffineTransform::scale (s));
    panel.setBounds (0, 0, NavePanel::designW, NavePanel::designH);
}
