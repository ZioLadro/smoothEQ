#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include "SmoothEngine.h"
#include "Params.h"

class SmoothProcessor;

namespace smooth
{
struct FactoryPreset { const char* name; void (*build) (SmoothProcessor&); };
const std::vector<FactoryPreset>& factoryPresets();

/** Factory + user presets (XML files in ~/Music/SmoothEQ Presets). */
class PresetManager
{
public:
    explicit PresetManager (SmoothProcessor& p) : proc (p) { refresh(); }

    static juce::File userDir();
    void refresh();
    const juce::StringArray& getNames() const { return names; }
    int getCurrent() const { return current; }
    void markCurrent (int index) { current = juce::jlimit (0, juce::jmax (0, names.size() - 1), index); }
    int numFactory() const { return (int) factoryPresets().size(); }
    void load (int index);
    bool save (const juce::String& name);
    bool removeCurrent();
    void next() { load ((current + 1) % juce::jmax (1, names.size())); }
    void prev() { load ((current - 1 + names.size()) % juce::jmax (1, names.size())); }
    void clearToDefaults();

private:
    SmoothProcessor& proc;
    juce::StringArray names;
    juce::Array<juce::File> userFiles;
    int current = 0;
};
} // namespace smooth

class SmoothProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    SmoothProcessor();
    ~SmoothProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SMOOTHEQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return (int) smooth::factoryPresets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, presets.getCurrent()); }
    void setCurrentProgram (int i) override { if (i >= 0 && i < getNumPrograms()) presets.load (i); }
    const juce::String getProgramName (int i) override
    {
        return i >= 0 && i < getNumPrograms() ? juce::String (smooth::factoryPresets()[(size_t) i].name) : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    std::unique_ptr<juce::XmlElement> createPresetXml();
    bool applyPresetXml (const juce::XmlElement&);

    float value (const juce::String& id) const { return state.getRawParameterValue (id)->load (std::memory_order_relaxed); }
    void setParam (const juce::String& id, float plainValue);
    /** Settings as the parameters give them now. */
    smooth::Settings readSettings() const noexcept;

    juce::AudioProcessorValueTreeState state;
    smooth::PresetManager presets;
    std::unique_ptr<smooth::Engine> engine = std::make_unique<smooth::Engine>();

    std::atomic<float> uiScale { 1.0f };
    std::atomic<int> uiBand { 2 };          // band selected in the editor, kept with the session
    std::atomic<double> uiSampleRate { 48000.0 };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void handleAsyncUpdate() override;
    void run (juce::AudioBuffer<float>&, bool bypassed);

    struct BandPtrs { std::atomic<float>* on {}, *freq {}, *gain {}, *q {}; };
    std::atomic<float>* pDepth {}, *pSharp {}, *pSelect {}, *pAttack {}, *pRelease {}, *pMode {}, *pRes {}, *pLo {}, *pHi {},
                      *pStereo {}, *pLink {}, *pMix {}, *pTrim {}, *pDelta {};
    std::array<BandPtrs, smooth::kNumBands> pBand {};
    smooth::Settings settings;
    std::atomic<int> reportedLatency { -1 };
    bool prepared = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmoothProcessor)
};
