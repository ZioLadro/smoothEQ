// SMOOTHEQ headless tests.
//   SmoothEQTests                                  -> run every test
//   SmoothEQTests --snap file [band] [preset] [scale] -> render the editor to a PNG, after some test audio
#include "SmoothProcessor.h"
#include "SmoothEditor.h"
#include <chrono>
#include <cstdio>
#include <random>

using namespace smooth;

static int passes = 0, failures = 0;
#define CHECK(cond, what) do { if (cond) ++passes; else { ++failures; std::printf ("  FAIL: %s  (%s:%d)\n", what, __FILE__, __LINE__); } } while (0)

struct Stereo { std::vector<float> l, r; };

static Stereo noise (int n, float level, unsigned seed)
{
    Stereo s { std::vector<float> ((size_t) n), std::vector<float> ((size_t) n) };
    std::mt19937 g (seed); std::normal_distribution<float> d;
    for (int i = 0; i < n; ++i) { s.l[(size_t) i] = level * d (g); s.r[(size_t) i] = level * d (g); }
    return s;
}

static void addSine (Stereo& s, float freq, float amp, double sr, bool left = true, bool right = true, float sign = 1.0f)
{
    for (size_t i = 0; i < s.l.size(); ++i)
    {
        const float v = amp * (float) std::sin (2.0 * 3.14159265358979 * freq * (double) i / sr);
        if (left) s.l[i] += v;
        if (right) s.r[i] += sign * v;
    }
}

/** Amplitude of one frequency over [from, to) (Goertzel). */
static float toneLevel (const std::vector<float>& x, float freq, double sr, int from, int to)
{
    const double w = 2.0 * 3.14159265358979 * freq / sr, k = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (int i = from; i < to; ++i) { const double s0 = x[(size_t) i] + k * s1 - s2; s2 = s1; s1 = s0; }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return (float) (2.0 * std::sqrt (re * re + im * im) / (to - from));
}

static float rms (const std::vector<float>& x, int from, int to)
{
    double s = 0; for (int i = from; i < to; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
    return (float) std::sqrt (s / std::max (1, to - from));
}

/** A processor with its parameters, run in blocks like a host would. */
struct Rig
{
    SmoothProcessor p;
    double sr;
    int block;
    explicit Rig (double rate = 48000.0, int blockSize = 512, int channels = 2) : sr (rate), block (blockSize)
    {
        const auto set = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        p.setPlayConfigDetails (channels, channels, sr, block);
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (set); layout.outputBuses.add (set);
        p.setBusesLayout (layout);
        p.prepareToPlay (sr, block);
    }
    void set (const juce::String& pid, float v) { p.setParam (pid, v); }
    Stereo run (Stereo in, bool bypassed = false, std::function<int()> nextBlock = {})
    {
        const int chans = p.getTotalNumOutputChannels();
        juce::MidiBuffer midi;
        for (size_t at = 0; at < in.l.size();)
        {
            const int n = (int) std::min<size_t> ((size_t) (nextBlock ? nextBlock() : block), in.l.size() - at);
            juce::AudioBuffer<float> buf (chans, n);
            buf.copyFrom (0, 0, in.l.data() + at, n);
            if (chans > 1) buf.copyFrom (1, 0, in.r.data() + at, n);
            if (bypassed) p.processBlockBypassed (buf, midi); else p.processBlock (buf, midi);
            std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + n, in.l.data() + at);
            if (chans > 1) std::copy (buf.getReadPointer (1), buf.getReadPointer (1) + n, in.r.data() + at);
            at += (size_t) n;
        }
        return in;
    }
};

static float maxDelayedError (const Stereo& out, const Stereo& in, int latency, int skip, bool stereo = true)
{
    float e = 0.0f;
    for (size_t i = (size_t) (latency + skip); i < out.l.size(); ++i)
    {
        e = std::max (e, std::abs (out.l[i] - in.l[i - (size_t) latency]));
        if (stereo) e = std::max (e, std::abs (out.r[i] - in.r[i - (size_t) latency]));
    }
    return e;
}

// ---------------------------------------------------------------- tests

