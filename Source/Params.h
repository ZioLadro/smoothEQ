#pragma once
// Parameter helpers and the parameter ids. Never rename an id or reorder a choice list: hosts store them in sessions.
#include <juce_audio_processors/juce_audio_processors.h>
#include "SmoothCore.h"

namespace smooth
{
inline std::unique_ptr<juce::AudioParameterFloat> fparam (const juce::String& id, const juce::String& name, float lo, float hi,
                                                          float def, const juce::String& unit = {}, float skewCentre = -1.0f, int decimals = -1)
{
    juce::NormalisableRange<float> r (lo, hi);
    if (skewCentre > lo && skewCentre < hi) r.setSkewForCentre (skewCentre);
    const int dec = decimals >= 0 ? decimals : ((hi - lo) >= 90.0f ? 0 : 1);
    auto toText = [dec, unit] (float v, int) {
        juce::String number;
        if (unit == "Hz" && v >= 1000.0f) return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
        number = dec > 0 ? juce::String (v, dec) : juce::String (juce::roundToInt (v));
        return number + (unit.isNotEmpty() ? " " + unit : juce::String());
    };
    auto fromText = [] (const juce::String& t) {
        const auto v = t.retainCharacters ("0123456789.-").getFloatValue();
        return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
    };
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                        juce::AudioParameterFloatAttributes().withLabel (unit)
                                                            .withStringFromValueFunction (toText).withValueFromStringFunction (fromText));
}

// On/Off as a 2-item choice: it always stores a snapped value, so state save/restore round-trips exactly (pluginval 8+).
inline std::unique_ptr<juce::AudioParameterChoice> bparam (const juce::String& id, const juce::String& name, bool def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, juce::StringArray { "Off", "On" }, def ? 1 : 0);
}

inline std::unique_ptr<juce::AudioParameterChoice> cparam (const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def);
}

namespace id
{
    inline constexpr const char* depth = "depth";
    inline constexpr const char* sharp = "sharp";
    inline constexpr const char* select = "select";
    inline constexpr const char* attack = "attack";
    inline constexpr const char* release = "release";
    inline constexpr const char* mode = "mode";
    inline constexpr const char* res = "res";
    inline constexpr const char* lo = "lo";
    inline constexpr const char* hi = "hi";
    inline constexpr const char* stereo = "stereo";
    inline constexpr const char* link = "link";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* trim = "trim";
    inline constexpr const char* delta = "delta";

    /** Band parameters: b1_on, b1_freq, b1_gain, b1_q ... b6_q. Band 1 is the low shelf, 6 the high shelf. */
    inline juce::String band (int b, const char* what) { return "b" + juce::String (b + 1) + "_" + what; }
}

inline const juce::StringArray& modeNames()       { static const juce::StringArray a { "Soft", "Hard" }; return a; }
inline const juce::StringArray& resolutionNames() { static const juce::StringArray a { "Low Latency", "Normal", "High Res" }; return a; }
inline const juce::StringArray& stereoNames()     { static const juce::StringArray a { "Stereo", "Mid", "Side", "Mid/Side" }; return a; }
inline const char* bandName (int b)
{
    static const char* n[kNumBands] = { "Low Shelf", "Bell 1", "Bell 2", "Bell 3", "Bell 4", "High Shelf" };
    return n[b];
}
inline const char* bandShort (int b)
{
    static const char* n[kNumBands] = { "LS", "1", "2", "3", "4", "HS" };
    return n[b];
}
} // namespace smooth
