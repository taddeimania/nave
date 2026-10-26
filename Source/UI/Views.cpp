#include "Views.h"
#include "../Presets.h"

namespace nave::ui
{
using namespace juce;

namespace
{
constexpr int kSegments = 16;

int categoryIndex (const char* name)
{
    const auto& c = categories();
    for (int i = 0; i < (int) c.size(); ++i)
        if (String (c[(size_t) i]) == name) return i;
    return 0;
}
} // namespace

// ===========================================================================
// Meter
// ===========================================================================
void Meter::push (float peak)
{
    level = std::max (peak, level * 0.82f);
    if (peak >= hold) { hold = peak; holdFrames = 24; }
    else if (--holdFrames < 0) hold *= 0.9f;

    auto segs = [] (float v)
    {
        if (v <= 0.0f) return 0;
        const float db = Decibels::gainToDecibels (v);
        return jlimit (0, kSegments, (int) std::ceil ((db + 48.0f) / 48.0f * kSegments));
    };
    const int s = segs (level), h = segs (hold);
    if (s != shownSegs || h != shownHold)
    {
        shownSegs = s;
        shownHold = h;
        repaint();
    }
}

void Meter::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float segH = r.getHeight() / (float) kSegments;
    for (int i = 0; i < kSegments; ++i)
    {
        const auto seg = Rectangle<float> (r.getX(), r.getBottom() - (float) (i + 1) * segH, r.getWidth(), segH).reduced (0.0f, 1.5f);
        const Colour c = i >= kSegments - 1 ? colours::ledRed : i >= kSegments - 4 ? colours::ledYellow : colours::ledGreen;
        const bool lit = i < shownSegs || i == shownHold - 1;
        g.setColour (lit ? c : colours::socket);
        g.fillRect (seg);
    }
}

// ===========================================================================
// LcdReadout
// ===========================================================================
void LcdReadout::set (const String& big, const String& small, bool led, Colour c)
{
    if (big != bigText || small != smallText || led != ledOn || c != ledCol)
    {
        bigText = big;
        smallText = small;
        ledOn = led;
        ledCol = c;
        repaint();
    }
}

void LcdReadout::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    drawLcdBox (g, r);
    if (caption.isNotEmpty())
        drawText (g, caption, { r.getX() + 8.0f, r.getY() + 4.0f, r.getWidth() - 30.0f, 12.0f }, 9.5f, colours::inkDim, true,
                  Justification::centredLeft);
    drawLed (g, { r.getRight() - 12.0f, r.getY() + 11.0f }, 4.5f, ledCol, ledOn);
    const float smallH = smallText.isNotEmpty() ? 16.0f : 0.0f;
    auto body = r.reduced (8.0f, 4.0f).withTrimmedTop (caption.isNotEmpty() ? 10.0f : 0.0f);
    if (smallH > 0.0f)
        drawText (g, smallText, body.removeFromBottom (smallH), 10.5f, colours::inkDim, true, Justification::centred);
    g.setColour (bigColour);
    g.setFont (panelFont (bigH, true));
    g.drawFittedText (bigText, body.toNearestInt(), Justification::centred, 1, 0.7f);
}

// ===========================================================================
// TypeLcd
// ===========================================================================
void TypeLcd::set (int type)
{
    if (type != shown)
    {
        shown = type;
        repaint();
    }
}

