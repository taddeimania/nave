// Snapshot.cpp — dev tool: run Nave without a host, render its panel to a
// PNG and, optionally, every factory preset to WAV files.
//   NaveSnapshot out.png [wavFolder|-] [input.wav|-] [preset index] [browser query]
// Without an input file it plays a short built-in phrase: drum hits, a
// plucked chord and a held note, so every display has something to show.
// With a browser query, the panel is captured with the browser open on it.
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Presets.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
using namespace nave;

// Six seconds: kick and snare, a plucked chord, then a held note into silence.
juce::AudioBuffer<float> testPhrase (double rate)
{
    const int n = (int) (rate * 6.0);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    Rng rng (7);
    auto add = [&] (int at, int len, auto&& fn)
    {
        for (int i = 0; i < len && at + i < n; ++i)
        {
            const float v = fn (i, (float) i / (float) rate);
            b.addSample (0, at + i, v);
            b.addSample (1, at + i, v);
        }
    };
    for (int hit = 0; hit < 4; ++hit)
    {
        const int at = (int) (rate * (0.1 + 0.5 * hit));
        if (hit % 2 == 0)   // kick
            add (at, (int) (0.3 * rate), [&] (int, float t) { return 0.7f * std::exp (-t * 14.0f) * std::sin (6.2832f * (45.0f + 90.0f * std::exp (-t * 30.0f)) * t); });
        else                // snare
            add (at, (int) (0.25 * rate), [&] (int, float t) { return std::exp (-t * 22.0f) * (0.45f * rng.bi() + 0.3f * std::sin (6.2832f * 190.0f * t)); });
    }
    const float chord[] { 220.0f, 277.2f, 329.6f, 440.0f };
    for (int k = 0; k < 4; ++k)
        add ((int) (rate * (2.2 + 0.03 * k)), (int) (1.2 * rate), [&] (int, float t)
        {
            const float ph = std::fmod (t * chord[k], 1.0f);
            return 0.18f * std::exp (-t * 3.5f) * (2.0f * ph - 1.0f);
        });
    add ((int) (rate * 3.6), (int) (1.2 * rate), [&] (int, float t)
    {
        const float env = std::min (1.0f, t / 0.02f) * (t > 1.0f ? std::max (0.0f, 1.0f - (t - 1.0f) / 0.2f) : 1.0f);
        return 0.25f * env * std::sin (6.2832f * 523.25f * t + 0.6f * std::sin (6.2832f * 5.0f * t));
    });
    return b;
}

void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double rate)
{
    f.deleteFile();
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (f), rate,
                                                                     (unsigned int) b.getNumChannels(), 24, {}, 0));
    if (w)
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

juce::AudioBuffer<float> render (NaveProcessor& proc, const juce::AudioBuffer<float>& in, int block, int start = 0, int length = -1)
{
    const int n = length < 0 ? in.getNumSamples() - start : length;
    juce::AudioBuffer<float> out (2, n);
    juce::AudioBuffer<float> io (2, block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < n; pos += block)
    {
        const int len = juce::jmin (block, n - pos);
        io.setSize (2, len, false, false, true);
        io.clear();
        if (start + pos < in.getNumSamples())
        {
            const int avail = juce::jmin (len, in.getNumSamples() - (start + pos));
            io.copyFrom (0, 0, in, 0, start + pos, avail);
            io.copyFrom (1, 0, in, 1, start + pos, avail);
        }
        proc.processBlock (io, midi);
        out.copyFrom (0, pos, io, 0, 0, len);
        out.copyFrom (1, pos, io, 1, 0, len);
    }
    return out;
}

bool given (int argc, char** argv, int i) { return argc > i && juce::String (argv[i]).isNotEmpty() && juce::String (argv[i]) != "-"; }
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
    {
        std::printf ("usage: NaveSnapshot out.png [wavFolder|-] [input.wav|-] [preset index] [browser query]\n");
        return 1;
    }
    const double rate = 48000.0;
    const int block = 256;

    NaveProcessor proc;
    proc.setRateAndBufferSizeDetails (rate, block);
    proc.prepareToPlay (rate, block);

    auto phrase = testPhrase (rate);
    if (given (argc, argv, 3))
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File::getCurrentWorkingDirectory().getChildFile (argv[3])));
        if (r)
        {
            const int len = (int) juce::jmin<juce::int64> (r->lengthInSamples, (juce::int64) (rate * 12.0));
            juce::AudioBuffer<float> v (2, len);
            r->read (&v, 0, len, 0, true, true);
            phrase = v;
        }
    }

    if (given (argc, argv, 2))
    {
        const juce::File dir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        dir.createDirectory();
        juce::AudioBuffer<float> padded (2, phrase.getNumSamples() + (int) (rate * 4.0));
        padded.clear();
        padded.copyFrom (0, 0, phrase, 0, 0, phrase.getNumSamples());
        padded.copyFrom (1, 0, phrase, 1, 0, phrase.getNumSamples());
        writeWav (dir.getChildFile ("00 dry.wav"), padded, rate);
        const auto& presets = factoryPresets();
        for (int p = 0; p < (int) presets.size(); ++p)
        {
            proc.loadFactory (p);
            proc.prepareToPlay (rate, block);
            const auto out = render (proc, padded, block);
            const float peak = out.getMagnitude (0, out.getNumSamples());
            std::printf ("%02d %-24s peak %.3f\n", p + 1, presets[(size_t) p].name, peak);
            writeWav (dir.getChildFile (juce::String (p + 1).paddedLeft ('0', 2) + " " + presets[(size_t) p].name + ".wav"), out, rate);
        }
    }

    proc.loadFactory (argc > 4 ? juce::String (argv[4]).getIntValue() : 0);
    proc.prepareToPlay (rate, block);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (NavePanel::designW, NavePanel::designH);

    // Play the phrase in real-ish time so the meters move and the tail is measured.
    const int stop = (int) (rate * 3.0);
    for (int pos = 0; pos < stop; pos += block * 8)
    {
        render (proc, phrase, block, pos, juce::jmin (block * 8, stop - pos));
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    }
    if (argc > 5)
    {
        if (auto* panel = dynamic_cast<NavePanel*> (ed->getChildComponent (0)))
        {
            panel->openBrowser (juce::String (argv[5]).startsWith ("@"));
            juce::String q (argv[5]);
            if (q.startsWith ("@")) q = q.substring (1);
            // Type the query into the search box.
            if (auto* search = dynamic_cast<juce::TextEditor*> (panel->getChildComponent (panel->getNumChildComponents() - 1)->getChildComponent (0)))
                search->setText (q, true);
        }
    }
    // Let the tail probe finish and the display settle, still playing so the
    // meters show something.
    for (int i = 0; i < 60; ++i)
    {
        const int at = (int) (rate * 2.1) + (i % 12) * block * 8;
        render (proc, phrase, block, at, block * 8);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    }

    const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat().writeImageToStream (img, stream);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    ed.reset();
    return 0;
}
