// EngineTests.cpp — checks Nave's signal path without JUCE or a host.
// Run with ctest, or directly: ./NaveEngineTests [-v]
#include "Engine/Probe.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace nave;

namespace
{
int failures = 0, checks = 0;
bool verbose = false;

void check (bool ok, const std::string& what)
{
    ++checks;
    if (! ok)
    {
        ++failures;
        std::printf ("FAIL: %s\n", what.c_str());
    }
    else if (verbose)
        std::printf ("ok:   %s\n", what.c_str());
}

std::string fmt (const char* f, double a, double b = 0.0)
{
    char buf[160];
    std::snprintf (buf, sizeof buf, f, a, b);
    return buf;
}

EngineSettings startFor (int t)
{
    EngineSettings s;
    const auto& st = typeInfo (t).start;
    s.type = t;
    s.decay = st.decay; s.size = st.size; s.diffusion = st.diffusion; s.early = st.early;
    s.a = st.a; s.b = st.b; s.c = st.c; s.mode = st.mode;
    s.mix = 1.0f;
    s.predelayMs = 0.0f;
    return s;
}

struct Result { double inEnergy = 0, outEnergy = 0; float peak = 0; bool finite = true; };

// Pink noise (Paul Kellet's filter): closer to music's balance than white.
struct Pink
{
    Rng rng;
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    explicit Pink (uint32_t seed) : rng (seed) {}
    float next()
    {
        const float w = rng.bi();
        b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
        b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
        b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
        const float out = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f) * 0.11f;
        b6 = w * 0.115926f;
        return out;
    }
};

// Runs stereo pink noise (or silence) through the engine, `seconds` long, in `block`-sized calls.
Result run (Engine& e, const EngineSettings& s, float sr, float seconds, float noiseLevel, int block = 256,
            float measureFrom = 0.0f, uint32_t seed = 1)
{
    Result res;
    Pink pl (seed), pr (seed + 101);
    std::vector<float> l ((size_t) block), r ((size_t) block), ol ((size_t) block), orr ((size_t) block);
    const int total = (int) (seconds * sr);
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        for (int i = 0; i < n; ++i) { l[(size_t) i] = pl.next() * noiseLevel * 3.5f; r[(size_t) i] = pr.next() * noiseLevel * 3.5f; }
        e.process (l.data(), r.data(), ol.data(), orr.data(), n, s);
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (ol[(size_t) i]) || ! std::isfinite (orr[(size_t) i])) res.finite = false;
            res.peak = std::max (res.peak, std::max (std::fabs (ol[(size_t) i]), std::fabs (orr[(size_t) i])));
            if ((float) (pos + i) >= measureFrom * sr)
            {
                res.inEnergy += (double) l[(size_t) i] * l[(size_t) i] + (double) r[(size_t) i] * r[(size_t) i];
                res.outEnergy += (double) ol[(size_t) i] * ol[(size_t) i] + (double) orr[(size_t) i] * orr[(size_t) i];
            }
        }
    }
    return res;
}

double db (double ratio) { return 10.0 * std::log10 (std::max (1.0e-30, ratio)); }

// ---------------------------------------------------------------------------
void testEveryType()
{
    const float sr = 48000.0f;
    for (int t = 0; t < NumTypes; ++t)
    {
        Engine e;
        e.prepare (sr, 256);
        const auto r = run (e, startFor (t), sr, 4.0f, 0.17f, 256, 2.0f);
        const std::string name = typeInfo (t).name;
        check (r.finite, name + ": finite");
        check (r.peak < 2.0f, name + fmt (": peak %.2f under 2", r.peak));
        const double lvl = db (r.outEnergy / r.inEnergy);
        check (lvl > -9.0 && lvl < -2.0, name + fmt (": level %.1f dB near -5", lvl));
    }
}

void testDryPassthrough()
{
    const float sr = 48000.0f;
    for (int t : { (int) TConcertHall, (int) TReflections, (int) TSwell, (int) TTape })
    {
        Engine e;
        e.prepare (sr, 256);
        auto s = startFor (t);
        s.mix = 0.0f;
        Rng rng (5);
        std::vector<float> l (300), r (300), ol (300), orr (300);
        double err = 0.0;
        for (int b = 0; b < 40; ++b)
        {
            for (int i = 0; i < 300; ++i) { l[(size_t) i] = rng.bi() * 0.3f; r[(size_t) i] = rng.bi() * 0.3f; }
            e.process (l.data(), r.data(), ol.data(), orr.data(), 300, s);
            if (t == TReflections || t == TSwell) continue;   // they move the dry sound on purpose
            for (int i = 0; i < 300; ++i)
                err = std::max (err, (double) std::max (std::fabs (ol[(size_t) i] - l[(size_t) i]), std::fabs (orr[(size_t) i] - r[(size_t) i])));
        }
        if (t != TReflections && t != TSwell)
            check (err < 1.0e-6, std::string (typeInfo (t).name) + fmt (": MIX 0 is the dry signal (err %.2g)", err));
    }
}