void TypeLcd::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    drawLcdBox (g, r);
    if (hover)
    {
        g.setColour (colours::inkDim);
        g.drawRect (r, 1.5f);
    }
    if (shown < 0)
        return;
    const auto& info = typeInfo (shown);
    const int cat = categoryIndex (info.category);

    drawLed (g, { r.getX() + 16.0f, r.getY() + 16.0f }, 5.0f, colours::category (cat), true);
    drawText (g, info.category, { r.getX() + 28.0f, r.getY() + 8.0f, 200.0f, 16.0f }, 11.5f, colours::category (cat), true,
              Justification::centredLeft);
    drawText (g, String (shown + 1).paddedLeft ('0', 2) + " / " + String (NumTypes),
              { r.getRight() - 112.0f, r.getY() + 8.0f, 100.0f, 16.0f }, 11.0f, colours::inkDim, true, Justification::centredRight);

    g.setColour (colours::lcd);
    g.setFont (panelFont (40.0f, true));
    g.drawFittedText (String (info.name).toUpperCase(), r.reduced (14.0f, 0.0f).withTrimmedTop (22.0f).withTrimmedBottom (20.0f).toNearestInt(),
                      Justification::centred, 1, 0.6f);

    // A magnifier and a hint along the bottom.
    const auto hint = Rectangle<float> (r.getX(), r.getBottom() - 22.0f, r.getWidth(), 16.0f);
    const float hx = hint.getCentreX() - 70.0f, hy = hint.getCentreY();
    g.setColour (hover ? colours::ink : colours::inkDim);
    g.drawEllipse (Rectangle<float> (9.0f, 9.0f).withCentre ({ hx, hy - 1.0f }), 1.5f);
    g.drawLine (hx + 3.2f, hy + 2.2f, hx + 7.0f, hy + 6.0f, 1.8f);
    drawText (g, "CLICK TO SEARCH TYPES", { hx + 12.0f, hint.getY(), 180.0f, 16.0f }, 10.0f, hover ? colours::ink : colours::inkDim,
              true, Justification::centredLeft);
}

// ===========================================================================
// TailDisplay
// ===========================================================================
class TailDisplay::Prober final : public Thread
{
public:
    Prober() : Thread ("Nave tail") { startThread (Thread::Priority::low); }
    ~Prober() override
    {
        cancel.store (true);
        signalThreadShouldExit();
        notify();
        stopThread (5000);
    }

    void request (const nave::EngineSettings& s)
    {
        const ScopedLock sl (lock);
        pending = s;
        has = true;
        at = Time::getMillisecondCounter();
        cancel.store (true);
        notify();
    }

    bool fetch (ImpulseView& out, int& seenVersion)
    {
        const ScopedLock sl (lock);
        if (version == seenVersion) return false;
        out = result;
        seenVersion = version;
        return true;
    }

    bool busy() const
    {
        const ScopedLock sl (lock);
        return has || working.load();
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            wait (30);
            nave::EngineSettings s;
            bool go = false;
            {
                const ScopedLock sl (lock);
                if (has && Time::getMillisecondCounter() - at > 110)
                {
                    s = pending;
                    has = false;
                    go = true;
                    cancel.store (false);
                }
            }
            if (! go) continue;
            working.store (true);
            auto v = renderImpulse (s, 32000.0f, 30.0f, &cancel);
            working.store (false);
            if (v.valid && ! cancel.load())
            {
                const ScopedLock sl (lock);
                result = std::move (v);
                ++version;
            }
        }
    }

private:
    CriticalSection lock;
    nave::EngineSettings pending;
    bool has = false;
    uint32 at = 0;
    std::atomic<bool> cancel { false }, working { false };
    ImpulseView result;
    int version = 0;
};

namespace
{
// Only what changes the tail itself.
bool sameTail (const nave::EngineSettings& a, const nave::EngineSettings& b)
{
    auto near = [] (float x, float y) { return std::abs (x - y) <= 1.0e-4f * std::max (1.0f, std::abs (x)); };
    return a.type == b.type && near (a.decay, b.decay) && near (a.size, b.size) && near (a.predelayMs, b.predelayMs)
        && near (a.diffusion, b.diffusion) && near (a.early, b.early) && near (a.a, b.a) && near (a.b, b.b) && near (a.c, b.c)
        && a.mode == b.mode && near (a.tone, b.tone) && near (a.lowEnd, b.lowEnd) && near (a.midDb, b.midDb)
        && near (a.lowCutHz, b.lowCutHz) && near (a.highCutHz, b.highCutHz) && near (a.modDepth, b.modDepth)
        && near (a.modRate, b.modRate);
}

String secs (float s)
{
    if (s <= 0.0f) return "-";
    if (s < 1.0f) return String (roundToInt (s * 1000.0f)) + " ms";
    return String (s, s < 10.0f ? 2 : 1) + " s";
}
} // namespace

