// Presets.h — factory presets, and user presets saved as files.
//
// User presets live in a folder next to the settings file:
//   macOS:   ~/Library/Application Support/Nave/Presets
//   Windows: %APPDATA%\Nave\Presets
//   Linux:   ~/.config/Nave/Presets
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace nave
{
struct FactoryPreset
{
    const char* name;
    const char* blurb;                                   // one line, shown under the preset name
    std::vector<std::pair<const char*, float>> values;   // plain values; anything not listed is default
};

const std::vector<FactoryPreset>& factoryPresets();

juce::File userPresetFolder();
juce::Array<juce::File> userPresets();   // sorted by name
inline constexpr const char* kPresetExtension = ".nave";
} // namespace nave
