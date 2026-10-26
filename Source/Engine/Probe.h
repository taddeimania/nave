// Probe.h — renders the current settings' impulse response offline and
// measures it, for the TAIL display: the envelope of the whole tail and of its
// low and high bands, and decay times from the energy decay curve. No JUCE.
#pragma once

#include "Engine.h"
#include <atomic>
#include <vector>

namespace nave
{
struct ImpulseView
{
    static constexpr int kBins = 300;
    float seconds = 1.0f;                 // time span the bins cover
    std::vector<float> full, low, high;   // dB per bin, 0 = the loudest bin of `full`
    float rt60 = 0.0f, rtLow = 0.0f, rtHigh = 0.0f;   // 0 when it can't be measured
    float length = 0.0f;                  // seconds until the tail falls 60 dB below its peak
    float onset = 0.0f;                   // seconds until the first sound
    bool valid = false;
};

// Renders up to `maxSeconds`. Mix, dynamics and HOLD are ignored: the probe
// hears the reverb alone. `cancel` (optional) stops early when set.
ImpulseView renderImpulse (EngineSettings, float sampleRate, float maxSeconds, const std::atomic<bool>* cancel = nullptr);
} // namespace nave