void testPredelay()
{
    const float sr = 48000.0f;
    for (int t : { (int) TPlate, (int) TConcertHall, (int) TSpring })
    {
        Engine e;
        e.prepare (sr, 256);
        auto s = startFor (t);
        s.predelayMs = 250.0f;
        std::vector<float> l (256, 0.0f), r (256, 0.0f), ol (256), orr (256);
        int first = -1;
        for (int pos = 0; pos < (int) (0.6f * sr); pos += 256)
        {
            std::fill (l.begin(), l.end(), 0.0f);
            std::fill (r.begin(), r.end(), 0.0f);
            if (pos == 0) l[0] = r[0] = 1.0f;
            e.process (l.data(), r.data(), ol.data(), orr.data(), 256, s);
            for (int i = 0; i < 256 && first < 0; ++i)
                if (std::fabs (ol[(size_t) i]) > 1.0e-6f || std::fabs (orr[(size_t) i]) > 1.0e-6f) first = pos + i;
        }
        check (first >= (int) (0.249f * sr), std::string (typeInfo (t).name) + fmt (": nothing before the 250 ms pre-delay (first at %.1f ms)", first * 1000.0 / sr));
    }
}

void testDecayTracksKnob()
{
    const float sr = 48000.0f;
    struct Case { int type; float decay; float tol; };
    const Case cases[] { { TConcertHall, 1.5f, 0.25f }, { TConcertHall, 3.0f, 0.2f }, { TConcertHall, 6.0f, 0.2f },
                         { TPlate, 1.0f, 0.25f }, { TPlate, 4.0f, 0.2f }, { TSpring, 2.0f, 0.3f }, { TCloud, 12.0f, 0.25f },
                         { TStudio, 0.8f, 0.3f } };
    for (auto c : cases)
    {
        auto s = startFor (c.type);
        s.decay = c.decay;
        const auto v = renderImpulse (s, sr, 30.0f);
        check (v.valid && std::abs (v.rt60 - c.decay) < c.tol * c.decay,
               std::string (typeInfo (c.type).name) + fmt (": DECAY %.1f s measures %.2f s", c.decay, v.rt60));
    }
}

void testHold()
{
    const float sr = 48000.0f;
    for (int t : { (int) TConcertHall, (int) TPlate, (int) TCloud, (int) TShimmer, (int) TSpring })
    {
        Engine e;
        e.prepare (sr, 256);
        auto s = startFor (t);
        s.decay = 1.0f;
        run (e, s, sr, 1.0f, 0.2f);
        s.hold = true;
        s.holdMode = 1;   // FREEZE: no more input
        const auto early = run (e, s, sr, 1.0f, 0.2f, 256, 0.5f);
        const auto late = run (e, s, sr, 4.0f, 0.2f, 256, 3.5f);
        const double drop = db (late.outEnergy / 0.5 / (early.outEnergy / 0.5));
        check (late.finite && drop > -9.0 && drop < 3.0,
               std::string (typeInfo (t).name) + fmt (": FREEZE holds the tail (%.1f dB over 3 s)", drop));

        // INFINITE with sound still coming in stays bounded.
        s.holdMode = 0;
        const auto inf = run (e, s, sr, 8.0f, 0.4f);
        check (inf.finite && inf.peak < 6.0f, std::string (typeInfo (t).name) + fmt (": INFINITE stays bounded (peak %.2f)", inf.peak));
    }
}

void testExtremes()
{
    const float sr = 48000.0f;
    for (int t = 0; t < NumTypes; ++t)
    {
        Engine e;
        e.prepare (sr, 512);
        auto s = startFor (t);
        s.decay = 60.0f; s.size = 1.0f; s.diffusion = 1.0f; s.early = 1.0f;
        s.a = s.b = s.c = 1.0f;
        s.mode = (int) typeInfo (t).modeNames.size() - 1;
        s.tone = 1.0f; s.lowEnd = 10.0f; s.midDb = 12.0f; s.modDepth = 1.0f; s.modRate = 8.0f;
        s.inputDb = 12.0f;
        const auto r = run (e, s, sr, 10.0f, 0.5f, 512);
        check (r.finite && r.peak < 40.0f, std::string (typeInfo (t).name) + fmt (": everything at maximum stays sane (peak %.1f)", r.peak));

        auto z = startFor (t);
        z.decay = 0.0f; z.size = 0.0f; z.diffusion = 0.0f; z.early = 0.0f; z.a = z.b = z.c = 0.0f;
        z.tone = 0.0f; z.lowEnd = -10.0f; z.modDepth = 0.0f; z.modRate = 0.02f;
        Engine e2;
        e2.prepare (sr, 64);
        const auto r2 = run (e2, z, sr, 2.0f, 0.5f, 64);
        check (r2.finite && r2.peak < 4.0f, std::string (typeInfo (t).name) + ": everything at minimum stays sane");
    }
}

