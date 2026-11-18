#pragma once
// SMOOTHEQ engine: dynamic resonance suppression on a short-time Fourier transform.
//
// Every hop (a quarter of the FFT) the last N samples of both channels go through one complex FFT (left in the real
// part, right in the imaginary part). For each bin the detector compares the level with the level of its neighbourhood
// on a log-frequency axis (the "baseline"): what pokes out of it by more than the selectivity threshold is a resonance,
// and that bin is turned down by the excess, at most by Depth, scaled by the sensitivity curve. The reduction moves
// with attack and release, is smoothed across neighbouring bins, then multiplies the spectrum. Hann windows on both
// sides at 75 % overlap add up to a constant, so with nothing to reduce the output is the input delayed by N samples.
//
// No JUCE, real-time safe: everything is allocated in prepare(); process() only reads and writes preallocated memory.
#include "SmoothCore.h"

namespace smooth
{
enum StereoMode { smStereo = 0, smMid, smSide, smMidSide };

struct Settings
{
    float depth = 6.0f;          // dB, the most a bin is turned down (before the curve)
    float sharpness = 0.5f;      // 0..1: baseline window from 2 octaves (broad bumps count) to 1/6 octave (narrow only)
    float selectivity = 0.4f;    // 0..1: how far above the baseline a peak must be (0.5 .. 12 dB)
    float attackMs = 5.0f;
    float releaseMs = 80.0f;
    bool soft = true;            // soft knee: small excesses are reduced less
    int resolution = 1;          // 0 low latency, 1 normal, 2 high resolution
    float lo = 20.0f, hi = 20000.0f;
    Bands bands = defaultBands();
    int stereo = smStereo;
    float link = 1.0f;           // 0..1, how much the two channels share their reduction
    float mix = 1.0f;            // 0..1
    float trimDb = 0.0f;
    bool delta = false;          // output what is removed
};

/** What the interface reads. Written by the audio thread once per frame, read by the message thread. */
struct UiState
{
    std::array<std::atomic<float>, kDisplayPoints> in {}, gr {};
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f }, grPeak { 0.0f };
    std::atomic<int> frames { 0 };
};

class Engine
{
public:
    UiState ui;

    /** Allocates for the largest FFT any resolution can need at this sample rate. */
    void prepare (double sampleRate, int resolution)
    {
        sr = sampleRate;
        maxN = fftSizeFor (kNumResolutions - 1, sr);
        fft.prepare (maxN);
        const size_t n = (size_t) maxN, bins = n / 2 + 1;
        window.assign (n, 0.0f); synth.assign (n, 0.0f);
        spec.assign (n, {});
        for (size_t c = 0; c < 2; ++c)
        {
            inBuf[c].assign (n, 0.0f); outAcc[c].assign (n, 0.0f); dry[c].assign (n + 1, 0.0f);
            x[c].assign (bins, {}); power[c].assign (bins, 0.0f); db[c].assign (bins, 0.0f);
            grState[c].assign (bins, 0.0f); grTarget[c].assign (bins, 0.0f);
        }
        prefix.assign (bins + 1, 0.0);
        tmp.assign (bins, 0.0f);
        mult.assign (bins, 1.0f);
        freqOf.assign (bins, 0.0f);
        configure (fftSizeFor (resolution, sr));
        mixNow = -1.0f;
    }

    int latency() const noexcept { return N; }
    int fftSize() const noexcept { return N; }
    double sampleRate() const noexcept { return sr; }

    /** Clears every buffer: the output starts again from silence (also after a resolution change). */
    void reset() noexcept
    {
        for (size_t c = 0; c < 2; ++c)
        {
            std::fill (inBuf[c].begin(), inBuf[c].end(), 0.0f);
            std::fill (outAcc[c].begin(), outAcc[c].end(), 0.0f);
            std::fill (dry[c].begin(), dry[c].end(), 0.0f);
            std::fill (power[c].begin(), power[c].end(), 0.0f);
            std::fill (grState[c].begin(), grState[c].end(), 0.0f);
        }
        pos = 0; dryPos = 0;
        for (auto& v : ui.in) v.store (-120.0f, std::memory_order_relaxed);
        for (auto& v : ui.gr) v.store (0.0f, std::memory_order_relaxed);
    }

