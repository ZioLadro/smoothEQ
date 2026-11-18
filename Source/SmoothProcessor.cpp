#include "SmoothProcessor.h"
#include "SmoothEditor.h"

using namespace smooth;

juce::AudioProcessorValueTreeState::ParameterLayout SmoothProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (fparam (id::depth, "Depth", 0.0f, 24.0f, 6.0f, "dB", 8.0f, 1));
    l.add (fparam (id::sharp, "Sharpness", 0.0f, 100.0f, 50.0f, "%"));
    l.add (fparam (id::select, "Selectivity", 0.0f, 100.0f, 40.0f, "%"));
    l.add (fparam (id::attack, "Attack", 0.5f, 100.0f, 5.0f, "ms", 10.0f, 1));
    l.add (fparam (id::release, "Release", 5.0f, 500.0f, 80.0f, "ms", 80.0f, 0));
    l.add (cparam (id::mode, "Mode", modeNames(), 0));
    l.add (cparam (id::res, "Resolution", resolutionNames(), 1));
    l.add (fparam (id::lo, "Low Cut", 20.0f, 20000.0f, 20.0f, "Hz", 640.0f, 0));
    l.add (fparam (id::hi, "High Cut", 20.0f, 20000.0f, 20000.0f, "Hz", 640.0f, 0));

    const auto def = defaultBands();
    for (int b = 0; b < kNumBands; ++b)
    {
        const juce::String n = bandName (b);
        l.add (bparam (id::band (b, "on"), n + " On", true));
        l.add (fparam (id::band (b, "freq"), n + " Freq", 20.0f, 20000.0f, def[(size_t) b].freq, "Hz", 640.0f, 0));
        l.add (fparam (id::band (b, "gain"), n + " Gain", -12.0f, 12.0f, 0.0f, "dB", -1.0f, 1));
        l.add (fparam (id::band (b, "q"), n + " Q", 0.2f, 6.0f, 1.0f, "", 1.0f, 2));
    }

    l.add (cparam (id::stereo, "Stereo", stereoNames(), 0));
    l.add (fparam (id::link, "Link", 0.0f, 100.0f, 100.0f, "%"));
    l.add (fparam (id::mix, "Mix", 0.0f, 100.0f, 100.0f, "%"));
    l.add (fparam (id::trim, "Output", -12.0f, 12.0f, 0.0f, "dB", -1.0f, 1));
    l.add (bparam (id::delta, "Delta", false));
    return l;
}

SmoothProcessor::SmoothProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "SMOOTHEQ", createLayout()),
      presets (*this)
{
    auto raw = [this] (const juce::String& i) { return state.getRawParameterValue (i); };
    pDepth = raw (id::depth); pSharp = raw (id::sharp); pSelect = raw (id::select);
    pAttack = raw (id::attack); pRelease = raw (id::release); pMode = raw (id::mode); pRes = raw (id::res);
    pLo = raw (id::lo); pHi = raw (id::hi);
    pStereo = raw (id::stereo); pLink = raw (id::link); pMix = raw (id::mix); pTrim = raw (id::trim); pDelta = raw (id::delta);
    for (int b = 0; b < kNumBands; ++b)
        pBand[(size_t) b] = { raw (id::band (b, "on")), raw (id::band (b, "freq")), raw (id::band (b, "gain")), raw (id::band (b, "q")) };

    engine->prepare (48000.0, 1);
    setLatencySamples (engine->latency());
    reportedLatency = engine->latency();
}

SmoothProcessor::~SmoothProcessor() { cancelPendingUpdate(); }

bool SmoothProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void SmoothProcessor::prepareToPlay (double sampleRate, int)
{
    uiSampleRate = sampleRate;
    settings = readSettings();
    engine->prepare (sampleRate, settings.resolution);
    setLatencySamples (engine->latency());
    reportedLatency = engine->latency();
    prepared = true;
}

Settings SmoothProcessor::readSettings() const noexcept
{
    Settings s;
    auto v = [] (const std::atomic<float>* p) { return p->load (std::memory_order_relaxed); };
    s.depth = v (pDepth);
    s.sharpness = v (pSharp) * 0.01f;
    s.selectivity = v (pSelect) * 0.01f;
    s.attackMs = v (pAttack);
    s.releaseMs = v (pRelease);
    s.soft = v (pMode) < 0.5f;
    s.resolution = juce::jlimit (0, kNumResolutions - 1, (int) v (pRes));
    s.lo = v (pLo);
    s.hi = juce::jmax (s.lo, v (pHi));
    for (int b = 0; b < kNumBands; ++b)
    {
        auto& band = s.bands[(size_t) b];
        const auto& p = pBand[(size_t) b];
        band.on = v (p.on) > 0.5f;
        band.freq = v (p.freq);
        band.gain = v (p.gain);
        band.q = v (p.q);
        band.type = b == 0 ? btLowShelf : (b == kNumBands - 1 ? btHighShelf : btBell);
    }
    s.stereo = (int) v (pStereo);
    s.link = v (pLink) * 0.01f;
    s.mix = v (pMix) * 0.01f;
    s.trimDb = v (pTrim);
    s.delta = v (pDelta) > 0.5f;
    return s;
}

