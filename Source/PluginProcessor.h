#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

#include "Engine/Engine.h"
#include "Params.h"

// A stereo reverb: put it on a track (MIX to taste) or on a return track
// (MIX at 100%, with KEEP MIX on so presets don't change it).
class NaveProcessor final : public juce::AudioProcessor
{
public:
    NaveProcessor();
    ~NaveProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    // Factory presets double as the host's programs.
    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, factoryIndex); }
    void setCurrentProgram (int index) override { loadFactory (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- parameters (message thread) ---------------------------------------------
    void setParam (const juce::String& id, float plainValue);
    float getParam (const juce::String& id) const { return params.at (id)->load(); }
    int   geti (const char* id) const { return juce::roundToInt (getParam (id)); }

    // Picks a type and moves SPACE and CHARACTER to where that type starts.
    void selectType (int type);
    void stepType (int delta);

    // The settings as they stand, for the tail display's probe.
    nave::EngineSettings currentSettings() const;

    // --- presets (message thread) --------------------------------------------------
    void loadFactory (int index);
    bool loadUserPreset (const juce::File&);
    bool saveUserPreset (const juce::String& name, juce::String& error);
    void stepPreset (int delta);              // through factory, then user presets
    juce::String presetName() const { return currentName; }
    juce::String presetBlurb() const;
    int  presetNumber() const;                // 1-based across factory and user; 0 if none
    bool presetEdited() const;

    // --- read by the panel --------------------------------------------------------------
    float takeInputPeak()  { return uiIn.exchange (0.0f); }
    float takeOutputPeak() { return uiOut.exchange (0.0f); }
    float takeWetPeak()    { return uiWet.exchange (0.0f); }
    float duckDb() const   { return uiDuck.load(); }
    bool  gateOpen() const { return uiGate.load(); }
    float swellLevel() const { return uiSwell.load(); }
    double bpm() const     { return hostBpm.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    void cacheParams();
    nave::EngineSettings readSettings (bool audioThread) const;
    std::unique_ptr<juce::XmlElement> stateXml();
    void applyState (const juce::XmlElement&);
    std::vector<float> snapshot() const;
    void applyValues (const std::vector<std::pair<juce::String, float>>& plainValues, bool keepMix);

    std::map<juce::String, std::atomic<float>*> params;
    struct
    {
        std::atomic<float> *type, *decay, *size, *predelay, *sync, *division, *diffusion, *early;
        std::atomic<float> *a, *b, *c, *mode;
        std::atomic<float> *tone, *lowEnd, *mid, *lowCut, *highCut, *modDepth, *modRate;
        std::atomic<float> *duck, *gate, *release, *hold, *holdMode;
        std::atomic<float> *input, *mix, *width, *output, *mixLock;
    } fast {};

    nave::Engine engine;
    std::vector<float> silence, scratch;

    // Presets.
    int factoryIndex = 0;
    juce::File userFile;
    juce::String currentName;
    std::vector<float> loadedValues;

    // Audio thread -> panel.
    std::atomic<float> uiIn { 0.0f }, uiOut { 0.0f }, uiWet { 0.0f }, uiDuck { 0.0f }, uiSwell { 1.0f };
    std::atomic<bool> uiGate { true };
    std::atomic<double> hostBpm { 120.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NaveProcessor)
};
