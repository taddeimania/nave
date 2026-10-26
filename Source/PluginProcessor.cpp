#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace nave;

NaveProcessor::NaveProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NAVE", createParameterLayout())
{
    for (auto* p : getParameters())
        if (auto* wp = dynamic_cast<juce::RangedAudioParameter*> (p))
            params[wp->getParameterID()] = apvts.getRawParameterValue (wp->getParameterID());

    cacheParams();
    // The parameter defaults are the first factory preset.
    currentName = factoryPresets().front().name;
    loadedValues = snapshot();
}

bool NaveProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto monoOrStereo = [] (const juce::AudioChannelSet& s)
    {
        return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo();
    };
    return monoOrStereo (in) && monoOrStereo (out);
}

void NaveProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    silence.assign ((size_t) juce::jmax (512, samplesPerBlock), 0.0f);
    scratch.assign (silence.size(), 0.0f);
}

double NaveProcessor::getTailLengthSeconds() const
{
    const int t = juce::jlimit (0, NumTypes - 1, juce::roundToInt (fast.type->load()));
    return juce::jmin (90.0, (double) decayFor (t, fast.decay->load()) * 1.6 + fast.predelay->load() * 0.001 + 2.0);
}

// ---------------------------------------------------------------------------
void NaveProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void NaveProcessor::cacheParams()
{
    auto ptr = [this] (const char* id) { return params.at (id); };
    fast.type = ptr (ids::type);           fast.decay = ptr (ids::decay);       fast.size = ptr (ids::size);
    fast.predelay = ptr (ids::predelay);   fast.sync = ptr (ids::sync);         fast.division = ptr (ids::division);
    fast.diffusion = ptr (ids::diffusion); fast.early = ptr (ids::early);
    fast.a = ptr (ids::charA);             fast.b = ptr (ids::charB);           fast.c = ptr (ids::charC);
    fast.mode = ptr (ids::charMode);
    fast.tone = ptr (ids::tone);           fast.lowEnd = ptr (ids::lowEnd);     fast.mid = ptr (ids::mid);
    fast.lowCut = ptr (ids::lowCut);       fast.highCut = ptr (ids::highCut);
    fast.modDepth = ptr (ids::modDepth);   fast.modRate = ptr (ids::modRate);
    fast.duck = ptr (ids::duck);           fast.gate = ptr (ids::gate);         fast.release = ptr (ids::release);
    fast.hold = ptr (ids::hold);           fast.holdMode = ptr (ids::holdMode);
    fast.input = ptr (ids::input);         fast.mix = ptr (ids::mix);           fast.width = ptr (ids::width);
    fast.output = ptr (ids::output);       fast.mixLock = ptr (ids::mixLock);
}

// No lookups by name here: the audio thread calls it.
nave::EngineSettings NaveProcessor::readSettings (bool) const
{
    auto get = [] (const std::atomic<float>* p) { return p->load (std::memory_order_relaxed); };
    auto geti = [&] (const std::atomic<float>* p) { return juce::roundToInt (get (p)); };

    EngineSettings s;
    s.type = juce::jlimit (0, NumTypes - 1, geti (fast.type));
    s.decay = get (fast.decay);
    s.size = get (fast.size);
    if (get (fast.sync) > 0.5f)
    {
        const auto& d = ids::divisions();
        const int i = juce::jlimit (0, (int) d.size() - 1, geti (fast.division));
        s.predelayMs = (float) (d[(size_t) i].beats * 60000.0 / juce::jmax (20.0, hostBpm.load (std::memory_order_relaxed)));
    }
    else
        s.predelayMs = get (fast.predelay);
    s.diffusion = get (fast.diffusion);
    s.early = get (fast.early);
    s.a = get (fast.a);
    s.b = get (fast.b);
    s.c = get (fast.c);
    s.mode = geti (fast.mode);
    s.tone = get (fast.tone);
    s.lowEnd = get (fast.lowEnd);
    s.midDb = get (fast.mid);
    s.lowCutHz = get (fast.lowCut);
    s.highCutHz = get (fast.highCut);
    s.modDepth = get (fast.modDepth);
    s.modRate = get (fast.modRate);
    s.duck = get (fast.duck);
    s.gateDb = get (fast.gate);
    s.releaseMs = get (fast.release);
    s.hold = get (fast.hold) > 0.5f;
    s.holdMode = geti (fast.holdMode);
    s.inputDb = get (fast.input);
    s.mix = get (fast.mix);
    s.width = get (fast.width);
    s.outputDb = get (fast.output);
    return s;
}