TailDisplay::TailDisplay() : prober (std::make_unique<Prober>()) {}
TailDisplay::~TailDisplay() { prober.reset(); }

void TailDisplay::push (const nave::EngineSettings& s, float wetPeak, bool isHold, int mode)
{
    bool dirty = false;
    if (! askedOnce || ! sameTail (s, asked))
    {
        asked = s;
        askedOnce = true;
        prober->request (s);
    }
    const bool busy = prober->busy();
    if (busy != measuring) { measuring = busy; dirty = true; }

    if (prober->fetch (target, seen) && target.valid)
    {
        if (full.empty())
        {
            full = target.full; low = target.low; high = target.high;
            seconds = target.seconds;
        }
        predelayMs = s.predelayMs;
        dirty = true;
    }
    // Ease towards the new curve.
    if (! full.empty() && target.valid)
    {
        float moved = 0.0f;
        for (size_t i = 0; i < full.size(); ++i)
        {
            const float df = target.full[i] - full[i], dl = target.low[i] - low[i], dh = target.high[i] - high[i];
            full[i] += 0.35f * df; low[i] += 0.35f * dl; high[i] += 0.35f * dh;
            moved = std::max (moved, std::max (std::abs (df), std::max (std::abs (dl), std::abs (dh))));
        }
        const float ds = target.seconds - seconds;
        seconds += 0.35f * ds;
        if (moved > 0.05f || std::abs (ds) > 0.001f * target.seconds) dirty = true;
    }

    const float w = std::max (wetPeak, wet * 0.85f);
    if (std::abs (Decibels::gainToDecibels (w + 1.0e-6f) - Decibels::gainToDecibels (wet + 1.0e-6f)) > 0.5f) dirty = true;
    wet = w;
    if (isHold != hold || mode != holdMode) { hold = isHold; holdMode = mode; dirty = true; }
    if (measuring && (++frames % 8) == 0) dirty = true;
    if (dirty) repaint();
}

