#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "Settings.h"
#include "UI/Views.h"
#include "UI/Widgets.h"

// The panel is laid out in a fixed design space and scaled to fit the window,
// as on Beadwork, Posy, Seyir, Mottle and Descant.
class NavePanel final : public juce::Component, private juce::Timer
{
public:
    static constexpr int designW = 1500;
    static constexpr int designH = 830;

    explicit NavePanel (NaveProcessor&);
    ~NavePanel() override;

    void paint (juce::Graphics&) override;
    void resized() override {}

    std::function<void (bool)> onTooltips;
    void syncTooltips();
    void openBrowser (bool presets);

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using Knob = nave::ui::Knob;
    using PanelButton = nave::ui::PanelButton;
    using ChoiceDisplay = nave::ui::ChoiceDisplay;

    void timerCallback() override;

    void addKnob (Knob&, const char* id, const juce::String& caption, juce::Rectangle<int>, const juce::String& tip);
    void addToggle (PanelButton&, const char* id, juce::Rectangle<int>, const juce::String& tip);
    void addButton (PanelButton&, juce::Rectangle<int>, const juce::String& tip,
                    std::function<void (const juce::ModifierKeys&)> click);
    void addChoice (ChoiceDisplay&, const char* id, const juce::String& caption, juce::Rectangle<int>, const juce::String& tip);
    void addRadio (PanelButton&, const char* id, int value, juce::Rectangle<int>, const juce::String& tip);
    void chooseParam (juce::Component& target, const char* id);
    void chooseMode();
    void refreshType (int type);

    void openPresetMenu();
    void savePresetDialog();

    NaveProcessor& proc;
    nave::ui::PanelLookAndFeel lnf;
    nave::Settings settings;

    // Header
    ChoiceDisplay presetChoice;
    PanelButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, tipsBtn { "TIPS" };

    // Type
    nave::ui::TypeLcd typeLcd;
    PanelButton typePrev { "<" }, typeNext { ">" }, searchBtn { "SEARCH" };
    PanelButton holdBtn { "HOLD" }, infiniteBtn { "INFINITE" }, freezeBtn { "FREEZE" };

    // Tail
    nave::ui::TailDisplay tail;

    // Space
    Knob decayKnob, sizeKnob, predelayKnob, diffusionKnob, earlyKnob;
    ChoiceDisplay divisionChoice;
    PanelButton syncBtn { "SYNC" };

    // Character
    std::array<Knob, 3> charKnobs;
    ChoiceDisplay modeChoice;

    // Motion
    Knob depthKnob, rateKnob;

    // Tone
    Knob toneKnob, lowEndKnob, midKnob, lowCutKnob, highCutKnob;

    // Dynamics
    Knob duckKnob, gateKnob, releaseKnob;

    // Output
    Knob inputKnob, mixKnob, widthKnob, outputKnob;
    nave::ui::Meter inMeter, outMeter;
    PanelButton keepMixBtn { "KEEP MIX" };

    nave::ui::Browser browser;

    struct Radio { PanelButton* button; const char* id; int value; };
    std::vector<Radio> radios;
    std::vector<std::pair<PanelButton*, const char*>> toggles;
    std::vector<ChoiceDisplay*> choices;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::unique_ptr<juce::AlertWindow> saveWindow;

    int tooltipsApplied = -1, shownType = -1;
    juce::String blurb, typeBlurb, charText, dynText;
    bool gateLed = true;
    float duckShown = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NavePanel)
};

class NaveEditor final : public juce::AudioProcessorEditor
{
public:
    explicit NaveEditor (NaveProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    NavePanel panel;
    std::unique_ptr<juce::TooltipWindow> tooltips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NaveEditor)
};