nave::EngineSettings NaveProcessor::currentSettings() const { return readSettings (false); }

void NaveProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int n = buffer.getNumSamples();
    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    if (n == 0 || mainOut.getNumChannels() == 0)
        return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpmNow = pos->getBpm())
                hostBpm.store (*bpmNow, std::memory_order_relaxed);

    const auto s = readSettings (true);

    // A mono track gives one channel. The engine reads every input sample of
    // a block before writing that block's output, so in place is safe.
    const float* inL = mainIn.getNumChannels() > 0 ? mainIn.getReadPointer (0) : silence.data();
    const float* inR = mainIn.getNumChannels() > 1 ? mainIn.getReadPointer (1) : inL;
    float* outL = mainOut.getWritePointer (0);
    float* outR = mainOut.getNumChannels() > 1 ? mainOut.getWritePointer (1) : nullptr;
    if (outR == nullptr)
    {
        if (scratch.size() < (size_t) n) return;
        engine.process (inL, inR, outL, scratch.data(), n, s);
        for (int i = 0; i < n; ++i) outL[i] = 0.5f * (outL[i] + scratch[(size_t) i]);
    }
    else
        engine.process (inL, inR, outL, outR, n, s);

    for (int ch = mainOut.getNumChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // To the panel.
    auto raise = [] (std::atomic<float>& a, float v)
    {
        float cur = a.load();
        while (v > cur && ! a.compare_exchange_weak (cur, v)) {}
    };
    raise (uiIn, engine.takeInputPeak());
    raise (uiOut, engine.takeOutputPeak());
    raise (uiWet, engine.takeWetPeak());
    uiDuck.store (engine.duckReductionDb(), std::memory_order_relaxed);
    uiGate.store (engine.gateOpen(), std::memory_order_relaxed);
    uiSwell.store (engine.swellLevel(), std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
void NaveProcessor::selectType (int type)
{
    type = juce::jlimit (0, NumTypes - 1, type);
    const auto& st = typeInfo (type).start;
    setParam (ids::type, (float) type);
    setParam (ids::decay, st.decay);
    setParam (ids::size, st.size);
    setParam (ids::diffusion, st.diffusion);
    setParam (ids::early, st.early);
    setParam (ids::charA, st.a);
    setParam (ids::charB, st.b);
    setParam (ids::charC, st.c);
    setParam (ids::charMode, (float) st.mode);
}

void NaveProcessor::stepType (int delta)
{
    selectType ((geti (ids::type) + delta + NumTypes) % NumTypes);
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------
int NaveProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

const juce::String NaveProcessor::getProgramName (int index)
{
    const auto& f = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) f.size()) ? juce::String (f[(size_t) index].name) : juce::String();
}

juce::String NaveProcessor::presetBlurb() const
{
    const auto& f = factoryPresets();
    if (userFile == juce::File() && juce::isPositiveAndBelow (factoryIndex, (int) f.size()))
        return f[(size_t) factoryIndex].blurb;
    return {};
}

std::vector<float> NaveProcessor::snapshot() const
{
    std::vector<float> v;
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto& id = rp->getParameterID();
            if (id != ids::hold && id != ids::mixLock)
                v.push_back (rp->getValue());
        }
    return v;
}

bool NaveProcessor::presetEdited() const
{
    const auto now = snapshot();
    if (now.size() != loadedValues.size())
        return true;
    for (size_t i = 0; i < now.size(); ++i)
        if (std::abs (now[i] - loadedValues[i]) > 0.002f)
            return true;
    return false;
}