void SmoothProcessor::run (juce::AudioBuffer<float>& buffer, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;
    const int chans = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels());
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, buffer.getNumSamples());
    if (chans <= 0) return;
    settings = readSettings();
    engine->process (buffer.getWritePointer (0), chans > 1 ? buffer.getWritePointer (1) : nullptr, buffer.getNumSamples(), settings, bypassed);
    if (engine->latency() != reportedLatency.load (std::memory_order_relaxed))
    {
        reportedLatency = engine->latency();
        triggerAsyncUpdate();   // the host hears about the new latency from the message thread
    }
}

void SmoothProcessor::handleAsyncUpdate() { setLatencySamples (reportedLatency.load()); }

void SmoothProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { run (buffer, false); }
void SmoothProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { run (buffer, true); }

void SmoothProcessor::setParam (const juce::String& pid, float plain)
{
    if (auto* p = state.getParameter (pid))
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

std::unique_ptr<juce::XmlElement> SmoothProcessor::createPresetXml()
{
    auto copy = state.copyState();
    for (int i = copy.getNumProperties(); --i >= 0;)
    {
        const auto name = copy.getPropertyName (i);
        if (name != juce::Identifier ("version")) copy.removeProperty (name, nullptr);
    }
    copy.setProperty ("version", SMOOTHEQ_VERSION, nullptr);
    return copy.createXml();
}

bool SmoothProcessor::applyPresetXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (state.state.getType())) return false;
    auto tree = juce::ValueTree::fromXml (xml);
    // parameters missing from an older preset go back to their defaults
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            bool found = false;
            for (const auto& child : tree)
                if (child.hasProperty ("id") && child["id"].toString() == r->getParameterID()) { found = true; break; }
            if (! found) r->setValueNotifyingHost (r->getDefaultValue());
        }
    state.replaceState (tree);
    return true;
}

void SmoothProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto xml = createPresetXml();
    if (xml == nullptr) return;
    xml->setAttribute ("uiScale", (double) uiScale.load());
    xml->setAttribute ("uiBand", uiBand.load());
    xml->setAttribute ("preset", presets.getCurrent());
    copyXmlToBinary (*xml, dest);
}

void SmoothProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! applyPresetXml (*xml)) return;
    uiScale = (float) xml->getDoubleAttribute ("uiScale", 1.0);
    uiBand = juce::jlimit (0, kNumBands - 1, xml->getIntAttribute ("uiBand", 2));
    presets.markCurrent (xml->getIntAttribute ("preset", 0));
}

// ---------------------------------------------------------------- presets

