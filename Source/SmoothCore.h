#pragma once
// SMOOTHEQ core: FFT, the sensitivity curve and small helpers. No JUCE here, nothing allocates after prepare().
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <functional>

namespace smooth
{
constexpr float kPi = 3.14159265358979f;
constexpr int kNumBands = 6;             // low shelf, four bells, high shelf
constexpr int kDisplayPoints = 256;      // log-spaced points from 20 Hz to 20 kHz, for the interface
constexpr float kDisplayLo = 20.0f, kDisplayHi = 20000.0f;
constexpr int kNumResolutions = 3;       // FFT 1024 / 2048 / 4096 at 48 kHz, doubled per octave of sample rate
constexpr int kBaseFft[kNumResolutions] = { 1024, 2048, 4096 };
constexpr float kMaxReduction = 30.0f;   // dB, hard ceiling of any bin

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
/** Exact comparison, on purpose (cached values, switches): spelled out so -Wfloat-equal stays quiet. */
inline bool exactlyEqual (float a, float b) noexcept { return std::equal_to<float>() (a, b); }
inline float clampf (float v, float lo, float hi) noexcept { return std::min (hi, std::max (lo, v)); }

/** Frequency of display point i (0 .. kDisplayPoints - 1). */
inline float displayFreq (float i) noexcept
{
    return kDisplayLo * std::pow (kDisplayHi / kDisplayLo, i / (float) (kDisplayPoints - 1));
}

/** FFT size for a resolution at a sample rate: the same resolution in hertz from 44.1 kHz to 192 kHz. */
inline int fftSizeFor (int resolution, double sampleRate) noexcept
{
    const int factor = sampleRate > 150000.0 ? 4 : (sampleRate > 75000.0 ? 2 : 1);
    return kBaseFft[std::min (std::max (resolution, 0), kNumResolutions - 1)] * factor;
}

// ---------------------------------------------------------------- FFT

/** In-place complex radix-2 FFT. Tables are built for the largest size in prepare(); any smaller power of two
    reuses them (the twiddles by stride), so changing size on the audio thread allocates nothing. */
class Fft
{
public:
    void prepare (int maxSize)
    {
        maxN = maxSize;
        twiddle.resize ((size_t) maxN / 2);
        for (int i = 0; i < maxN / 2; ++i)
        {
            const double a = -2.0 * 3.14159265358979323846 * i / maxN;
            twiddle[(size_t) i] = { (float) std::cos (a), (float) std::sin (a) };
        }
        bitrev.resize ((size_t) maxN);
        setSize (maxN);
    }

    void setSize (int size) noexcept
    {
        n = size;
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            bitrev[(size_t) i] = r;
        }
    }

    int size() const noexcept { return n; }

    /** inverse: no 1/N scaling, the caller folds it into the synthesis window. */
    void transform (std::complex<float>* x, bool inverse) const noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = bitrev[(size_t) i];
            if (j > i) std::swap (x[i], x[j]);
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, stride = maxN / len;
            for (int i = 0; i < n; i += len)
                for (int k = 0; k < half; ++k)
                {
                    auto w = twiddle[(size_t) (k * stride)];
                    if (inverse) w = std::conj (w);
                    const auto a = x[i + k], b = x[i + k + half] * w;
                    x[i + k] = a + b;
                    x[i + k + half] = a - b;
                }
        }
    }

private:
    int maxN = 0, n = 0;
    std::vector<std::complex<float>> twiddle;
    std::vector<int> bitrev;
};

// ---------------------------------------------------------------- sensitivity curve

enum BandType { btLowShelf = 0, btBell, btHighShelf };

struct Band
{
    bool on = true;
    float freq = 1000.0f;   // Hz
    float gain = 0.0f;      // dB, -12 .. +12: how much more (or less) the detector acts here
    float q = 1.0f;         // bells: bandwidth ~ 1/q octaves. Shelves: steepness
    int type = btBell;

    bool operator== (const Band& o) const noexcept
    {
        return on == o.on && exactlyEqual (freq, o.freq) && exactlyEqual (gain, o.gain) && exactlyEqual (q, o.q) && type == o.type;
    }
};

using Bands = std::array<Band, kNumBands>;

inline Bands defaultBands() noexcept
{
    Bands b;
    const float f[kNumBands] = { 120.0f, 350.0f, 1200.0f, 3500.0f, 7500.0f, 12000.0f };
    for (int i = 0; i < kNumBands; ++i)
    {
        b[(size_t) i].freq = f[i];
        b[(size_t) i].type = i == 0 ? btLowShelf : (i == kNumBands - 1 ? btHighShelf : btBell);
    }
    return b;
}

/** Sensitivity of the detector at f, in dB. Smooth shapes on a log-frequency axis: a Gaussian bell whose width at
    half height is 1/q octaves, and logistic shelves. Shared by the engine and the drawing, so what you see is used. */
inline float bandDb (const Band& b, float f) noexcept
{
    if (! b.on || exactlyEqual (b.gain, 0.0f)) return 0.0f;
    const float oct = std::log2 (std::max (f, 1.0f) / b.freq);
    if (b.type == btBell)
    {
        const float sigma = 1.0f / (2.355f * std::max (b.q, 0.05f));
        return b.gain * std::exp (-0.5f * (oct / sigma) * (oct / sigma));
    }
    const float k = 2.5f * std::max (b.q, 0.1f);
    const float s = 1.0f / (1.0f + std::exp (-k * oct * 2.0f));      // 0 below the corner, 1 above
    return b.gain * (b.type == btHighShelf ? s : 1.0f - s);
}

inline float curveDb (const Bands& bands, float f) noexcept
{
    float s = 0.0f;
    for (auto& b : bands) s += bandDb (b, f);
    return s;
}

/** The working range: full sensitivity between lo and hi, a third-octave raised-cosine fade outside. */
inline float rangeWeight (float f, float lo, float hi) noexcept
{
    auto fade = [] (float octOutside) {
        if (octOutside <= 0.0f) return 1.0f;
        if (octOutside >= 0.333f) return 0.0f;
        return 0.5f + 0.5f * std::cos (kPi * octOutside / 0.333f);
    };
    const float l = std::log2 (std::max (f, 1.0f));
    return fade (std::log2 (lo) - l) * fade (l - std::log2 (hi));
}

/** Multiplier on the reduction from the curve: +6 dB doubles it, -6 dB halves it. */
inline float curveMultiplier (float curveDbValue) noexcept { return std::exp2 (clampf (curveDbValue, -24.0f, 12.0f) / 6.0f); }
} // namespace smooth