static void testNull()
{
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (int res = 0; res < kNumResolutions; ++res)
            for (int ch : { 1, 2 })
            {
                Rig r (sr, 480, ch);
                r.set (id::depth, 0.0f);
                r.set (id::res, (float) res);
                r.p.prepareToPlay (sr, 480);
                const auto in = noise ((int) sr, 0.3f, 7);
                const auto out = r.run (in);
                const int lat = r.p.getLatencySamples();
                CHECK (lat == fftSizeFor (res, sr), "latency is the FFT size");
                CHECK (maxDelayedError (out, in, lat, lat, ch == 2) < 1.0e-5f, "depth 0: the output is the input, delayed");
            }
}

static void testResonance()
{
    const double sr = 48000.0;
    Rig r (sr);
    r.set (id::depth, 12.0f);
    auto in = noise (4 * 48000, 0.05f, 3);
    addSine (in, 2000.0f, 0.3f, sr);
    const auto out = r.run (in);
    const int lat = r.p.getLatencySamples(), from = 48000, to = 4 * 48000 - lat;
    const float before = toneLevel (in.l, 2000.0f, sr, from, to);
    const float after = toneLevel (out.l, 2000.0f, sr, from + lat, to + lat);
    const float cut = gainToDb (before) - gainToDb (after);
    std::printf ("  resonance at 2 kHz turned down by %.1f dB\n", cut);
    CHECK (cut > 8.0f && cut < 13.0f, "a strong resonance is reduced by about Depth");

    // the noise around it is (nearly) left alone
    const float nIn = toneLevel (in.l, 600.0f, sr, from, to), nOut = toneLevel (out.l, 600.0f, sr, from + lat, to + lat);
    CHECK (std::abs (gainToDb (nIn) - gainToDb (nOut)) < 1.5f, "the noise floor away from the peak is untouched");
    CHECK (r.p.engine->reductionAt (2000.0f) > 10.0f, "the detector holds the 2 kHz bin");
    CHECK (r.p.engine->reductionAt (600.0f) < 1.0f, "and nothing at 600 Hz");
}

static void testNoiseIsLeftAlone()
{
    Rig r;
    const auto in = noise (3 * 48000, 0.1f, 11);
    const auto out = r.run (in);
    const int lat = r.p.getLatencySamples();
    const float d = gainToDb (rms (out.l, 48000 + lat, 3 * 48000)) - gainToDb (rms (in.l, 48000, 3 * 48000 - lat));
    std::printf ("  white noise level change %.2f dB\n", d);
    CHECK (std::abs (d) < 0.6f, "white noise passes at default settings");
}

static void testCurveAndRange()
{
    const double sr = 48000.0;
    auto make = [] { auto in = noise (3 * 48000, 0.05f, 5); addSine (in, 3000.0f, 0.3f, 48000.0); return in; };
    float reduction[3];
    for (int k = 0; k < 3; ++k)
    {
        Rig r (sr);
        r.set (id::depth, 12.0f);
        if (k == 1) { r.set (id::band (3, "freq"), 3000.0f); r.set (id::band (3, "gain"), -12.0f); }   // protect 3 kHz
        if (k == 2) { r.set (id::lo, 100.0f); r.set (id::hi, 1000.0f); }                                // range below it
        r.run (make());
        reduction[k] = r.p.engine->reductionAt (3000.0f);
    }
    std::printf ("  3 kHz reduction: plain %.1f, curve -12 dB %.1f, outside range %.1f\n", reduction[0], reduction[1], reduction[2]);
    CHECK (reduction[1] < reduction[0] * 0.3f, "a -12 dB sensitivity node protects its region");
    CHECK (reduction[2] < 0.01f, "nothing happens outside the range");

    Rig r (sr);
    r.set (id::band (2, "freq"), 1000.0f); r.set (id::band (2, "gain"), 6.0f);
    r.run (noise (4096, 0.01f, 1));
    CHECK (std::abs (r.p.engine->curveMultiplierAt (1000.0f) - 2.0f) < 0.05f, "+6 dB sensitivity doubles the reduction");
}

static void testDelta()
{
    auto in = noise (2 * 48000, 0.05f, 9);
    addSine (in, 1500.0f, 0.3f, 48000.0);
    Rig a, b;
    a.set (id::depth, 10.0f); b.set (id::depth, 10.0f); b.set (id::delta, 1.0f);
    const auto wet = a.run (in), removed = b.run (in);
    Stereo sum = wet;
    for (size_t i = 0; i < sum.l.size(); ++i) { sum.l[i] += removed.l[i]; sum.r[i] += removed.r[i]; }
    const int lat = a.p.getLatencySamples();
    CHECK (maxDelayedError (sum, in, lat, 4800) < 1.0e-4f, "output + delta = the dry signal");
    CHECK (rms (removed.l, 48000, 96000) > 0.01f, "delta carries the removed resonance");
}