void testTypeSwitching()
{
    const float sr = 48000.0f;
    Engine e;
    e.prepare (sr, 480);
    Rng rng (8);
    std::vector<float> l (480), r (480), ol (480), orr (480);
    bool finite = true;
    float peak = 0.0f;
    for (int step = 0; step < 300; ++step)
    {
        auto s = startFor ((step * 7) % NumTypes);
        for (int i = 0; i < 480; ++i) { l[(size_t) i] = rng.bi() * 0.3f; r[(size_t) i] = rng.bi() * 0.3f; }
        e.process (l.data(), r.data(), ol.data(), orr.data(), 480, s);
        for (int i = 0; i < 480; ++i)
        {
            finite = finite && std::isfinite (ol[(size_t) i]) && std::isfinite (orr[(size_t) i]);
            peak = std::max (peak, std::fabs (ol[(size_t) i]));
        }
    }
    check (finite && peak < 3.0f, fmt ("switching type every 10 ms stays clean (peak %.2f)", peak));
}

void testDuckAndGate()
{
    const float sr = 48000.0f;
    {
        Engine a, b;
        a.prepare (sr, 256);
        b.prepare (sr, 256);
        auto s = startFor (TConcertHall);
        const auto plain = run (a, s, sr, 3.0f, 0.3f, 256, 1.5f);
        s.duck = 1.0f;
        const auto ducked = run (b, s, sr, 3.0f, 0.3f, 256, 1.5f);
        const double d = db (ducked.outEnergy / plain.outEnergy);
        check (d < -12.0, fmt ("DUCK at full pulls the tail down under loud playing (%.1f dB)", d));
    }
    {
        // After the input stops, a gated tail is gone; an ungated one isn't.
        for (int gated = 0; gated < 2; ++gated)
        {
            Engine e;
            e.prepare (sr, 256);
            auto s = startFor (TArena);
            s.gateDb = gated ? -40.0f : -80.0f;
            s.releaseMs = 100.0f;
            run (e, s, sr, 1.0f, 0.3f);
            const auto tail = run (e, s, sr, 1.0f, 0.0f, 256, 0.5f);
            const double lvl = db (tail.outEnergy / (0.5 * sr * 2 * 0.03));
            if (gated) check (lvl < -60.0, fmt ("GATE closes the tail when the input stops (%.1f dB)", lvl));
            else       check (lvl > -40.0, fmt ("without GATE the tail rings on (%.1f dB)", lvl));
        }
    }
}

void testSwell()
{
    const float sr = 48000.0f;
    Engine e;
    e.prepare (sr, 256);
    auto s = startFor (TSwell);
    s.a = 0.6f;   // rise about 300 ms
    s.mode = 0;
    run (e, s, sr, 0.5f, 0.0f);
    const auto start = run (e, s, sr, 0.06f, 0.3f);
    const auto later = run (e, s, sr, 1.5f, 0.3f, 256, 1.2f);
    const double a = start.outEnergy / 0.06, b = later.outEnergy / 0.3;
    check (db (a / b) < -10.0, fmt ("SWELL starts quiet and rises (%.1f dB at the start)", db (a / b)));
}

void testBlockSizes()
{
    const float sr = 48000.0f;
    auto s = startFor (TPlate);
    std::vector<float> ref;
    for (int block : { 512, 37, 1 })
    {
        Engine e;
        e.prepare (sr, 512);
        std::vector<float> out;
        Rng rng (2);
        std::vector<float> l ((size_t) block), r ((size_t) block), ol ((size_t) block), orr ((size_t) block);
        const int total = (int) (0.5f * sr);
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = std::min (block, total - pos);
            for (int i = 0; i < n; ++i) { l[(size_t) i] = rng.bi() * 0.2f; r[(size_t) i] = rng.bi() * 0.2f; }
            e.process (l.data(), r.data(), ol.data(), orr.data(), n, s);
            out.insert (out.end(), ol.begin(), ol.begin() + n);
        }
        if (ref.empty()) { ref = out; continue; }
        double diff = 0, sig = 0;
        for (size_t i = 0; i < ref.size(); ++i) { diff += (ref[i] - out[i]) * (ref[i] - out[i]); sig += ref[i] * ref[i]; }
        check (db (diff / sig) < -40.0, fmt ("block size %d gives the same sound (difference %.1f dB)", block, db (diff / sig)));
    }
}