void NaveProcessor::applyValues (const std::vector<std::pair<juce::String, float>>& plain, bool keepMix)
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto& id = rp->getParameterID();
            if (id == ids::mixLock || id == ids::hold) continue;
            if (keepMix && id == ids::mix) continue;
            float v = rp->getDefaultValue();
            for (const auto& [pid, value] : plain)
                if (id == pid)
                    v = rp->convertTo0to1 (value);
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (v);
            rp->endChangeGesture();
        }
}

void NaveProcessor::loadFactory (int index)
{
    const auto& list = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return;
    const auto& preset = list[(size_t) index];
    std::vector<std::pair<juce::String, float>> values;
    for (const auto& [id, plain] : preset.values)
        values.push_back ({ id, plain });
    applyValues (values, getParam (ids::mixLock) > 0.5f);
    factoryIndex = index;
    userFile = juce::File();
    currentName = preset.name;
    loadedValues = snapshot();
}

std::unique_ptr<juce::XmlElement> NaveProcessor::stateXml()
{
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr)
        return nullptr;
    xml->setAttribute ("presetName", currentName);
    xml->setAttribute ("presetFactory", userFile == juce::File() ? factoryIndex : -1);
    xml->setAttribute ("presetFile", userFile.getFullPathName());
    return xml;
}

void NaveProcessor::applyState (const juce::XmlElement& xml)
{
    if (xml.hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (xml));
}

bool NaveProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    // Read the preset's values and apply them like a factory preset, so KEEP
    // MIX and HOLD are left alone.
    std::vector<std::pair<juce::String, float>> values;
    for (auto* child : xml->getChildWithTagNameIterator ("PARAM"))
        values.push_back ({ child->getStringAttribute ("id"), (float) child->getDoubleAttribute ("value") });
    applyValues (values, getParam (ids::mixLock) > 0.5f);

    userFile = file;
    factoryIndex = -1;
    currentName = file.getFileNameWithoutExtension();
    loadedValues = snapshot();
    return true;
}

bool NaveProcessor::saveUserPreset (const juce::String& rawName, juce::String& error)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
    {
        error = "Give the preset a name.";
        return false;
    }
    const auto folder = userPresetFolder();
    if (! folder.createDirectory())
    {
        error = "Couldn't create " + folder.getFullPathName();
        return false;
    }
    const auto file = folder.getChildFile (name + kPresetExtension);
    currentName = name;
    userFile = file;
    factoryIndex = -1;
    auto xml = stateXml();
    if (xml == nullptr || ! xml->writeTo (file))
    {
        error = "Couldn't write " + file.getFullPathName();
        return false;
    }
    loadedValues = snapshot();
    return true;
}

void NaveProcessor::stepPreset (int delta)
{
    const int numFactory = (int) factoryPresets().size();
    const auto users = userPresets();
    const int total = numFactory + users.size();
    int cur = presetNumber() - 1;
    if (cur < 0) cur = 0;
    const int next = (cur + delta + total) % total;
    if (next < numFactory)
        loadFactory (next);
    else
        loadUserPreset (users[next - numFactory]);
}

int NaveProcessor::presetNumber() const
{
    if (userFile == juce::File())
        return factoryIndex >= 0 ? factoryIndex + 1 : 0;
    const auto users = userPresets();
    const int i = users.indexOf (userFile);
    return i >= 0 ? (int) factoryPresets().size() + i + 1 : 0;
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
void NaveProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = stateXml())
        copyXmlToBinary (*xml, destData);
}

void NaveProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    applyState (*xml);
    currentName = xml->getStringAttribute ("presetName", currentName);
    factoryIndex = xml->getIntAttribute ("presetFactory", -1);
    userFile = juce::File (xml->getStringAttribute ("presetFile"));
    if (userFile != juce::File() && ! userFile.existsAsFile())
        userFile = juce::File();
    loadedValues = snapshot();
}

juce::AudioProcessorEditor* NaveProcessor::createEditor()
{
    return new NaveEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NaveProcessor();
}