static void testStereoModes()
{
    const double sr = 48000.0;
    // a resonance only in the side: Mid mode leaves it, Side mode takes it
    auto in = noise (3 * 48000, 0.03f, 13);
    addSine (in, 2500.0f, 0.25f, sr, true, true, -1.0f);
    float cut[2];
    for (int m = 0; m < 2; ++m)
    {
        Rig r (sr);
        r.set (id::depth, 12.0f);
        r.set (id::stereo, m == 0 ? 1.0f : 2.0f);
        const auto out = r.run (in);
        const int lat = r.p.getLatencySamples();
        cut[m] = gainToDb (toneLevel (in.l, 2500.0f, sr, 48000, 3 * 48000 - lat)) - gainToDb (toneLevel (out.l, 2500.0f, sr, 48000 + lat, 3 * 48000));
    }
    std::printf ("  side resonance: Mid mode %.1f dB, Side mode %.1f dB\n", cut[0], cut[1]);
    CHECK (std::abs (cut[0]) < 1.0f, "Mid mode does not touch the side");
    CHECK (cut[1] > 6.0f, "Side mode reduces a side resonance");

    // link: a resonance only on the left is also cut on the right when linked, not when unlinked
    auto one = noise (2 * 48000, 0.03f, 17);
    addSine (one, 1800.0f, 0.3f, sr, true, false);
    addSine (one, 1800.0f, 0.01f, sr, false, true);
    float right[2];
    for (int k = 0; k < 2; ++k)
    {
        Rig r (sr);
        r.set (id::depth, 12.0f);
        r.set (id::link, k == 0 ? 0.0f : 100.0f);
        const auto out = r.run (one);
        const int lat = r.p.getLatencySamples();
        right[k] = gainToDb (toneLevel (one.r, 1800.0f, sr, 48000, 2 * 48000 - lat)) - gainToDb (toneLevel (out.r, 1800.0f, sr, 48000 + lat, 2 * 48000));
    }
    CHECK (right[1] > right[0] + 3.0f, "linked channels share the reduction");
}

static void testBlockSizes()
{
    auto in = noise (48000, 0.05f, 21);
    addSine (in, 900.0f, 0.3f, 48000.0);
    Rig a (48000.0, 64), b (48000.0, 1024);
    for (auto* r : { &a, &b }) { r->set (id::depth, 9.0f); r->set (id::attack, 1.0f); }
    std::mt19937 g (3);
    const auto x = a.run (in);
    const auto y = b.run (in, false, [&g] { return 1 + (int) (g() % 1500); });
    float e = 0.0f;
    for (size_t i = 0; i < x.l.size(); ++i) e = std::max (e, std::abs (x.l[i] - y.l[i]));
    CHECK (e < 1.0e-6f, "output does not depend on the block size");
}

static void testBypass()
{
    auto in = noise (48000, 0.1f, 4);
    addSine (in, 2000.0f, 0.3f, 48000.0);
    Rig r;
    r.set (id::depth, 12.0f);
    const auto out = r.run (in, true);
    CHECK (maxDelayedError (out, in, r.p.getLatencySamples(), 0) < 1.0e-6f, "bypass: dry, delayed by the latency");
}

static void testResolutionChange()
{
    Rig r;
    CHECK (r.p.getLatencySamples() == 2048, "normal resolution at 48 kHz: 2048");
    r.set (id::res, 2.0f);
    r.run (noise (4800, 0.1f, 2));
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    CHECK (r.p.engine->latency() == 4096, "the engine switches to 4096");
    CHECK (r.p.getLatencySamples() == 4096, "and the host is told");
}

