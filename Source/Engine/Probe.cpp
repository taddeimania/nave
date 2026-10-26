#include "Probe.h"

#include <memory>

namespace nave
{
namespace
{
// Decay time from an energy decay curve (dB per frame), fitting -5 to -25 dB.
float rtFrom (const std::vector<float>& frames, float frameSeconds)
{
    const int n = (int) frames.size();
    if (n < 4)
        return 0.0f;
    std::vector<double> edc ((size_t) n);
    double acc = 0.0;
    for (int i = n - 1; i >= 0; --i)
    {
        acc += frames[(size_t) i];
        edc[(size_t) i] = acc;
    }
    if (acc <= 1.0e-20)
        return 0.0f;
    // Start the fit after the curve's knee: wherever the energy peaks.
    int peak = 0;
    for (int i = 1; i < n; ++i)
        if (frames[(size_t) i] > frames[(size_t) peak]) peak = i;
    const double ref = edc[(size_t) peak];
    int t5 = -1, t25 = -1;
    for (int i = peak; i < n; ++i)
    {
        const double db = 10.0 * std::log10 (std::max (1.0e-30, edc[(size_t) i] / ref));
        if (t5 < 0 && db <= -5.0) t5 = i;
        if (db <= -25.0) { t25 = i; break; }
    }
    if (t5 < 0 || t25 <= t5)
        return 0.0f;
    return 3.0f * (float) (t25 - t5) * frameSeconds;
}
} // namespace

ImpulseView renderImpulse (EngineSettings s, float sampleRate, float maxSeconds, const std::atomic<bool>* cancel)
{
    ImpulseView view;
    s.mix = 1.0f;
    s.duck = 0.0f;
    s.gateDb = -80.0f;
    s.hold = false;
    s.inputDb = 0.0f;
    s.outputDb = 0.0f;
    s.width = 1.0f;

    auto engine = std::make_unique<Engine>();
    engine->prepare (sampleRate, 256);

    const int block = 256;
    const float frameSeconds = 0.005f;
    const int frameLen = std::max (1, (int) (frameSeconds * sampleRate));
    const int total = (int) (maxSeconds * sampleRate);

    Biquad lowL, lowR, highL, highR;
    lowL.lowpass (300.0, 0.707, sampleRate);  lowR = lowL;
    highL.highpass (4000.0, 0.707, sampleRate); highR = highL;

    std::vector<float> fFull, fLow, fHigh;
    fFull.reserve ((size_t) (total / frameLen + 1));
    std::vector<float> l ((size_t) block), r ((size_t) block), ol ((size_t) block), or_ ((size_t) block);
    double eF = 0, eL = 0, eH = 0;
    int inFrame = 0;
    float peakFrame = 0.0f;
    int quietFrames = 0, onsetFrame = -1;

    // Let the smoothers settle on silence first, so the impulse meets the
    // settings rather than their fade-in.
    for (int pre = 0; pre < (int) (0.25f * sampleRate); pre += block)
        engine->process (l.data(), r.data(), ol.data(), or_.data(), block, s);
    engine->reset();
    for (int pre = 0; pre < (int) (0.25f * sampleRate); pre += block)
        engine->process (l.data(), r.data(), ol.data(), or_.data(), block, s);

    for (int pos = 0; pos < total; pos += block)
    {
        if (cancel != nullptr && cancel->load (std::memory_order_relaxed))
            return view;
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        if (pos == 0) { l[0] = 1.0f; r[0] = 1.0f; }
        engine->process (l.data(), r.data(), ol.data(), or_.data(), block, s);
        for (int i = 0; i < block; ++i)
        {
            const float a = ol[(size_t) i], b = or_[(size_t) i];
            eF += 0.5 * (a * a + b * b);
            const float la = lowL.tick (a), lb = lowR.tick (b), ha = highL.tick (a), hb = highR.tick (b);
            eL += 0.5 * (la * la + lb * lb);
            eH += 0.5 * (ha * ha + hb * hb);
            if (++inFrame == frameLen)
            {
                fFull.push_back ((float) eF);
                fLow.push_back ((float) eL);
                fHigh.push_back ((float) eH);
                peakFrame = std::max (peakFrame, (float) eF);
                if (onsetFrame < 0 && eF > 1.0e-7) onsetFrame = (int) fFull.size() - 1;
                eF = eL = eH = 0.0;
                inFrame = 0;
                // Stop once the tail has been 80 dB down for half a second.
                if (peakFrame > 0.0f && fFull.back() < peakFrame * 1.0e-8f) ++quietFrames;
                else quietFrames = 0;
            }
        }
        if (quietFrames * frameSeconds > 0.5f && pos > (int) (0.3f * sampleRate))
            break;
    }
    if (fFull.empty() || peakFrame <= 0.0f)
        return view;

    view.rt60 = rtFrom (fFull, frameSeconds);
    view.rtLow = rtFrom (fLow, frameSeconds);
    view.rtHigh = rtFrom (fHigh, frameSeconds);
    view.onset = (float) std::max (0, onsetFrame) * frameSeconds;

    // Length: the last frame within 60 dB of the peak.
    int last = 0;
    for (int i = 0; i < (int) fFull.size(); ++i)
        if (fFull[(size_t) i] > peakFrame * 1.0e-6f) last = i;
    view.length = (float) (last + 1) * frameSeconds;

    // The span shown: the whole tail to -60 dB plus a little air.
    view.seconds = clampf (view.length * 1.12f + 0.02f, 0.2f, maxSeconds);
    const int framesShown = std::max (1, (int) (view.seconds / frameSeconds));
    auto bins = [&] (const std::vector<float>& frames, std::vector<float>& out)
    {
        out.assign (ImpulseView::kBins, -120.0f);
        for (int b = 0; b < ImpulseView::kBins; ++b)
        {
            const int a = b * framesShown / ImpulseView::kBins;
            const int e = std::max (a + 1, (b + 1) * framesShown / ImpulseView::kBins);
            // The mean energy of the frames in the bin, and its neighbours a
            // little, so the curve reads as a level rather than as noise.
            double sum = 0.0;
            int count = 0;
            for (int i = std::max (0, a - 1); i < e + 1 && i < (int) frames.size(); ++i, ++count)
                sum += frames[(size_t) i] * (i < a || i >= e ? 0.5 : 1.0);
            const float mean = count > 0 ? (float) (sum / count) : 0.0f;
            out[(size_t) b] = mean > 0.0f ? 10.0f * std::log10 (mean / peakFrame) : -120.0f;
        }
        // Light smoothing in dB, keeping the onset sharp.
        std::vector<float> sm (out);
        for (int b = 2; b < ImpulseView::kBins - 1; ++b)
            sm[(size_t) b] = 0.25f * out[(size_t) b - 1] + 0.5f * out[(size_t) b] + 0.25f * out[(size_t) b + 1];
        out = sm;
    };
    bins (fFull, view.full);
    bins (fLow, view.low);
    bins (fHigh, view.high);
    view.valid = true;
    return view;
}
} // namespace nave