namespace smooth
{
namespace
{
    struct Edit
    {
        SmoothProcessor& p;
        Edit& set (const juce::String& i, float v) { p.setParam (i, v); return *this; }
        Edit& detect (float depth, float sharp, float select, float attack, float release)
        {
            return set (id::depth, depth).set (id::sharp, sharp).set (id::select, select).set (id::attack, attack).set (id::release, release);
        }
        Edit& range (float lo, float hi) { return set (id::lo, lo).set (id::hi, hi); }
        Edit& band (int oneBased, float freq, float gain, float q = 1.0f)
        {
            const int b = oneBased - 1;
            return set (id::band (b, "freq"), freq).set (id::band (b, "gain"), gain).set (id::band (b, "q"), q);
        }
    };
}

// Starting points by source. Values chosen by ear on typical material: change Depth first, then Selectivity.
const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list = {
        { "Default", [] (SmoothProcessor&) {} },
        { "Vocal De-Harsh", [] (SmoothProcessor& p) {
            Edit { p }.detect (8.0f, 60.0f, 35.0f, 3.0f, 60.0f).range (400.0f, 12000.0f).band (4, 3200.0f, 4.0f, 0.8f).band (5, 7000.0f, 2.0f); } },
        { "Vocal Sibilance", [] (SmoothProcessor& p) {
            Edit { p }.detect (10.0f, 30.0f, 25.0f, 1.0f, 40.0f).range (4000.0f, 16000.0f).band (6, 6000.0f, 4.0f, 1.0f)
                      .set (id::res, 0); } },
        { "Vocal Boxy Room", [] (SmoothProcessor& p) {
            Edit { p }.detect (6.0f, 70.0f, 40.0f, 8.0f, 120.0f).range (150.0f, 1500.0f).band (2, 400.0f, 4.0f, 0.9f); } },
        { "Podcast Voice", [] (SmoothProcessor& p) {
            Edit { p }.detect (7.0f, 55.0f, 35.0f, 4.0f, 90.0f).range (100.0f, 10000.0f).band (3, 1100.0f, -3.0f, 0.7f); } },
        { "Acoustic Guitar Body", [] (SmoothProcessor& p) {
            Edit { p }.detect (6.0f, 65.0f, 40.0f, 6.0f, 100.0f).range (80.0f, 2000.0f).band (2, 220.0f, 3.0f, 1.0f); } },
        { "Electric Guitar Fizz", [] (SmoothProcessor& p) {
            Edit { p }.detect (9.0f, 45.0f, 30.0f, 2.0f, 60.0f).range (2000.0f, 12000.0f).band (4, 3500.0f, 3.0f, 0.9f); } },
        { "Drum Room Ring", [] (SmoothProcessor& p) {
            Edit { p }.detect (10.0f, 85.0f, 50.0f, 10.0f, 200.0f).range (100.0f, 3000.0f).set (id::res, 2); } },
        { "Cymbal Tamer", [] (SmoothProcessor& p) {
            Edit { p }.detect (8.0f, 40.0f, 35.0f, 2.0f, 50.0f).range (5000.0f, 20000.0f).band (6, 9000.0f, 3.0f, 1.0f); } },
        { "Bass Mud", [] (SmoothProcessor& p) {
            Edit { p }.detect (6.0f, 70.0f, 40.0f, 15.0f, 150.0f).range (60.0f, 500.0f).set (id::res, 2).set (id::mode, 1); } },
        { "Strings Smooth", [] (SmoothProcessor& p) {
            Edit { p }.detect (5.0f, 40.0f, 40.0f, 8.0f, 120.0f).range (1500.0f, 10000.0f); } },
        { "Synth Whistle Hunter", [] (SmoothProcessor& p) {
            Edit { p }.detect (14.0f, 90.0f, 30.0f, 1.0f, 30.0f).set (id::mode, 1); } },
        { "Mix Bus Glue", [] (SmoothProcessor& p) {
            Edit { p }.detect (3.0f, 50.0f, 50.0f, 10.0f, 150.0f).set (id::stereo, 3).set (id::link, 0.0f); } },
        { "Master Gentle", [] (SmoothProcessor& p) {
            Edit { p }.detect (2.5f, 55.0f, 55.0f, 15.0f, 200.0f).set (id::res, 2).set (id::mix, 80.0f); } },
        { "Wide Harsh Sides", [] (SmoothProcessor& p) {
            Edit { p }.detect (8.0f, 50.0f, 35.0f, 3.0f, 80.0f).range (2000.0f, 16000.0f).set (id::stereo, 2); } },
    };
    return list;
}

juce::File PresetManager::userDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("SmoothEQ Presets");
    d.createDirectory();
    return d;
}

void PresetManager::refresh()
{
    names.clear();
    for (auto& f : factoryPresets()) names.add (juce::String ("Factory / ") + f.name);
    userFiles = userDir().findChildFiles (juce::File::findFiles, false, "*.smeq");
    userFiles.sort();
    for (auto& f : userFiles) names.add (f.getFileNameWithoutExtension());
}

void PresetManager::clearToDefaults()
{
    for (auto* p : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (std::abs (r->getValue() - r->getDefaultValue()) > 1.0e-6f) r->setValueNotifyingHost (r->getDefaultValue());
}

void PresetManager::load (int index)
{
    if (index < 0 || index >= names.size()) return;
    current = index;
    if (index < numFactory())
    {
        clearToDefaults();
        factoryPresets()[(size_t) index].build (proc);
    }
    else if (auto xml = juce::XmlDocument::parse (userFiles[index - numFactory()]))
        proc.applyPresetXml (*xml);
}

bool PresetManager::save (const juce::String& name)
{
    auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto f = userDir().getChildFile (clean + ".smeq");
    auto xml = proc.createPresetXml();
    const bool ok = xml != nullptr && xml->writeTo (f);
    refresh();
    current = juce::jmax (0, names.indexOf (clean));
    return ok;
}

bool PresetManager::removeCurrent()
{
    if (current < numFactory()) return false;
    const bool ok = userFiles[current - numFactory()].deleteFile();
    refresh();
    current = 0;
    return ok;
}
} // namespace smooth

#ifndef SMOOTHEQ_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SmoothProcessor(); }
#endif
juce::AudioProcessorEditor* SmoothProcessor::createEditor() { return new SmoothEditor (*this); }