    /** right may be null (mono). bypass fades to the delayed dry signal, so the latency never changes. */
    void process (float* left, float* right, int numSamples, const Settings& s, bool bypass) noexcept
    {
        const int wantN = fftSizeFor (s.resolution, sr);
        if (wantN != N && wantN <= maxN) configure (wantN);
        if (! curveValid || ! (s.bands == curveBands) || ! exactlyEqual (s.lo, curveLo) || ! exactlyEqual (s.hi, curveHi)) buildCurve (s);
        settings = &s;

        const bool stereoIn = right != nullptr;
        const bool ms = stereoIn && s.stereo != smStereo;
        active[0] = ! ms || s.stereo != smSide;
        active[1] = stereoIn && (! ms || s.stereo != smMid);

        const float mixTarget = clampf (s.mix, 0.0f, 1.0f), trimTarget = dbToGain (s.trimDb);
        const float bypassTarget = bypass ? 1.0f : 0.0f;
        if (mixNow < 0.0f) { mixNow = mixTarget; trimNow = trimTarget; bypassNow = bypassTarget; }
        const float step = 1.0f / (float) (0.02 * sr);   // 20 ms ramps
        float inPk = 0.0f, outPk = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float a = left[i], b = stereoIn ? right[i] : 0.0f;
            inPk = std::max (inPk, std::max (std::abs (a), std::abs (b)));
            float u = a, v = b;
            if (ms) { u = 0.5f * (a + b); v = 0.5f * (a - b); }
            inBuf[0][(size_t) (N - hop + pos)] = u;
            inBuf[1][(size_t) (N - hop + pos)] = v;
            float wu = outAcc[0][(size_t) pos], wv = outAcc[1][(size_t) pos];

            // dry, delayed by exactly N samples
            const float da = dry[0][(size_t) dryPos], dbv = dry[1][(size_t) dryPos];
            dry[0][(size_t) dryPos] = a; dry[1][(size_t) dryPos] = b;
            if (++dryPos >= N) dryPos = 0;

            if (++pos == hop) { frame(); pos = 0; }

            float wa = wu, wb = wv;
            if (ms) { wa = wu + wv; wb = wu - wv; }

            mixNow += clampf (mixTarget - mixNow, -step, step);
            trimNow += clampf (trimTarget - trimNow, -step * 4.0f, step * 4.0f);
            bypassNow += clampf (bypassTarget - bypassNow, -step, step);

            float oa, ob;
            if (s.delta) { oa = mixNow * (da - wa); ob = mixNow * (dbv - wb); }
            else         { oa = da + mixNow * (wa - da); ob = dbv + mixNow * (wb - dbv); }
            oa *= trimNow; ob *= trimNow;
            oa += bypassNow * (da - oa); ob += bypassNow * (dbv - ob);
            left[i] = oa;
            if (stereoIn) right[i] = ob;
            outPk = std::max (outPk, std::max (std::abs (oa), stereoIn ? std::abs (ob) : 0.0f));
        }
        ui.inPeak.store (std::max (inPk, ui.inPeak.load (std::memory_order_relaxed)), std::memory_order_relaxed);
        ui.outPeak.store (std::max (outPk, ui.outPeak.load (std::memory_order_relaxed)), std::memory_order_relaxed);
    }

    /** Reduction (dB, >= 0) the engine applies at a frequency right now, max of the channels. For tests. */
    float reductionAt (float f) const noexcept
    {
        const int k = std::min (N / 2, std::max (0, (int) std::lround (f * N / sr)));
        return std::max (grState[0][(size_t) k], grState[1][(size_t) k]);
    }

    float curveMultiplierAt (float f) const noexcept
    {
        const int k = std::min (N / 2, std::max (0, (int) std::lround (f * N / sr)));
        return mult[(size_t) k];
    }

