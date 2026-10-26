// Views.h — Nave's custom panel pieces: level meters, the type display, the
// TAIL display (the real impulse response of the current settings, measured
// in the background) and the type and preset browser.
#pragma once

#include "../Engine/Probe.h"
#include "../PluginProcessor.h"
#include "Widgets.h"

namespace nave::ui
{
// Segmented LED meter with a falling peak, as on Mottle and Descant.
class Meter final : public juce::Component
{
public:
    void push (float peak);   // call from the panel timer
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f, hold = 0.0f;
    int holdFrames = 0, shownSegs = -1, shownHold = -1;
};

// A black LCD with a big line and a small line.
class LcdReadout final : public juce::Component, public juce::SettableTooltipClient
{
public:
    juce::String caption;
    float bigH = 30.0f;
    juce::Colour bigColour = colours::lcd;

    void set (const juce::String& big, const juce::String& small, bool led, juce::Colour ledColour);
    void paint (juce::Graphics&) override;

private:
    juce::String bigText, smallText;
    bool ledOn = false;
    juce::Colour ledCol = colours::ledGreen;
};

// The current type: its category, name and number. Click to search.
class TypeLcd final : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onOpen;
    void set (int type);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onOpen) onOpen(); }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

private:
    int shown = -1;
    bool hover = false;
};

// ---------------------------------------------------------------------------
// TAIL: renders the impulse response of the current settings on a background
// thread whenever they change, and draws its envelope (whole, lows, highs)
// with the decay times measured from it.
// ---------------------------------------------------------------------------
class TailDisplay final : public juce::Component, public juce::SettableTooltipClient
{
public:
    TailDisplay();
    ~TailDisplay() override;

    // From the panel timer.
    void push (const nave::EngineSettings&, float wetPeak, bool hold, int holdMode);
    void paint (juce::Graphics&) override;

private:
    class Prober;
    std::unique_ptr<Prober> prober;
    nave::EngineSettings asked;
    bool askedOnce = false, measuring = false;
    int seen = -1;
    ImpulseView target;
    std::vector<float> full, low, high;   // what's drawn, easing towards target
    float seconds = 3.0f, wet = 0.0f, predelayMs = 0.0f;
    bool hold = false;
    int holdMode = 0, frames = 0;
};

// ---------------------------------------------------------------------------
// The browser: type a few letters to find a type (or a preset), or click
// through the categories. Arrow keys move, Return picks, Escape closes.
// ---------------------------------------------------------------------------
class Browser final : public juce::Component, private juce::TextEditor::Listener, private juce::KeyListener
{
public:
    explicit Browser (NaveProcessor&);
    ~Browser() override;

    std::function<void()> onClose;
    void open (bool presets);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Item
    {
        int kind = 0;           // 0 type, 1 factory preset, 2 user preset
        int index = 0;
        juce::File file;
        juce::String name, category, blurb;
        int cat = 0;
    };
    class Grid;

    void rebuild();
    void choose (int item);
    void moveHighlight (int delta);
    void textEditorTextChanged (juce::TextEditor&) override { rebuild(); }
    void textEditorReturnKeyPressed (juce::TextEditor&) override { choose (highlight); }
    void textEditorEscapeKeyPressed (juce::TextEditor&) override { if (onClose) onClose(); }
    using juce::Component::keyPressed;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;

    NaveProcessor& proc;
    juce::TextEditor search;
    PanelButton typesTab { "TYPES" }, presetsTab { "PRESETS" }, closeBtn { "CLOSE" };
    std::array<PanelButton, 7> catBtns;
    juce::Viewport view;
    std::unique_ptr<Grid> grid;
    std::vector<Item> items;
    int highlight = 0, category = -1;
    bool presetsMode = false;

    friend class Grid;
};
} // namespace nave::ui