static void testState()
{
    juce::MemoryBlock blob;
    {
        Rig r;
        r.set (id::depth, 13.5f); r.set (id::sharp, 77.0f); r.set (id::stereo, 3.0f); r.set (id::band (4, "freq"), 6100.0f);
        r.set (id::band (1, "on"), 0.0f); r.set (id::delta, 1.0f); r.set (id::res, 0.0f);
        r.p.uiBand = 4;
        r.p.getStateInformation (blob);
    }
    Rig r;
    r.p.setStateInformation (blob.getData(), (int) blob.getSize());
    CHECK (std::abs (r.p.value (id::depth) - 13.5f) < 0.01f, "depth restored");
    CHECK (std::abs (r.p.value (id::sharp) - 77.0f) < 0.01f, "sharpness restored");
    CHECK ((int) r.p.value (id::stereo) == 3, "stereo mode restored");
    CHECK (std::abs (r.p.value (id::band (4, "freq")) - 6100.0f) < 1.0f, "band frequency restored");
    CHECK (r.p.value (id::band (1, "on")) < 0.5f, "band switch restored");
    CHECK (r.p.value (id::delta) > 0.5f, "delta restored");
    CHECK (r.p.uiBand.load() == 4, "selected band restored");
}

static void testPresets()
{
    Rig r;
    const int n = (int) factoryPresets().size();
    CHECK (r.p.getNumPrograms() == n, "programs are the factory presets");
    auto in = noise (24000, 0.05f, 8);
    addSine (in, 3300.0f, 0.2f, 48000.0);
    for (int i = 0; i < n; ++i)
    {
        r.p.setCurrentProgram (i);
        const auto out = r.run (in);
        bool finite = true;
        for (float v : out.l) finite = finite && std::isfinite (v);
        CHECK (finite, factoryPresets()[(size_t) i].name);
    }
    r.p.setCurrentProgram (1);
    CHECK (std::abs (r.p.value (id::lo) - 400.0f) < 1.0f, "a preset sets the range");
    r.p.setCurrentProgram (0);
    CHECK (std::abs (r.p.value (id::lo) - 20.0f) < 0.01f, "Default puts it back");

    // a user preset round trip
    r.set (id::depth, 17.0f);
    const juce::String name = "zz SmoothEQ test " + juce::String (juce::Random::getSystemRandom().nextInt (100000));
    CHECK (r.p.presets.save (name), "user preset saved");
    r.set (id::depth, 3.0f);
    r.p.presets.load (r.p.presets.getNames().indexOf (name));
    CHECK (std::abs (r.p.value (id::depth) - 17.0f) < 0.01f, "user preset loaded");
    CHECK (r.p.presets.removeCurrent(), "user preset removed");
}

static void testRobust()
{
    Rig r (96000.0, 333);
    r.set (id::depth, 24.0f); r.set (id::sharp, 100.0f); r.set (id::select, 0.0f); r.set (id::attack, 0.5f); r.set (id::release, 5.0f);
    r.set (id::mode, 1.0f);
    for (int b = 0; b < kNumBands; ++b) r.set (id::band (b, "gain"), 12.0f);
    Stereo in { std::vector<float> (96000), std::vector<float> (96000) };
    for (int i = 0; i < 96000; ++i)
    {
        const int seg = i / 16000;
        float v = 0.0f;
        if (seg == 1) v = (i / 50) % 2 ? 1.0f : -1.0f;            // full-scale square
        if (seg == 2) v = 0.9f;                                     // DC
        if (seg == 3) v = 1.0e-30f;                                 // denormal range
        if (seg == 4) v = (float) std::sin (i * 0.2) * 4.0f;        // way over full scale
        in.l[(size_t) i] = v; in.r[(size_t) i] = -v;
    }
    const auto out = r.run (in);
    bool finite = true; float peak = 0.0f;
    for (size_t i = 0; i < out.l.size(); ++i) { finite = finite && std::isfinite (out.l[i]) && std::isfinite (out.r[i]); peak = std::max (peak, std::abs (out.l[i])); }
    CHECK (finite, "no NaN or Inf on hostile input");
    CHECK (peak < 8.0f, "no blow-up");
    r.set (id::lo, 15000.0f); r.set (id::hi, 100.0f);   // crossed range: no work, no crash
    r.run (noise (9600, 0.1f, 1));
    ++passes;
}

static void testInterface()
{
    Rig r;
    r.run (noise (9600, 0.1f, 1));
    std::unique_ptr<juce::AudioProcessorEditor> ed (r.p.createEditor());
    auto* se = dynamic_cast<SmoothEditor*> (ed.get());
    CHECK (se != nullptr, "editor");
    for (int b = 0; b < kNumBands; ++b) { se->selectBand (b); se->tickForSnapshot(); }
    se->setSize (768, 480); se->setSize (2560, 1600);
    CHECK (std::abs (r.p.uiScale.load() - 2.0f) < 0.01f, "the window scale is kept");
}