private:
    void configure (int size) noexcept
    {
        N = size; hop = N / 4;
        fft.setSize (N);
        // periodic Hann on both sides; Hann^2 at 75 % overlap sums to 1.5; the inverse FFT needs 1/N
        for (int i = 0; i < N; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (2.0f * kPi * (float) i / (float) N);
            window[(size_t) i] = w;
            synth[(size_t) i] = w / (1.5f * (float) N);
        }
        // a full-scale sine reads 0 dB: its peak bin is N/4 with this window, and the two channels share one FFT
        normDb = 20.0f * std::log10 ((float) N / 4.0f);
        for (int k = 0; k <= N / 2; ++k) freqOf[(size_t) k] = (float) (k * sr / N);
        for (int p = 0; p < kDisplayPoints; ++p)
        {
            const float f0 = displayFreq ((float) p - 0.5f), f1 = displayFreq ((float) p + 0.5f);
            dispLo[(size_t) p] = std::min (N / 2, std::max (1, (int) std::floor (f0 * N / sr)));
            dispHi[(size_t) p] = std::min (N / 2, std::max (dispLo[(size_t) p], (int) std::ceil (f1 * N / sr)));
            dispPos[(size_t) p] = std::min ((float) N / 2.0f - 1.001f, std::max (1.0f, displayFreq ((float) p) * (float) N / (float) sr));
        }
        curveValid = false;
        reset();
    }

    void buildCurve (const Settings& s) noexcept
    {
        curveBands = s.bands; curveLo = s.lo; curveHi = s.hi; curveValid = true;
        for (int k = 0; k <= N / 2; ++k)
        {
            const float f = std::max (freqOf[(size_t) k], 1.0f);
            mult[(size_t) k] = curveMultiplier (curveDb (s.bands, f)) * rangeWeight (f, s.lo, s.hi);
        }
    }

    void frame() noexcept
    {
        const Settings& s = *settings;
        const int half = N / 2;
        for (int i = 0; i < N; ++i)
            spec[(size_t) i] = { inBuf[0][(size_t) i] * window[(size_t) i], inBuf[1][(size_t) i] * window[(size_t) i] };
        fft.transform (spec.data(), false);
        for (int k = 0; k <= half; ++k)
        {
            const auto z = spec[(size_t) k], zc = std::conj (spec[(size_t) ((N - k) % N)]);
            x[0][(size_t) k] = 0.5f * (z + zc);
            x[1][(size_t) k] = std::complex<float> (0.0f, -0.5f) * (z - zc);
        }

        const float framePeriod = (float) hop / (float) sr;
        const float att = std::exp (-framePeriod / std::max (0.0001f, s.attackMs * 0.001f));
        const float rel = std::exp (-framePeriod / std::max (0.001f, s.releaseMs * 0.001f));
        const float det = std::exp (-framePeriod / 0.015f);          // detector: 15 ms power average

        for (size_t c = 0; c < 2; ++c)
        {
            if (! active[c]) { std::fill (grTarget[c].begin(), grTarget[c].begin() + half + 1, 0.0f); }
            else detect (c, s, det);
            auto& st = grState[c];
            for (int k = 0; k <= half; ++k)
            {
                const float t = grTarget[c][(size_t) k], g = st[(size_t) k];
                st[(size_t) k] = t > g ? t + att * (g - t) : t + rel * (g - t);
            }
        }

        // link, then smooth across bins (two passes of 1/4 1/2 1/4) so neighbouring bins never disagree sharply
        const float link = active[1] && active[0] ? clampf (s.link, 0.0f, 1.0f) : 0.0f;
        for (size_t c = 0; c < 2; ++c)
        {
            auto& g = grTarget[c];   // reused as the gain table of this frame
            for (int k = 0; k <= half; ++k)
            {
                const float own = grState[c][(size_t) k], both = std::max (grState[0][(size_t) k], grState[1][(size_t) k]);
                g[(size_t) k] = active[c] ? own + link * (both - own) : 0.0f;
            }
            for (int pass = 0; pass < 2; ++pass)
            {
                tmp[0] = g[0];
                for (int k = 1; k < half; ++k) tmp[(size_t) k] = 0.25f * g[(size_t) k - 1] + 0.5f * g[(size_t) k] + 0.25f * g[(size_t) k + 1];
                tmp[(size_t) half] = g[(size_t) half];
                std::copy (tmp.begin(), tmp.begin() + half + 1, g.begin());
            }
        }
        publish (half);
        for (size_t c = 0; c < 2; ++c)
            for (int k = 0; k <= half; ++k)
                grTarget[c][(size_t) k] = grTarget[c][(size_t) k] > 0.001f ? dbToGain (-std::min (grTarget[c][(size_t) k], kMaxReduction)) : 1.0f;

        // rebuild the full spectrum of (left + i right) from the two half spectra
        for (int k = 0; k <= half; ++k)
        {
            const auto a = x[0][(size_t) k] * grTarget[0][(size_t) k];
            const auto b = x[1][(size_t) k] * grTarget[1][(size_t) k];
            const std::complex<float> j (0.0f, 1.0f);
            spec[(size_t) k] = a + j * b;
            if (k > 0 && k < half) spec[(size_t) (N - k)] = std::conj (a) + j * std::conj (b);
        }
        fft.transform (spec.data(), true);

        for (size_t c = 0; c < 2; ++c)
        {
            auto& o = outAcc[c];
            std::copy (o.begin() + hop, o.begin() + N, o.begin());
            std::fill (o.begin() + (N - hop), o.begin() + N, 0.0f);
            for (int i = 0; i < N; ++i)
                o[(size_t) i] += (c == 0 ? spec[(size_t) i].real() : spec[(size_t) i].imag()) * synth[(size_t) i];
            auto& in = inBuf[c];
            std::copy (in.begin() + hop, in.begin() + N, in.begin());
        }
    }

    void detect (size_t c, const Settings& s, float detCoef) noexcept
    {
        const int half = N / 2;
        auto& p = power[c];
        auto& d = db[c];
        for (int k = 0; k <= half; ++k)
        {
            const float m = std::norm (x[c][(size_t) k]);
            p[(size_t) k] = m + detCoef * (p[(size_t) k] - m);
        }
        // level per bin, lightly smoothed over three bins, in dB full scale
        for (int k = 0; k <= half; ++k)
        {
            const float l = p[(size_t) std::max (0, k - 1)], r = p[(size_t) std::min (half, k + 1)];
            d[(size_t) k] = 10.0f * std::log10 (0.25f * l + 0.5f * p[(size_t) k] + 0.25f * r + 1.0e-20f) - normDb;
        }
        prefix[0] = 0.0;
        for (int k = 0; k <= half; ++k) prefix[(size_t) k + 1] = prefix[(size_t) k] + (double) d[(size_t) k];

        const float octaves = std::exp2 (1.0f + clampf (s.sharpness, 0.0f, 1.0f) * (std::log2 (1.0f / 6.0f) - 1.0f));
        const float spread = std::exp2 (octaves * 0.5f) - 1.0f;
        const float minHalf = 4.0f * (float) N / (float) fftSizeFor (1, sr);   // a few Hann main lobes at any size
        const float threshold = 0.5f + 11.5f * clampf (s.selectivity, 0.0f, 1.0f);
        const float depth = clampf (s.depth, 0.0f, kMaxReduction);
        auto& t = grTarget[c];
        t[0] = 0.0f;
        for (int k = 1; k <= half; ++k)
        {
            const int w = (int) std::max (std::max (minHalf, 2.0f), (float) k * spread);
            const int a = std::max (1, k - w), b = std::min (half, k + w);
            const float base = (float) ((prefix[(size_t) b + 1] - prefix[(size_t) a]) / (double) (b - a + 1));
            float e = d[(size_t) k] - base - threshold;
            if (e <= 0.0f || d[(size_t) k] < -100.0f || depth <= 0.0f) { t[(size_t) k] = 0.0f; continue; }
            if (s.soft) e = e * e / (e + 4.0f);
            t[(size_t) k] = std::min (kMaxReduction, std::min (e, depth) * mult[(size_t) k]);
        }
    }

    void publish (int half) noexcept
    {
        float peak = 0.0f;
        for (int k = 1; k <= half; ++k)
            peak = std::max (peak, std::max (grTarget[0][(size_t) k], grTarget[1][(size_t) k]));
        for (int pnt = 0; pnt < kDisplayPoints; ++pnt)
        {
            float lvl = -140.0f, red = 0.0f;
            if (dispHi[(size_t) pnt] - dispLo[(size_t) pnt] <= 2)
            {   // fewer bins than points down low: interpolate, or the curve turns into steps
                const float at = dispPos[(size_t) pnt];
                const auto k = (size_t) at;
                const float t = at - (float) k;
                for (size_t c = 0; c < 2; ++c)
                    if (active[c] || c == 0) lvl = std::max (lvl, db[c][k] + t * (db[c][k + 1] - db[c][k]));
                for (size_t c = 0; c < 2; ++c)
                    red = std::max (red, grTarget[c][k] + t * (grTarget[c][k + 1] - grTarget[c][k]));
            }
            else
                for (int k = dispLo[(size_t) pnt]; k <= dispHi[(size_t) pnt]; ++k)
                {
                    for (size_t c = 0; c < 2; ++c)
                        if (active[c] || c == 0) lvl = std::max (lvl, db[c][(size_t) k]);
                    red = std::max (red, std::max (grTarget[0][(size_t) k], grTarget[1][(size_t) k]));
                }
            if (! active[0] && ! active[1]) lvl = -140.0f;
            ui.in[(size_t) pnt].store (lvl, std::memory_order_relaxed);
            ui.gr[(size_t) pnt].store (red, std::memory_order_relaxed);
        }
        ui.grPeak.store (std::max (peak, ui.grPeak.load (std::memory_order_relaxed)), std::memory_order_relaxed);
        ui.frames.fetch_add (1, std::memory_order_relaxed);
    }

    double sr = 48000.0;
    int maxN = 0, N = 2048, hop = 512, pos = 0, dryPos = 0;
    float normDb = 0.0f;
    Fft fft;
    std::vector<float> window, synth, tmp, mult, freqOf;
    std::vector<std::complex<float>> spec;
    std::array<std::vector<float>, 2> inBuf, outAcc, dry, power, db, grState, grTarget;
    std::array<std::vector<std::complex<float>>, 2> x;
    std::vector<double> prefix;
    std::array<int, kDisplayPoints> dispLo {}, dispHi {};
    std::array<float, kDisplayPoints> dispPos {};
    bool active[2] { true, true };
    const Settings* settings = nullptr;
    Bands curveBands;
    float curveLo = 0.0f, curveHi = 0.0f;
    bool curveValid = false;
    float mixNow = -1.0f, trimNow = 1.0f, bypassNow = 0.0f;
};
} // namespace smooth