void testSampleRates()
{
    auto s = startFor (TConcertHall);
    const auto a = renderImpulse (s, 44100.0f, 20.0f);
    const auto b = renderImpulse (s, 96000.0f, 20.0f);
    check (std::abs (a.rt60 - b.rt60) < 0.15f * a.rt60, fmt ("Concert Hall decays alike at 44.1 and 96 kHz (%.2f vs %.2f s)", a.rt60, b.rt60));
    for (int t : { (int) TSpring, (int) TPlate, (int) TReflections })
    {
        Engine e;
        e.prepare (96000.0, 512);
        const auto r = run (e, startFor (t), 96000.0f, 2.0f, 0.2f, 512, 1.0f);
        const double lvl = db (r.outEnergy / r.inEnergy);
        check (r.finite && lvl > -10.0 && lvl < 0.0, std::string (typeInfo (t).name) + fmt (" at 96 kHz: level %.1f dB", lvl));
    }
}

void testSearch()
{
    auto best = [] (const std::string& q)
    {
        int bestT = -1, bestS = 0;
        for (int t = 0; t < NumTypes; ++t)
        {
            const auto& i = typeInfo (t);
            const int sc = matchScore (q, i.name, i.category, i.blurb, i.keywords);
            if (sc > bestS) { bestS = sc; bestT = t; }
        }
        return bestT;
    };
    check (best ("lofi") == TLofi, "search: 'lofi' finds Lo-Fi");
    check (best ("gate") == TGated, "search: 'gate' finds Gated");
    check (best ("church") == TCathedral, "search: 'church' finds Cathedral");
    check (best ("80s drums") == TGated, "search: '80s drums' finds Gated");
    check (best ("surf") == TSurfSpring, "search: 'surf' finds Surf Spring");
    check (best ("octave down") == TAbyss, "search: 'octave down' finds Abyss");
    check (best ("concert") == TConcertHall, "search: 'concert' finds Concert Hall");
    check (best ("xyzzy") == -1, "search: nonsense finds nothing");
    int halls = 0;
    for (int t = 0; t < NumTypes; ++t)
        halls += matchScore ("hall", typeInfo (t).name, typeInfo (t).category, typeInfo (t).blurb, typeInfo (t).keywords) > 0;
    check (halls >= 4, fmt ("search: 'hall' finds the halls (%d)", halls));
}

void testCatalogue()
{
    check ((int) types().size() == NumTypes, "the catalogue has every type");
    for (int t = 0; t < NumTypes; ++t)
    {
        const auto& i = typeInfo (t);
        const auto& st = i.start;
        check (st.decay >= i.voicing.decayMin && st.decay <= i.voicing.decayMax, std::string (i.name) + ": starting decay in range");
        check (i.modeNames.empty() || st.mode < (int) i.modeNames.size(), std::string (i.name) + ": starting choice exists");
        check ((int) i.modeNames.size() <= kMaxModes, std::string (i.name) + ": choices fit");
    }
    check (charText (TTape, 0, 0.0f) == "60 ms", "tape TIME reads in ms");
    check (charText (TReflections, 0, 0.5f) == "CENTRE", "X reads CENTRE in the middle");
    check (std::string (shiftName (1.0f, true)) == "+2 OCT" && std::string (shiftName (0.0f, true)) == "OFF", "shimmer shifts read");
}

void testProbe()
{
    auto s = startFor (TGated);
    const auto v = renderImpulse (s, 32000.0f, 10.0f);
    check (v.valid && v.length > 0.2f && v.length < 0.6f, fmt ("probe: the gated tail is short (%.2f s)", v.length));
    auto h = startFor (TArena);
    const auto w = renderImpulse (h, 32000.0f, 30.0f);
    check (w.valid && (int) w.full.size() == ImpulseView::kBins && w.seconds > 4.0f, fmt ("probe: the arena tail is long (%.1f s shown)", w.seconds));
    check (w.rtHigh < w.rtLow, "probe: highs die before lows in the arena");
}
} // namespace

int main (int argc, char** argv)
{
    verbose = argc > 1 && std::strcmp (argv[1], "-v") == 0;
    testCatalogue();
    testSearch();
    testEveryType();
    testDryPassthrough();
    testPredelay();
    testDecayTracksKnob();
    testHold();
    testExtremes();
    testTypeSwitching();
    testDuckAndGate();
    testSwell();
    testBlockSizes();
    testSampleRates();
    testProbe();
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
