// Params.h — Nave's host parameters. Every knob, button and choice is
// automatable and can be MIDI-mapped in Live.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/Engine.h"

namespace nave::ids
{
// Type
inline constexpr const char* type = "type";

// Space
inline constexpr const char* decay     = "decay";
inline constexpr const char* size      = "size";
inline constexpr const char* predelay  = "predelay";
inline constexpr const char* sync      = "sync";
inline constexpr const char* division  = "division";
inline constexpr const char* diffusion = "diffusion";
inline constexpr const char* early     = "early";

// Character: their meaning depends on the type.
inline constexpr const char* charA    = "charA";
inline constexpr const char* charB    = "charB";
inline constexpr const char* charC    = "charC";
inline constexpr const char* charMode = "charMode";

// Tone
inline constexpr const char* tone    = "tone";
inline constexpr const char* lowEnd  = "lowEnd";
inline constexpr const char* mid     = "mid";
inline constexpr const char* lowCut  = "lowCut";
inline constexpr const char* highCut = "highCut";

// Motion
inline constexpr const char* modDepth = "modDepth";
inline constexpr const char* modRate  = "modRate";

// Dynamics
inline constexpr const char* duck    = "duck";
inline constexpr const char* gate    = "gate";
inline constexpr const char* release = "release";

// Hold
inline constexpr const char* hold     = "hold";
inline constexpr const char* holdMode = "holdMode";

// Output
inline constexpr const char* input   = "input";
inline constexpr const char* mix     = "mix";
inline constexpr const char* width   = "width";
inline constexpr const char* output  = "output";
inline constexpr const char* mixLock = "mixLock";

// ---------------------------------------------------------------------------
inline juce::StringArray typeNames()
{
    juce::StringArray n;
    for (const auto& t : nave::types()) n.add (juce::String (t.name).toUpperCase());
    return n;
}
inline const juce::StringArray& holdModeNames() { static const juce::StringArray n { "INFINITE", "FREEZE" }; return n; }
inline juce::StringArray modeSlots()
{
    juce::StringArray n;
    for (int i = 1; i <= nave::kMaxModes; ++i) n.add (juce::String (i));
    return n;
}

// Pre-delay note values when SYNC is on.
struct Division { const char* name; double beats; };
inline const std::vector<Division>& divisions()
{
    static const std::vector<Division> d {
        { "1/64", 1.0 / 16 }, { "1/32T", 1.0 / 12 }, { "1/32", 1.0 / 8 }, { "1/16T", 1.0 / 6 }, { "1/16", 0.25 },
        { "1/16D", 0.375 }, { "1/8T", 1.0 / 3 }, { "1/8", 0.5 }, { "1/8D", 0.75 }, { "1/4T", 2.0 / 3 },
        { "1/4", 1.0 }, { "1/4D", 1.5 }, { "1/2", 2.0 } };
    return d;
}
inline juce::StringArray divisionNames()
{
    juce::StringArray n;
    for (const auto& d : divisions()) n.add (d.name);
    return n;
}

inline juce::String percent (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
inline juce::String decibels (float v)
{
    if (v <= -59.95f) return "OFF";
    const float r = std::round (v * 10.0f) / 10.0f;
    return (r > 0.0f ? "+" : "") + juce::String (r, 1) + " dB";
}
inline juce::String millis (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms";
}
inline juce::String seconds (float v)
{
    if (v < 1.0f) return juce::String (juce::roundToInt (v * 1000.0f)) + " ms";
    if (v < 10.0f) return juce::String (v, 2) + " s";
    return juce::String (v, 1) + " s";
}
inline juce::String hertz (float v)
{
    if (v >= 1000.0f) return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String (v < 10.0f ? juce::String (v, 2) : juce::String (juce::roundToInt (v))) + " Hz";
}
inline juce::String signedInt (float v)
{
    const int i = juce::roundToInt (v);
    return i == 0 ? juce::String ("0") : (i > 0 ? "+" : "") + juce::String (i);
}
} // namespace nave::ids

namespace nave
{
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto num = [] (const char* id, const char* name, NormalisableRange<float> range, float def,
                   std::function<String (float)> text)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
            AudioParameterFloatAttributes().withStringFromValueFunction ([text] (float v, int) { return text (v); }));
    };
    auto pct = [&] (const char* id, const char* name, float def) { return num (id, name, { 0.0f, 1.0f }, def, ids::percent); };
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };
    auto choice = [] (const char* id, const char* name, const StringArray& items, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def);
    };
    // On/off switches are two-step choices: they read back exactly what was saved.
    auto toggle = [] (const char* id, const char* name, bool def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, StringArray { "Off", "On" }, def ? 1 : 0);
    };

    const auto& hall = typeInfo (TConcertHall).start;

    layout.add (choice (ids::type, "Type", ids::typeNames(), TConcertHall));

    layout.add (num (ids::decay, "Decay", skewed (0.1f, 60.0f, 3.0f), hall.decay, ids::seconds));
    layout.add (pct (ids::size, "Size", hall.size));
    layout.add (num (ids::predelay, "Pre-delay", skewed (0.0f, 1500.0f, 120.0f), 20.0f, ids::millis));
    layout.add (toggle (ids::sync, "Pre-delay Sync", false));
    layout.add (choice (ids::division, "Pre-delay Note", ids::divisionNames(), 4));
    layout.add (pct (ids::diffusion, "Diffusion", hall.diffusion));
    layout.add (pct (ids::early, "Early", hall.early));

    layout.add (pct (ids::charA, "Character A", hall.a));
    layout.add (pct (ids::charB, "Character B", hall.b));
    layout.add (pct (ids::charC, "Character C", hall.c));
    layout.add (choice (ids::charMode, "Character Choice", ids::modeSlots(), hall.mode));

    layout.add (pct (ids::tone, "Tone", 0.5f));
    layout.add (num (ids::lowEnd, "Low End", { -10.0f, 10.0f }, 0.0f, ids::signedInt));
    layout.add (num (ids::mid, "Mid", { -12.0f, 12.0f }, 0.0f, ids::decibels));
    layout.add (num (ids::lowCut, "Low Cut", skewed (20.0f, 1000.0f, 120.0f), 20.0f,
                     [] (float v) { return v <= 20.5f ? String ("OFF") : ids::hertz (v); }));
    layout.add (num (ids::highCut, "High Cut", skewed (1000.0f, 20000.0f, 6000.0f), 20000.0f,
                     [] (float v) { return v >= 19900.0f ? String ("OFF") : ids::hertz (v); }));

    layout.add (pct (ids::modDepth, "Mod Depth", 0.3f));
    layout.add (num (ids::modRate, "Mod Rate", skewed (0.05f, 8.0f, 0.8f), 0.6f, ids::hertz));

    layout.add (pct (ids::duck, "Duck", 0.0f));
    layout.add (num (ids::gate, "Gate", { -80.0f, -20.0f }, -80.0f,
                     [] (float v) { return v <= -79.5f ? String ("OFF") : String (roundToInt (v)) + " dB"; }));
    layout.add (num (ids::release, "Release", skewed (20.0f, 2000.0f, 300.0f), 300.0f, ids::millis));

    layout.add (toggle (ids::hold, "Hold", false));
    layout.add (choice (ids::holdMode, "Hold Mode", ids::holdModeNames(), 0));

    layout.add (num (ids::input, "Input", { -24.0f, 12.0f }, 0.0f, ids::decibels));
    layout.add (num (ids::mix, "Mix", { 0.0f, 1.0f }, 0.3f, [] (float v) { return String (roundToInt (v * 100.0f)) + "% wet"; }));
    layout.add (num (ids::width, "Width", { 0.0f, 1.5f }, 1.0f, ids::percent));
    layout.add (num (ids::output, "Output", { -24.0f, 12.0f }, 0.0f, ids::decibels));
    layout.add (toggle (ids::mixLock, "Keep Mix", false));
    return layout;
}
} // namespace nave