void TailDisplay::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    drawLcdBox (g, r);

    auto top = Rectangle<float> (r.getX() + 12.0f, r.getY() + 6.0f, r.getWidth() - 24.0f, 18.0f);
    auto plot = r.reduced (12.0f, 0.0f).withTrimmedTop (32.0f).withTrimmedBottom (24.0f).withTrimmedLeft (34.0f).withTrimmedRight (22.0f);

    constexpr float floorDb = -66.0f;
    auto yOf = [&] (float db) { return plot.getY() + plot.getHeight() * clampf (db / floorDb, 0.0f, 1.0f); };
    const float span = std::max (0.05f, seconds);
    auto xOf = [&] (float t) { return plot.getX() + plot.getWidth() * clampf (t / span, 0.0f, 1.0f); };

    // Grid.
    for (int db = 0; db >= -60; db -= 20)
    {
        const float y = yOf ((float) db);
        g.setColour (colours::socket.withAlpha (0.7f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        drawText (g, String (db), { r.getX() + 4.0f, y - 7.0f, 38.0f, 14.0f }, 9.5f, colours::inkDim, true, Justification::centredRight);
    }
    {
        static const float steps[] { 0.01f, 0.02f, 0.05f, 0.1f, 0.2f, 0.25f, 0.5f, 1.0f, 2.0f, 2.5f, 5.0f, 10.0f };
        float step = 10.0f;
        for (float s : steps)
            if (span / s <= 8.0f) { step = s; break; }
        for (int k = 0; (float) k * step <= span * 1.0001f; ++k)
        {
            const float t = (float) k * step;
            const float x = xOf (t);
            g.setColour (colours::socket.withAlpha (0.45f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            const String label = k == 0 ? String ("0")
                               : step < 1.0f ? String (roundToInt (t * 1000.0f)) + " ms" : String (roundToInt (t)) + " s";
            drawText (g, label, { x - 30.0f, plot.getBottom() + 4.0f, 60.0f, 14.0f }, 9.5f, colours::inkDim, true, Justification::centred);
        }
    }

    // Curves.
    if (! full.empty())
    {
        const int n = (int) full.size();
        auto path = [&] (const std::vector<float>& v, bool closed)
        {
            Path p;
            for (int i = 0; i < n; ++i)
            {
                const float x = plot.getX() + plot.getWidth() * ((float) i + 0.5f) / (float) n;
                const float y = yOf (v[(size_t) i]);
                if (i == 0)
                {
                    if (closed) { p.startNewSubPath (plot.getX(), plot.getBottom()); p.lineTo (x, y); }
                    else p.startNewSubPath (x, y);
                }
                else p.lineTo (x, y);
            }
            if (closed) { p.lineTo (plot.getRight(), plot.getBottom()); p.closeSubPath(); }
            return p;
        };
        g.setColour (colours::lcd.withAlpha (0.16f));
        g.fillPath (path (full, true));
        g.setColour (colours::plates.withAlpha (0.9f));
        g.strokePath (path (low, false), PathStrokeType (1.4f));
        g.setColour (colours::brand.withAlpha (0.9f));
        g.strokePath (path (high, false), PathStrokeType (1.4f));
        g.setColour (colours::lcd);
        g.strokePath (path (full, false), PathStrokeType (2.0f));
    }

    // Pre-delay marker.
    if (predelayMs > 2.0f && predelayMs * 0.001f < span)
    {
        const float x = xOf (predelayMs * 0.001f);
        Path dash;
        dash.startNewSubPath (x, plot.getY());
        dash.lineTo (x, plot.getBottom());
        const float pattern[] { 4.0f, 4.0f };
        Path dashed;
        PathStrokeType (1.0f).createDashedStroke (dashed, dash, pattern, 2);
        g.setColour (colours::inkDim);
        g.fillPath (dashed);
        drawText (g, "PRE-DELAY " + ids::millis (predelayMs), { x + 4.0f, plot.getBottom() - 14.0f, 120.0f, 12.0f }, 9.5f,
                  colours::inkDim, true, Justification::centredLeft);
    }

    // The live wet level along the right edge.
    {
        const auto bar = Rectangle<float> (plot.getRight() + 8.0f, plot.getY(), 6.0f, plot.getHeight());
        g.setColour (colours::socket);
        g.fillRect (bar);
        const float db = Decibels::gainToDecibels (wet + 1.0e-6f, -100.0f) + 6.0f;
        if (db > floorDb)
        {
            g.setColour (colours::brand);
            g.fillRect (bar.withTop (yOf (std::min (0.0f, db))));
        }
    }

    // Readings.
    if (target.valid)
    {
        String t;
        t << "RT60 " << secs (target.rt60) << "     LOWS " << secs (target.rtLow) << "     HIGHS " << secs (target.rtHigh);
        drawText (g, t, top, 12.5f, colours::lcd, true, Justification::centredLeft);
        String right = measuring ? "MEASURING..." : "60 dB DOWN AT " + secs (target.length);
        drawText (g, right, top, 11.0f, measuring ? colours::brand : colours::inkDim, true, Justification::centredRight);
    }
    else
        drawText (g, "MEASURING...", top, 12.0f, colours::brand, true, Justification::centredLeft);

    // Legend, bottom right of the plot.
    {
        float x = plot.getRight() - 250.0f;
        const float y = plot.getY() + 6.0f;
        auto item = [&] (Colour c, const String& name)
        {
            g.setColour (c);
            g.fillRect (Rectangle<float> (x, y + 5.0f, 14.0f, 2.5f));
            drawText (g, name, { x + 18.0f, y, 70.0f, 12.0f }, 9.5f, colours::inkDim, true, Justification::centredLeft);
            x += 84.0f;
        };
        item (colours::lcd, "WHOLE");
        item (colours::plates, "LOWS");
        item (colours::brand, "HIGHS");
    }

    if (hold)
    {
        const auto banner = plot.withSizeKeepingCentre (330.0f, 50.0f);
        g.setColour (colours::black.withAlpha (0.85f));
        g.fillRect (banner);
        g.setColour (colours::brand);
        g.drawRect (banner, 1.5f);
        drawText (g, holdMode == 1 ? "HOLD: FROZEN" : "HOLD: INFINITE", banner, 22.0f, colours::brand, true);
    }
}

// ===========================================================================
// Browser
// ===========================================================================
class Browser::Grid final : public Component
{
public:
    explicit Grid (Browser& b) : owner (b) {}

    static constexpr int kCols = 4, kTileH = 74, kGap = 10;

    Rectangle<int> tile (int i) const
    {
        const int w = (getWidth() - (kCols - 1) * kGap) / kCols;
        return { (i % kCols) * (w + kGap), (i / kCols) * (kTileH + kGap), w, kTileH };
    }
    int heightFor (int count) const { return std::max (1, (count + kCols - 1) / kCols) * (kTileH + kGap); }

    int itemAt (Point<int> p) const
    {
        for (int i = 0; i < (int) owner.items.size(); ++i)
            if (tile (i).contains (p)) return i;
        return -1;
    }

    void paint (Graphics& g) override
    {
        const int curType = owner.proc.geti (ids::type);
        const int curPreset = owner.proc.presetNumber();
        const int numFactory = (int) factoryPresets().size();
        if (owner.items.empty())
        {
            drawText (g, owner.presetsMode ? "No presets match." : "No types match. Try fewer letters, or a word like ROOM, DARK or DRUMS.",
                      getLocalBounds().toFloat().withHeight (60.0f), 14.0f, colours::inkDim, true);
            return;
        }
        for (int i = 0; i < (int) owner.items.size(); ++i)
        {
            const auto& it = owner.items[(size_t) i];
            const auto r = tile (i).toFloat();
            if (! g.clipRegionIntersects (r.toNearestInt())) continue;
            const bool lit = i == owner.highlight;
            const bool current = (it.kind == 0 && it.index == curType)
                              || (it.kind == 1 && it.index + 1 == curPreset)
                              || (it.kind == 2 && numFactory + it.index + 1 == curPreset);
            g.setColour (lit ? colours::cream : colours::black);
            g.fillRect (r);
            g.setColour (current ? colours::brand : (lit ? colours::cream : colours::inkDim.withAlpha (0.5f)));
            g.drawRect (r, current ? 2.0f : 1.0f);
            g.setColour (colours::category (it.cat));
            g.fillRect (r.withWidth (5.0f).reduced (0.0f, 1.0f));

            const Colour ink = lit ? colours::black : colours::ink;
            const Colour dim = lit ? colours::black.withAlpha (0.6f) : colours::inkDim;
            auto body = r.reduced (14.0f, 7.0f);
            auto head = body.removeFromTop (20.0f);
            drawText (g, it.category, head, 9.5f, lit ? colours::black.withAlpha (0.6f) : colours::category (it.cat), true,
                      Justification::centredRight);
            drawText (g, it.name.toUpperCase(), head.withTrimmedRight (80.0f), 15.5f, ink, true, Justification::centredLeft);
            g.setColour (dim);
            g.setFont (panelFont (11.0f, false));
            g.drawFittedText (it.blurb, body.toNearestInt(), Justification::topLeft, 2, 1.0f);
        }
    }

    void mouseMove (const MouseEvent& e) override
    {
        const int i = itemAt (e.getPosition());
        if (i >= 0 && i != owner.highlight) { owner.highlight = i; repaint(); }
    }
    void mouseDown (const MouseEvent& e) override
    {
        const int i = itemAt (e.getPosition());
        if (i >= 0) owner.choose (i);
    }

private:
    Browser& owner;
};

Browser::Browser (NaveProcessor& p) : proc (p), grid (std::make_unique<Grid> (*this))
{
    setWantsKeyboardFocus (false);
    search.setFont (panelFont (22.0f, true));
    search.setTextToShowWhenEmpty ("TYPE TO SEARCH: HALL, DARK, DRUMS, 80S, OCTAVE...", colours::inkDim);
    search.setIndents (14, 10);
    search.setJustification (Justification::centredLeft);
    search.addListener (this);
    search.addKeyListener (this);
    search.setColour (TextEditor::highlightColourId, colours::brand.withAlpha (0.4f));
    search.setColour (CaretComponent::caretColourId, colours::lcd);
    addAndMakeVisible (search);

    typesTab.onClick = [this] (const ModifierKeys&) { presetsMode = false; rebuild(); search.grabKeyboardFocus(); };
    presetsTab.onClick = [this] (const ModifierKeys&) { presetsMode = true; rebuild(); search.grabKeyboardFocus(); };
    closeBtn.onClick = [this] (const ModifierKeys&) { if (onClose) onClose(); };
    for (auto* b : { &typesTab, &presetsTab, &closeBtn })
    {
        b->setFontHeight (12.0f);
        addAndMakeVisible (*b);
    }
    typesTab.setTooltip ("Search the reverb types.");
    presetsTab.setTooltip ("Search the presets, factory and yours.");
    closeBtn.setTooltip ("Close the browser (Escape).");

    for (int c = 0; c < 7; ++c)
    {
        auto& b = catBtns[(size_t) c];
        b.setLabel (c == 0 ? "ALL" : categories()[(size_t) (c - 1)]);
        b.setFontHeight (11.0f);
        b.onClick = [this, c] (const ModifierKeys&) { category = c - 1; rebuild(); search.grabKeyboardFocus(); };
        addAndMakeVisible (b);
    }

    view.setViewedComponent (grid.get(), false);
    view.setScrollBarsShown (true, false);
    view.setScrollBarThickness (8);
    view.getVerticalScrollBar().setColour (ScrollBar::thumbColourId, colours::inkDim);
    addAndMakeVisible (view);
}

Browser::~Browser()
{
    search.removeKeyListener (this);
}

void Browser::open (bool presets)
{
    presetsMode = presets;
    category = -1;
    search.clear();
    rebuild();
    setVisible (true);
    toFront (true);
    search.grabKeyboardFocus();
}

void Browser::resized()
{
    search.setBounds (16, 16, 640, 48);
    typesTab.setBounds (676, 20, 110, 40);
    presetsTab.setBounds (792, 20, 110, 40);
    closeBtn.setBounds (getWidth() - 126, 20, 110, 40);
    for (int c = 0; c < 7; ++c)
        catBtns[(size_t) c].setBounds (16 + c * 128, 80, 120, 30);
    view.setBounds (16, 126, getWidth() - 32, getHeight() - 142);
    grid->setSize (view.getWidth() - 12, grid->heightFor ((int) items.size()));
}

void Browser::paint (Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::ink.withAlpha (0.85f));
    g.drawRect (getLocalBounds().toFloat(), 1.5f);
    const String hint = String ((int) items.size()) + (presetsMode ? " PRESETS" : " TYPES")
                      + "     ARROWS MOVE   RETURN PICKS   ESC CLOSES";
    drawText (g, hint, { 920.0f, 80.0f, (float) getWidth() - 936.0f, 30.0f }, 10.5f, colours::inkDim, true, Justification::centredRight);
}

void Browser::rebuild()
{
    typesTab.setLit (! presetsMode);
    presetsTab.setLit (presetsMode);
    for (int c = 0; c < 7; ++c)
        catBtns[(size_t) c].setLit (category == c - 1);

    const std::string q = search.getText().toStdString();
    struct Scored { Item item; int score; int order; };
    std::vector<Scored> found;

    if (! presetsMode)
    {
        for (int t = 0; t < NumTypes; ++t)
        {
            const auto& info = typeInfo (t);
            const int cat = categoryIndex (info.category);
            if (category >= 0 && cat != category) continue;
            const int s = matchScore (q, info.name, info.category, info.blurb, info.keywords);
            if (s <= 0) continue;
            found.push_back ({ { 0, t, {}, info.name, info.category, info.blurb, cat }, s, t });
        }
    }
    else
    {
        // A preset matches on its own words and on its type's.
        const auto& f = factoryPresets();
        for (int i = 0; i < (int) f.size(); ++i)
        {
            int type = TConcertHall;
            for (const auto& [id, v] : f[(size_t) i].values)
                if (String (id) == ids::type) type = roundToInt (v);
            const auto& info = typeInfo (type);
            const int cat = categoryIndex (info.category);
            if (category >= 0 && cat != category) continue;
            const int s = matchScore (q, f[(size_t) i].name, info.name, f[(size_t) i].blurb, std::string (info.category) + " " + info.keywords);
            if (s <= 0) continue;
            found.push_back ({ { 1, i, {}, f[(size_t) i].name, String (info.name).toUpperCase(), f[(size_t) i].blurb, cat }, s, i });
        }
        const auto users = userPresets();
        for (int i = 0; i < users.size(); ++i)
        {
            const auto name = users[i].getFileNameWithoutExtension().toStdString();
            int type = TConcertHall;
            if (auto xml = XmlDocument::parse (users[i]))
                for (auto* child : xml->getChildWithTagNameIterator ("PARAM"))
                    if (child->getStringAttribute ("id") == ids::type) type = roundToInt (child->getDoubleAttribute ("value"));
            type = jlimit (0, NumTypes - 1, type);
            const auto& info = typeInfo (type);
            const int cat = categoryIndex (info.category);
            if (category >= 0 && cat != category) continue;
            const int s = matchScore (q, name, info.name, "", std::string (info.category) + " " + info.keywords);
            if (s <= 0) continue;
            found.push_back ({ { 2, i, users[i], name, "YOURS: " + String (info.name).toUpperCase(), "Your preset.", cat }, s, 1000 + i });
        }
    }

    if (! q.empty())
        std::stable_sort (found.begin(), found.end(), [] (const Scored& a, const Scored& b) { return a.score > b.score; });

    items.clear();
    for (auto& s : found) items.push_back (s.item);

    // Start on the current type or preset when nothing's typed.
    highlight = 0;
    if (q.empty())
    {
        const int curType = proc.geti (ids::type), curPreset = proc.presetNumber();
        for (int i = 0; i < (int) items.size(); ++i)
            if ((! presetsMode && items[(size_t) i].index == curType)
                || (presetsMode && items[(size_t) i].kind == 1 && items[(size_t) i].index + 1 == curPreset))
                highlight = i;
    }
    grid->setSize (std::max (1, view.getWidth() - 12), grid->heightFor ((int) items.size()));
    view.setViewPosition (0, 0);
    if (! items.empty())
        view.setViewPosition (0, std::max (0, grid->tile (highlight).getBottom() - view.getHeight()));
    grid->repaint();
    repaint();
}

void Browser::moveHighlight (int delta)
{
    if (items.empty()) return;
    highlight = jlimit (0, (int) items.size() - 1, highlight + delta);
    const auto t = grid->tile (highlight);
    if (t.getY() < view.getViewPositionY())
        view.setViewPosition (0, t.getY());
    else if (t.getBottom() > view.getViewPositionY() + view.getHeight())
        view.setViewPosition (0, t.getBottom() - view.getHeight());
    grid->repaint();
}

bool Browser::keyPressed (const KeyPress& k, Component*)
{
    if (k == KeyPress::downKey)  { moveHighlight (Grid::kCols); return true; }
    if (k == KeyPress::upKey)    { moveHighlight (-Grid::kCols); return true; }
    if (k == KeyPress::tabKey)   { presetsMode = ! presetsMode; rebuild(); return true; }
    // Left and right move through the results unless you're editing the text.
    if (k == KeyPress::rightKey && search.getCaretPosition() == search.getTotalNumChars()) { moveHighlight (1); return true; }
    if (k == KeyPress::leftKey && search.getCaretPosition() == search.getTotalNumChars() && search.getTotalNumChars() == 0)
    {
        moveHighlight (-1);
        return true;
    }
    if (k == KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    return false;
}

void Browser::choose (int i)
{
    if (! isPositiveAndBelow (i, (int) items.size()))
        return;
    const auto it = items[(size_t) i];
    if (it.kind == 0) proc.selectType (it.index);
    else if (it.kind == 1) proc.loadFactory (it.index);
    else proc.loadUserPreset (it.file);
    if (onClose) onClose();
}
} // namespace nave::ui