static void testRealtime()
{
    Rig r (48000.0, 256);
    r.set (id::res, 2.0f);
    r.p.prepareToPlay (48000.0, 256);
    const auto in = noise (10 * 48000, 0.1f, 6);
    const auto t0 = std::chrono::steady_clock::now();
    r.run (in);
    const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("  cost: %.2f %% of one core (High Res, stereo, 48 kHz)\n", 100.0 * s / 10.0);
    CHECK (s < 10.0, "faster than real time");
}

// ---------------------------------------------------------------- snapshot

static void runSnap (const juce::String& file, int band, int preset, float scale)
{
    juce::ScopedJuceInitialiser_GUI gui;
    Rig r;
    if (preset >= 0) r.p.presets.load (preset);
    else
    {   // something to look at: a sensitivity curve with some shape
        r.set (id::depth, 9.0f);
        r.set (id::band (0, "gain"), -6.0f); r.set (id::band (2, "gain"), 4.0f); r.set (id::band (3, "gain"), 3.0f);
        r.set (id::band (3, "q"), 1.6f); r.set (id::band (4, "on"), 0.0f); r.set (id::lo, 60.0f);
    }
    // pink-ish noise with a few ringing resonances
    auto in = noise (48000, 0.0f, 1);
    std::mt19937 g (5); std::normal_distribution<float> d;
    float lp = 0.0f;
    for (size_t i = 0; i < in.l.size(); ++i) { lp = 0.97f * lp + 0.03f * d (g); in.l[i] = in.r[i] = 0.25f * lp + 0.01f * d (g); }
    addSine (in, 340.0f, 0.12f, 48000.0); addSine (in, 2750.0f, 0.06f, 48000.0); addSine (in, 6400.0f, 0.03f, 48000.0);
    r.run (in);
    std::unique_ptr<juce::AudioProcessorEditor> ed (r.p.createEditor());
    auto* se = dynamic_cast<SmoothEditor*> (ed.get());
    se->selectBand (band);
    for (int k = 0; k < 6; ++k) { r.run (noise (2048, 0.0f, 1)); }
    // the analyser shows the last frames of the test signal
    for (int k = 0; k < 10; ++k)
    {
        Stereo part { std::vector<float> (in.l.begin() + k * 1600, in.l.begin() + (k + 1) * 1600),
                      std::vector<float> (in.r.begin() + k * 1600, in.r.begin() + (k + 1) * 1600) };
        r.run (part);
        se->tickForSnapshot (1);
    }
    se->setSize (juce::roundToInt (1280 * scale), juce::roundToInt (800 * scale));
    auto img = se->createComponentSnapshot (se->getLocalBounds(), true, 1.0f);
    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile (file);
    f.deleteFile();
    juce::FileOutputStream o (f);
    juce::PNGImageFormat().writeImageToStream (img, o);
    ed.reset();
}

int main (int argc, char** argv)
{
    if (argc > 1 && juce::String (argv[1]) == "--snap")
    {
        auto arg = [&] (int i, int def) { return argc > i ? juce::String (argv[i]).getIntValue() : def; };
        runSnap (argc > 2 ? argv[2] : "snap_smootheq.png", arg (3, 2), arg (4, -1), argc > 5 ? juce::String (argv[5]).getFloatValue() : 2.0f);
        return 0;
    }
    juce::ScopedJuceInitialiser_GUI gui;
    using Fn = void (*)();
    const std::vector<std::pair<const char*, Fn>> tests = {
        { "null", testNull }, { "resonance", testResonance }, { "noise", testNoiseIsLeftAlone }, { "curve", testCurveAndRange },
        { "delta", testDelta }, { "stereo", testStereoModes }, { "block size", testBlockSizes }, { "bypass", testBypass },
        { "resolution", testResolutionChange }, { "state", testState }, { "presets", testPresets }, { "robustness", testRobust },
        { "interface", testInterface }, { "real time", testRealtime } };
    for (auto& t : tests)
    {
        const int before = failures;
        t.second();
        std::printf ("%-12s %s\n", t.first, failures == before ? "ok" : "FAILED");
        std::fflush (stdout);
    }
    std::printf ("%d checks, %d failed\n", passes + failures, failures);
    return failures == 0 ? 0 : 1;
}
