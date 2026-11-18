#pragma once
// SMOOTHEQ UI parts. Same design language as SONIC GRAIN, DR. SEGG-UENCER and MULTITAILS (glass panels, slanted black
// plates, dark knobs with a yellow value arc, pixel-font readouts, text-only section titles). The ground is violet with a
// mint slab and silk ribbons behind the panels; a "silk band" (a sine wave on mint) edges the header and the deck where
// the siblings have hazard stripes or the fur band.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "SmoothProcessor.h"
#include "SmoothFonts.h"

namespace smooth::ui
{
namespace c
{
    // only the ground layer, its ornaments and the preset tag differ from the sibling plugins
    const juce::Colour ground    { 0xff6c35de };   // violet
    const juce::Colour groundAlt { 0xff3b1789 };   // header strip, white on it 11:1
    const juce::Colour slab      { 0xff19c6a6 };   // mint diagonal band behind the right side
    const juce::Colour silk      { 0xff8ff3dc };   // ribbon body
    const juce::Colour silkTip   { 0xfffdfcf8 };   // ribbon highlight
    const juce::Colour silkLine  { 0xff1d0b52 };   // ribbon outline
    const juce::Colour silkDeep  { 0xff0e8f78 };   // silk band ground
    const juce::Colour modGround { 0xff17191d };   // deck
    const juce::Colour glass     { 0xf5f6f7fa };   // rgba(246, 247, 250, 0.96)
    const juce::Colour panelShadow { 0x52200a50 };
    const juce::Colour glassEdge { 0xe6ffffff };
    const juce::Colour ink       { 0xff0b1430 };
    const juce::Colour inkSoft   { 0xff33415f };
    const juce::Colour plate     { 0xff111111 };
    const juce::Colour onPlate   { 0xffffffff };
    const juce::Colour accent    { 0xffffd21f };   // value
    const juce::Colour onAccent  { 0xff0b1430 };
    const juce::Colour go        { 0xff1f9e1a };   // active
    const juce::Colour signal    { 0xffe90101 };   // accents
    const juce::Colour tag       { 0xff0e7f6b };   // preset tag and hover: deep mint, white on it 5.0:1
    const juce::Colour knob      { 0xff16212f };
    const juce::Colour knobHi    { 0xff3a4b63 };
    const juce::Colour track     { 0x240b1430 };   // ink at 14 %
    const juce::Colour trackDark { 0x24ffffff };   // white at 14 %, for controls on plates and the deck
    const juce::Colour display   { 0xeb082660 };   // rgba(8, 38, 96, 0.92)
    const juce::Colour checker   { 0x1fffffff };   // white 12 % on the ground and the slab
}

/** Embedded faces (Rubik, Silkscreen; SIL Open Font License, see Resources/fonts). */
struct Fonts
{
    static Fonts& get() { static Fonts f; return f; }
    juce::Typeface::Ptr medium, bold, black, pixel;
private:
    Fonts()
    {
        medium = juce::Typeface::createSystemTypefaceFor (SmoothFonts::RubikMedium_ttf, SmoothFonts::RubikMedium_ttfSize);
        bold   = juce::Typeface::createSystemTypefaceFor (SmoothFonts::RubikBold_ttf, SmoothFonts::RubikBold_ttfSize);
        black  = juce::Typeface::createSystemTypefaceFor (SmoothFonts::RubikBlackItalic_ttf, SmoothFonts::RubikBlackItalic_ttfSize);
        pixel  = juce::Typeface::createSystemTypefaceFor (SmoothFonts::SilkscreenRegular_ttf, SmoothFonts::SilkscreenRegular_ttfSize);
    }
};

inline juce::Font face (const juce::Typeface::Ptr& t, float size, float tracking = 0.0f)
{
    return juce::Font (juce::FontOptions (t).withHeight (size)).withExtraKerningFactor (tracking);
}
inline juce::Font medium (float size, float tracking = 0.0f) { return face (Fonts::get().medium, size, tracking); }
inline juce::Font bold (float size, float tracking = 0.0f)   { return face (Fonts::get().bold, size, tracking); }
inline juce::Font black (float size, float tracking = 0.0f)  { return face (Fonts::get().black, size, tracking); }
// Silkscreen draws on a grid of 1/8 em and JUCE sizes a font by ascent + descent (1.28 em): x 1.024 puts every
// font pixel on exactly one (10) or two (20) screen pixels.
inline juce::Font pixel (float size)                         { return face (Fonts::get().pixel, size * 1.024f); }

/** A rectangle skewed by -30 degrees: the plate every button, tab and tag is drawn on. */
inline juce::Path platePath (juce::Rectangle<float> r)
{
    const float lean = juce::jmin (r.getHeight() * 0.577f, r.getWidth() * 0.3f);
    juce::Path p;
    p.addQuadrilateral (r.getX() + lean, r.getY(), r.getRight(), r.getY(), r.getRight() - lean, r.getBottom(), r.getX(), r.getBottom());
    return p;
}

/** Plate with the stacked hard shadow (signal, then go) used by the preset tag. */
inline void taggedPlate (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill)
{
    g.setColour (c::go);     g.fillPath (platePath (r.translated (7, 7)));
    g.setColour (c::signal); g.fillPath (platePath (r.translated (4, 4)));
    g.setColour (fill);      g.fillPath (platePath (r));
}

/** The silk band: a white sine wave over deep mint. It edges the header and the deck. */
inline void silkBand (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (c::silkDeep);
    g.fillRect (r);
    juce::Path wave;
    const float mid = r.getCentreY(), amp = r.getHeight() * 0.28f;
    wave.startNewSubPath (r.getX(), mid);
    for (float x = r.getX() + 2.0f; x <= r.getRight() + 1.0f; x += 2.0f)
        wave.lineTo (x, mid + amp * std::sin ((x - r.getX()) * 0.105f));
    g.setColour (c::silk);
    g.strokePath (wave, juce::PathStrokeType (juce::jmax (1.2f, r.getHeight() * 0.26f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c::silkLine.withAlpha (0.35f));
    g.fillRect (r.withTop (r.getBottom() - 1.0f));
}

/** One silk ribbon: a band of varying width following a slow sine, with a light fold along its middle. */
inline void ribbon (juce::Graphics& g, float x0, float x1, float y, float amp, float period, float phase, float width)
{
    auto centre = [&] (float x) { return y + amp * std::sin ((x - x0) / period * juce::MathConstants<float>::twoPi + phase); };
    auto w = [&] (float x) { return width * (0.55f + 0.45f * std::cos ((x - x0) / period * juce::MathConstants<float>::pi + phase)); };
    juce::Path body, fold;
    body.startNewSubPath (x0, centre (x0) - w (x0) * 0.5f);
    for (float x = x0 + 6.0f; x <= x1; x += 6.0f) body.lineTo (x, centre (x) - w (x) * 0.5f);
    for (float x = x1; x >= x0; x -= 6.0f) body.lineTo (x, centre (x) + w (x) * 0.5f);
    body.closeSubPath();
    fold.startNewSubPath (x0, centre (x0) - w (x0) * 0.12f);
    for (float x = x0 + 6.0f; x <= x1; x += 6.0f) fold.lineTo (x, centre (x) - w (x) * 0.12f);
    g.setColour (juce::Colour (0x33000000));
    g.fillPath (body, juce::AffineTransform::translation (8.0f, 8.0f));
    g.setGradientFill (juce::ColourGradient (c::silk, x0, 0.0f, c::silkTip, x1, 0.0f, false));
    g.fillPath (body);
    g.setColour (c::silkTip.withAlpha (0.8f));
    g.strokePath (fold, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c::silkLine);
    g.strokePath (body, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

inline void panelBody (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill)
{
    juce::Path body;
    body.addRoundedRectangle (r, 10.0f);
    juce::DropShadow (c::panelShadow, 28, { 0, 12 }).drawForPath (g, body);
    g.setColour (fill);
    g.fillPath (body);
    if (fill == c::glass) { g.setColour (c::glassEdge); g.strokePath (body, juce::PathStrokeType (1.0f)); }
}

/** Section title, text only (family rule since 2026-11-04): kicker Rubik Medium 11 tracking 0.42 in ink-soft, section
    Rubik Bold 17 tracking 0.32 in ink, both 16 px from the left edge, then a 1 px line (ink 18 %) 48 px below the top. */
inline void sectionTitle (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& kicker, const juce::String& title, bool dark = false)
{
    const int x = (int) r.getX() + 16, w = (int) r.getWidth() - 32;
    g.setColour (dark ? c::onPlate.withAlpha (0.65f) : c::inkSoft);
    g.setFont (medium (11.0f, 0.42f));
    g.drawText (kicker.toUpperCase(), x, (int) r.getY() + 12, w, 13, juce::Justification::left);
    g.setColour (dark ? c::onPlate : c::ink);
    g.setFont (bold (17.0f, 0.32f));
    g.drawText (title.toUpperCase(), x, (int) r.getY() + 26, w, 20, juce::Justification::left);
    g.setColour ((dark ? c::onPlate : c::ink).withAlpha (0.18f));
    g.fillRect ((float) x, r.getY() + 48.0f, (float) w, 1.0f);
}

inline void glassPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& kicker, const juce::String& title)
{
    panelBody (g, r, c::glass);
    sectionTitle (g, r, kicker, title);
}

/** Controls on a dark ground (header, deck) carry this property. */
inline bool isOnDark (const juce::Component& comp)
{
    for (auto* p = &comp; p != nullptr; p = p->getParentComponent())
        if (p->getProperties()["sgDark"]) return true;
    return false;
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr int kValueH = 13, kCaptionH = 15;

    LookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, c::ink);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, c::accent);
        setColour (juce::Label::textColourId, c::ink);
        setColour (juce::ComboBox::textColourId, c::ink);
        setColour (juce::ComboBox::arrowColourId, c::ink);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xfff6f7fa));
        setColour (juce::PopupMenu::textColourId, c::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c::plate);
        setColour (juce::PopupMenu::highlightedTextColourId, c::onPlate);
        setColour (juce::TextButton::buttonColourId, c::plate);
        setColour (juce::TextButton::buttonOnColourId, c::accent);
        setColour (juce::TextButton::textColourOffId, c::onPlate);
        setColour (juce::TextButton::textColourOnId, c::onAccent);
        setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xfff6f7fa));
        setColour (juce::AlertWindow::textColourId, c::ink);
        setColour (juce::AlertWindow::outlineColourId, c::plate);
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
        setColour (juce::TextEditor::textColourId, c::ink);
        setColour (juce::TextEditor::highlightColourId, c::accent);
        setColour (juce::TextEditor::outlineColourId, c::inkSoft);
        setColour (juce::TextEditor::focusedOutlineColourId, c::ink);
        setColour (juce::CaretComponent::caretColourId, c::ink);
        setColour (juce::TooltipWindow::backgroundColourId, c::plate);
        setColour (juce::TooltipWindow::textColourId, c::onPlate);
    }

    juce::Font getComboBoxFont (juce::ComboBox& b) override { return medium (b.getWidth() < 62 ? 10.5f : 12.0f); }
    juce::Font getPopupMenuFont() override { return medium (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return black (juce::jmin (12.5f, h * 0.55f), 0.04f); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
    juce::Font getAlertWindowTitleFont() override { return bold (15.0f); }
    juce::Font getAlertWindowMessageFont() override { return medium (13.0f); }
    juce::Font getAlertWindowFont() override { return medium (12.0f); }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (pixel (10.0f));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, isOnDark (s) ? c::onPlate : c::ink);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::white);
        l->setColour (juce::Label::textWhenEditingColourId, c::ink);
        l->setColour (juce::Label::outlineWhenEditingColourId, c::ink);
        return l;
    }

    // rotary knobs: knob on top, then a gap for the caption (a sibling Label), then the value
    juce::Slider::SliderLayout getSliderLayout (juce::Slider& s) override
    {
        if (! s.isRotary()) return juce::LookAndFeel_V4::getSliderLayout (s);
        juce::Slider::SliderLayout l;
        auto r = s.getLocalBounds();
        l.textBoxBounds = r.removeFromBottom (kValueH);
        r.removeFromBottom (kCaptionH);
        l.sliderBounds = r;
        return l;
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float size = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (size, size);
        const auto ctr = r.getCentre();
        const float ar = size * 0.5f - 3.0f, br = size * 0.5f - 9.0f;
        const bool dark = isOnDark (s);
        auto arc = [&] (float rad, float from, float to, juce::Colour col, float width)
        {
            juce::Path p;
            p.addCentredArc (ctr.x, ctr.y, rad, rad, 0, from, to, true);
            g.setColour (col);
            g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        const float ang = a0 + pos * (a1 - a0);
        arc (ar, a0, a1, dark ? c::trackDark : c::track, 4.0f);
        const bool bip = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        if (bip)
        {   // bipolar: the arc grows from 12 o'clock
            const float zero = a0 + (float) s.valueToProportionOfLength (0.0) * (a1 - a0);
            if (std::abs (ang - zero) > 0.01f) arc (ar, juce::jmin (zero, ang), juce::jmax (zero, ang), c::accent, 4.0f);
        }
        else if (pos > 0.002f) arc (ar, a0, ang, c::accent, 4.0f);

        juce::Path body;
        body.addEllipse (ctr.x - br, ctr.y - br, br * 2, br * 2);
        juce::DropShadow (juce::Colour (0x73000000), 6, { 0, 3 }).drawForPath (g, body);
        g.setGradientFill (juce::ColourGradient (c::knobHi, ctr.x, ctr.y - br, c::knob, ctr.x, ctr.y + br, false));
        g.fillPath (body);
        g.setColour (c::onPlate);
        g.drawLine ({ { ctr.x + br * 0.3f * std::sin (ang), ctr.y - br * 0.3f * std::cos (ang) },
                      { ctr.x + (br - 3) * std::sin (ang), ctr.y - (br - 3) * std::cos (ang) } }, 2.0f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const bool dark = isOnDark (s);
        g.setColour (dark ? c::trackDark : c::track);
        g.fillRoundedRectangle (r, 4.0f);
        const bool bip = s.getMinimum() < 0 && s.getMaximum() > 0;
        const float zero = bip ? (float) s.valueToProportionOfLength (0.0) : 0.0f;
        const float xz = r.getX() + zero * r.getWidth(), xv = pos;
        auto bar = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (xz, xv), r.getY(), juce::jmax (xz, xv), r.getBottom());
        g.setColour (bip ? (xv >= xz ? (dark ? c::accent : c::go) : c::signal) : c::accent);
        g.fillRoundedRectangle (bar, 3.0f);
        if (bip) { g.setColour (dark ? c::onPlate.withAlpha (0.6f) : c::inkSoft); g.fillRect (xz - 0.5f, r.getY() + 3, 1.0f, r.getHeight() - 6); }
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f, 2.0f).withTrimmedRight (3.0f).withTrimmedBottom (1.0f);
        const bool on = b.getToggleState();
        const bool rec = b.getProperties()["sgRec"];
        auto p = platePath (r);
        g.setColour (juce::Colour (0x59000000));
        g.fillPath (p, juce::AffineTransform::translation (3, 3));
        juce::Colour fill = b.getProperties()["sgDeck"] ? juce::Colour (0xff2c3038) : c::plate;
        if (on) fill = rec ? c::signal : c::accent;
        else if (down) fill = c::tag.darker (0.3f);
        else if (over) fill = c::tag;
        g.setColour (fill);
        g.fillPath (p);
        if (b.getClickingTogglesState() && ! b.getProperties()["sgNoLed"])
        {   // LED: state is never colour alone
            const float d = juce::jmin (8.0f, r.getHeight() * 0.34f);
            auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + r.getHeight() * 0.58f + d * 0.5f + 2, r.getCentreY() });
            g.setColour (on ? (rec ? c::onPlate : c::go) : c::onPlate.withAlpha (0.25f));
            g.fillEllipse (led);
            if (on && ! rec) { g.setColour (c::onAccent); g.drawEllipse (led, 1.2f); }
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f, 2.0f).withTrimmedRight (3.0f).withTrimmedBottom (1.0f);
        const bool on = b.getToggleState(), rec = b.getProperties()["sgRec"];
        if (b.getClickingTogglesState() && ! b.getProperties()["sgNoLed"]) r.removeFromLeft (r.getHeight() * 0.58f + 12.0f);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (on && ! rec ? c::onAccent : c::onPlate);
        g.drawText (b.getButtonText().toUpperCase(), r, juce::Justification::centred, false);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 1, box.getWidth() - 22, box.getHeight() - 2);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setBorderSize ({ 0, 3, 0, 2 });
        label.setFont (getComboBoxFont (box));
        label.setColour (juce::Label::textColourId, isOnDark (box) ? c::onPlate : c::ink);
        label.setMinimumHorizontalScale (0.7f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        const bool dark = isOnDark (box);
        g.setColour (dark ? juce::Colour (0x14ffffff) : juce::Colours::white);
        g.fillRoundedRectangle (r, 4.0f);
        const bool over = box.isMouseOver (true);
        g.setColour (dark ? c::onPlate.withAlpha (over ? 0.6f : 0.2f) : (over ? c::inkSoft : c::ink.withAlpha (0.18f)));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        juce::Path chev;
        const float ax = (float) w - 11.0f, ay = h * 0.5f;
        chev.startNewSubPath (ax - 3.5f, ay - 2.0f); chev.lineTo (ax, ay + 1.5f); chev.lineTo (ax + 3.5f, ay - 2.0f);
        g.setColour (dark ? c::onPlate : c::ink);
        g.strokePath (chev, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
        g.setColour (c::ink.withAlpha (0.25f));
        g.drawRect (0, 0, w, h, 1);
    }
};

/** Parameter-bound control. Kinds: Knob, HBar (number bar), Toggle, Menu. Knobs carry their caption between the knob and
    the value; the other kinds above the control. Empty label = no caption. */
class Control : public juce::Component
{
public:
    enum class Kind { Knob, HBar, Toggle, Menu };

    Control (juce::AudioProcessorValueTreeState& s, const juce::String& pid, const juce::String& label, Kind k)
        : kind (k), paramId (pid)
    {
        caption.setText (label.toUpperCase(), juce::dontSendNotification);
        caption.setJustificationType (k == Kind::Knob ? juce::Justification::centred : juce::Justification::centredLeft);
        caption.setFont (bold (11.0f, 0.06f));
        caption.setColour (juce::Label::textColourId, c::ink);
        caption.setBorderSize ({});
        caption.setMinimumHorizontalScale (0.75f);
        caption.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (caption);
        caption.setVisible (label.isNotEmpty() && k != Kind::Toggle);

        if (k == Kind::Toggle)
        {
            button.setClickingTogglesState (true);
            button.setButtonText (label);
            addAndMakeVisible (button);
            ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, pid, button);
        }
        else if (k == Kind::Menu)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (pid))) combo.addItemList (p->choices, 1);
            addAndMakeVisible (combo);
            ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, pid, combo);
        }
        else
        {
            using S = juce::Slider;
            if (k == Kind::Knob)
            {
                slider.setSliderStyle (S::RotaryHorizontalVerticalDrag);
                slider.setTextBoxStyle (S::TextBoxBelow, false, 80, LookAndFeel::kValueH);
            }
            else
            {
                slider.setSliderStyle (S::LinearBar);
                slider.setTextBoxStyle (S::NoTextBox, false, 0, 0);
                slider.setTooltip (label);
            }
            addAndMakeVisible (slider);
            sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, pid, slider);
            if (auto* p = s.getParameter (pid)) slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
            slider.onValueChange = [this] { repaint(); };
        }
    }

    const juce::String& getParamId() const noexcept { return paramId; }
    void setCaptionColour (juce::Colour col) { caption.setColour (juce::Label::textColourId, col); }

    void resized() override
    {
        auto r = getLocalBounds();
        if (kind == Kind::Toggle) { button.setBounds (r); return; }
        if (kind == Kind::Knob)
        {
            slider.setBounds (r);
            caption.setBounds (0, r.getBottom() - LookAndFeel::kValueH - LookAndFeel::kCaptionH, r.getWidth(), LookAndFeel::kCaptionH);
            caption.toFront (false);
            return;
        }
        if (caption.isVisible()) caption.setBounds (r.removeFromTop (16).withTrimmedBottom (2));
        if (kind == Kind::Menu) { combo.setBounds (r.withHeight (juce::jmin (r.getHeight(), 22))); return; }
        slider.setBounds (r);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (kind != Kind::HBar) return;
        const auto t = slider.getTextFromValue (slider.getValue());
        const auto f = pixel (10.0f);
        auto sb = slider.getBounds().toFloat();
        const float tw = juce::GlyphArrangement::getStringWidth (f, t) + 8.0f;
        auto patch = juce::Rectangle<float> (tw, 12.0f).withCentre (sb.getCentre());
        g.setColour (isOnDark (*this) ? c::plate.withAlpha (0.75f) : c::glass.withAlpha (1.0f));
        g.fillRoundedRectangle (patch, 2.0f);
        g.setColour (isOnDark (*this) ? c::onPlate : c::ink);
        g.setFont (f);
        g.drawText (t, sb, juce::Justification::centred);
    }

    juce::Slider slider;
    juce::TextButton button;
    juce::ComboBox combo;

private:
    Kind kind;
    juce::String paramId;
    juce::Label caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
};

/** Display well: the recessed dark-blue rectangle data is drawn in. */
inline void displayWell (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (c::display);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (c::plate);
    g.drawRoundedRectangle (r.reduced (3.0f), 4.0f, 6.0f);
    g.setColour (c::onPlate.withAlpha (0.12f));
    g.drawRoundedRectangle (r.reduced (6.5f), 3.0f, 1.0f);
    g.setColour (c::onPlate.withAlpha (0.18f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

/** Level bar for the input or the output, or (yellow, from the left) the peak reduction. */
class Meter : public juce::Component
{
public:
    enum Kind { Level, Reduction };
    Meter (std::atomic<float>& source, Kind k) : value (source), kind (k) {}
    void tick()
    {
        const float v = value.exchange (0.0f, std::memory_order_relaxed);   // the engine keeps the peak since the last look
        shown = juce::jmax (v, shown * (kind == Level ? 0.86f : 0.9f));
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (c::trackDark);
        g.fillRoundedRectangle (r, 2.0f);
        float x;
        if (kind == Level)
        {
            const float db = 20.0f * std::log10 (shown + 1.0e-6f);
            x = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
            g.setColour (db > 0.0f ? c::signal : c::go);
        }
        else
        {
            x = juce::jlimit (0.0f, 1.0f, shown / 24.0f);
            g.setColour (c::accent);
        }
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * x), 2.0f);
    }
    float shownValue() const noexcept { return shown; }
private:
    std::atomic<float>& value;
    Kind kind;
    float shown = 0.0f;
};

// ---------------------------------------------------------------- the spectrum

/** The detector at work: the input spectrum (white, filled), what leaves (white line), the reduction hanging from the top
    edge (yellow), the sensitivity curve with its six nodes, and the working range. Drag a node for frequency and
    sensitivity, the wheel over it for its width, double-click to switch it. Drag the LO / HI tags for the range. */
class SpectrumView : public juce::Component
{
public:
    std::function<void (int)> onSelectBand;

    explicit SpectrumView (SmoothProcessor& p) : proc (p)
    {
        shownIn.fill (-120.0f); shownGr.fill (0.0f);
        setRepaintsOnMouseActivity (true);
    }

    void setSelected (int b) { selected = b; repaint(); }

    void tick()
    {
        auto& u = proc.engine->ui;
        const int frames = u.frames.load (std::memory_order_relaxed);
        // with large host buffers frames arrive in bursts: hold the picture a little before letting it fall
        stale = frames != lastFrames ? 0 : stale + 1;
        lastFrames = frames;
        const bool live = stale < 6;
        for (int i = 0; i < kDisplayPoints; ++i)
        {
            const float in = live ? u.in[(size_t) i].load (std::memory_order_relaxed) : -140.0f;
            const float gr = live ? u.gr[(size_t) i].load (std::memory_order_relaxed) : 0.0f;
            auto& si = shownIn[(size_t) i];
            si = in > si ? in : juce::jmax (-140.0f, si - 1.6f);
            auto& sg = shownGr[(size_t) i];
            sg = gr > sg ? gr : sg * 0.82f + gr * 0.18f;
        }
        repaint();
    }

    // ---- geometry
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (12.0f, 12.0f).withTrimmedBottom (14.0f); }
    float xOf (float f) const { auto p = plot(); return p.getX() + std::log (f / kDisplayLo) / std::log (kDisplayHi / kDisplayLo) * p.getWidth(); }
    float fOf (float x) const { auto p = plot(); return kDisplayLo * std::pow (kDisplayHi / kDisplayLo, juce::jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth())); }
    float yOfLevel (float db) const { auto p = plot(); return p.getY() + p.getHeight() * juce::jlimit (0.0f, 1.0f, (6.0f - db) / 102.0f); }
    float yOfCurve (float db) const { auto p = plot(); return p.getCentreY() - db / 12.0f * p.getHeight() * 0.42f; }
    float curveOfY (float y) const { auto p = plot(); return juce::jlimit (-12.0f, 12.0f, (p.getCentreY() - y) / (p.getHeight() * 0.42f) * 12.0f); }
    float yOfReduction (float db) const { auto p = plot(); return p.getY() + juce::jlimit (0.0f, 1.0f, db / 24.0f) * p.getHeight() * 0.5f; }

    juce::Point<float> nodePos (int b) const
    {
        return { xOf (proc.value (id::band (b, "freq"))), yOfCurve (proc.value (id::band (b, "gain"))) };
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        displayWell (g, r);
        const auto p = plot();
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (r.reduced (7.0f).toNearestInt());

        // grid: decades bright, the rest faint; labels along the bottom
        g.setFont (pixel (10.0f));
        for (float f : { 30.f, 40.f, 50.f, 60.f, 70.f, 80.f, 90.f, 100.f, 200.f, 300.f, 400.f, 500.f, 600.f, 700.f, 800.f, 900.f, 1000.f,
                         2000.f, 3000.f, 4000.f, 5000.f, 6000.f, 7000.f, 8000.f, 9000.f, 10000.f })
        {
            const bool major = exactlyEqual (f, 100.f) || exactlyEqual (f, 1000.f) || exactlyEqual (f, 10000.f);
            g.setColour (c::onPlate.withAlpha (major ? 0.16f : 0.06f));
            g.fillRect (xOf (f), p.getY(), 1.0f, p.getHeight());
        }
        g.setColour (c::onPlate.withAlpha (0.55f));
        for (auto [f, t] : { std::pair<float, const char*> { 50.f, "50" }, { 100.f, "100" }, { 200.f, "200" }, { 500.f, "500" }, { 1000.f, "1K" },
                             { 2000.f, "2K" }, { 5000.f, "5K" }, { 10000.f, "10K" } })
            g.drawText (t, (int) xOf (f) - 20, (int) p.getBottom() + 2, 40, 12, juce::Justification::centred);
        g.setColour (c::onPlate.withAlpha (0.10f));
        for (float db : { -6.0f, 6.0f }) g.fillRect (p.getX(), yOfCurve (db), p.getWidth(), 1.0f);
        g.setColour (c::onPlate.withAlpha (0.22f));
        g.fillRect (p.getX(), yOfCurve (0.0f), p.getWidth(), 1.0f);

        // working range: outside is shaded and hatched
        const float lo = proc.value (id::lo), hi = proc.value (id::hi);
        auto shade = [&] (float x0, float x1) {
            if (x1 <= x0) return;
            auto band = juce::Rectangle<float>::leftTopRightBottom (x0, r.getY(), x1, r.getBottom());
            g.setColour (juce::Colour (0x66000000));
            g.fillRect (band);
            juce::Graphics::ScopedSaveState s2 (g);
            g.reduceClipRegion (band.toNearestInt());
            g.setColour (c::onPlate.withAlpha (0.05f));
            for (float x = x0 - r.getHeight(); x < x1; x += 9.0f) g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 2.0f);
        };
        shade (r.getX(), xOf (lo));
        shade (xOf (hi), r.getRight());

        // input spectrum and what leaves
        juce::Path in, out;
        for (int i = 0; i < kDisplayPoints; ++i)
        {
            const float x = xOf (displayFreq ((float) i));
            const float yi = yOfLevel (shownIn[(size_t) i]), yo = yOfLevel (shownIn[(size_t) i] - shownGr[(size_t) i]);
            if (i == 0) { in.startNewSubPath (x, yi); out.startNewSubPath (x, yo); } else { in.lineTo (x, yi); out.lineTo (x, yo); }
        }
        auto inFill = in;
        inFill.lineTo (p.getRight(), p.getBottom() + 14.0f); inFill.lineTo (p.getX(), p.getBottom() + 14.0f); inFill.closeSubPath();
        g.setColour (c::onPlate.withAlpha (0.13f));
        g.fillPath (inFill);
        g.setColour (c::onPlate.withAlpha (0.35f));
        g.strokePath (in, juce::PathStrokeType (1.0f));
        g.setColour (c::onPlate);
        g.strokePath (out, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // reduction, hanging from the top edge
        juce::Path edge;
        for (int i = 0; i < kDisplayPoints; ++i)
        {
            const float x = xOf (displayFreq ((float) i)), y = yOfReduction (shownGr[(size_t) i]);
            if (i == 0) edge.startNewSubPath (x, y); else edge.lineTo (x, y);
        }
        auto red = edge;
        red.lineTo (p.getRight(), p.getY()); red.lineTo (p.getX(), p.getY()); red.closeSubPath();
        g.setColour (c::accent.withAlpha (0.35f));
        g.fillPath (red);
        g.setColour (c::accent);
        g.strokePath (edge, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // sensitivity curve
        Bands bands;
        for (int b = 0; b < kNumBands; ++b)
        {
            auto& bd = bands[(size_t) b];
            bd.on = proc.value (id::band (b, "on")) > 0.5f; bd.freq = proc.value (id::band (b, "freq"));
            bd.gain = proc.value (id::band (b, "gain")); bd.q = proc.value (id::band (b, "q"));
            bd.type = b == 0 ? btLowShelf : (b == kNumBands - 1 ? btHighShelf : btBell);
        }
        juce::Path curve, selShape;
        for (int i = 0; p.getX() + 2.0f * (float) i <= p.getRight(); ++i)
        {
            const float x = p.getX() + 2.0f * (float) i, f = fOf (x);
            const float y = yOfCurve (juce::jlimit (-12.0f, 12.0f, curveDb (bands, f)));
            const float ys = yOfCurve (juce::jlimit (-12.0f, 12.0f, bandDb (bands[(size_t) selected], f)));
            if (i == 0) { curve.startNewSubPath (x, y); selShape.startNewSubPath (x, ys); }
            else        { curve.lineTo (x, y); selShape.lineTo (x, ys); }
        }
        auto selFill = selShape;
        selFill.lineTo (p.getRight(), yOfCurve (0.0f)); selFill.lineTo (p.getX(), yOfCurve (0.0f)); selFill.closeSubPath();
        g.setColour (c::slab.withAlpha (0.22f));
        g.fillPath (selFill);
        g.setColour (c::slab);
        g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // nodes: numbered discs; the selected one is yellow, a switched-off one hollow
        for (int b = 0; b < kNumBands; ++b)
        {
            const auto pt = nodePos (b);
            const bool on = bands[(size_t) b].on, sel = b == selected, over = b == hover;
            const float rad = sel ? 11.0f : 9.5f;
            auto disc = juce::Rectangle<float> (rad * 2, rad * 2).withCentre (pt);
            g.setColour (juce::Colour (0x66000000));
            g.fillEllipse (disc.translated (0.0f, 2.0f));
            g.setColour (sel ? c::accent : (on ? c::slab : c::display));
            g.fillEllipse (disc);
            g.setColour (sel ? c::onAccent : (over ? c::onPlate : c::plate));
            g.drawEllipse (disc, sel || over ? 2.0f : 1.5f);
            g.setFont (pixel (10.0f));
            g.setColour (sel ? c::onAccent : (on ? c::ink : c::onPlate.withAlpha (0.6f)));
            g.drawText (bandShort (b), disc.translated (0.5f, 0.5f), juce::Justification::centred);
        }

        // range tags
        for (int k = 0; k < 2; ++k)
        {
            const float x = xOf (k == 0 ? lo : hi);
            g.setColour (c::onPlate.withAlpha (dragRange == k || hoverRange == k ? 0.9f : 0.45f));
            g.fillRect (x - 0.5f, p.getY(), 1.0f, p.getHeight());
            auto tagR = juce::Rectangle<float> (34.0f, 16.0f).withCentre ({ x + (k == 0 ? 15.0f : -15.0f), p.getBottom() - 12.0f });
            g.setColour (c::plate);
            g.fillPath (platePath (tagR));
            g.setColour (c::onPlate);
            g.drawText (k == 0 ? "LO" : "HI", tagR.translated (1.0f, 0.5f), juce::Justification::centred);
        }

        // corners
        g.setFont (pixel (10.0f));
        g.setColour (c::onPlate.withAlpha (0.5f));
        g.drawText ("IN", (int) p.getX() + 6, (int) p.getY() + 2, 30, 12, juce::Justification::left);
        g.setColour (c::onPlate);
        g.drawText ("OUT", (int) p.getX() + 28, (int) p.getY() + 2, 40, 12, juce::Justification::left);
        g.setColour (c::accent);
        g.drawText ("REDUCTION", (int) p.getX() + 66, (int) p.getY() + 2, 120, 12, juce::Justification::left);
        g.setColour (c::slab);
        g.drawText ("SENSITIVITY", (int) p.getX() + 146, (int) p.getY() + 2, 120, 12, juce::Justification::left);
        g.setColour (c::onPlate.withAlpha (0.55f));
        g.drawText ("+12", (int) p.getRight() - 40, (int) yOfCurve (12.0f) - 6, 36, 12, juce::Justification::right);
        g.drawText ("0", (int) p.getRight() - 40, (int) yOfCurve (0.0f) - 13, 36, 12, juce::Justification::right);
        g.drawText ("-12", (int) p.getRight() - 40, (int) yOfCurve (-12.0f) - 12, 36, 12, juce::Justification::right);
        if (hover >= 0 || drag >= 0)
        {
            const int b = drag >= 0 ? drag : hover;
            const auto pt = nodePos (b);
            const auto txt = juce::String (bandName (b)).toUpperCase() + "  " + freqText (proc.value (id::band (b, "freq")))
                           + "  " + juce::String (proc.value (id::band (b, "gain")), 1) + " DB  Q " + juce::String (proc.value (id::band (b, "q")), 2);
            const float tw = juce::GlyphArrangement::getStringWidth (pixel (10.0f), txt) + 14.0f;
            auto box = juce::Rectangle<float> (tw, 18.0f).withCentre ({ juce::jlimit (p.getX() + tw * 0.5f, p.getRight() - tw * 0.5f, pt.x), pt.y - 26.0f });
            if (box.getY() < p.getY()) box.setY (pt.y + 16.0f);
            g.setColour (c::plate.withAlpha (0.9f));
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (c::onPlate);
            g.drawText (txt, box, juce::Justification::centred);
        }
    }

    static juce::String freqText (float f)
    {
        return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " KHZ" : juce::String (juce::roundToInt (f)) + " HZ";
    }

    // ---- mouse
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = nodeAt (e.position), hr = h < 0 ? rangeAt (e.position) : -1;
        if (h != hover || hr != hoverRange) { hover = h; hoverRange = hr; repaint(); }
        setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : (hr >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor));
    }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; hoverRange = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = nodeAt (e.position);
        dragRange = drag < 0 ? rangeAt (e.position) : -1;
        if (drag >= 0)
        {
            selected = drag;
            if (onSelectBand) onSelectBand (drag);
            begin ({ id::band (drag, "freq"), id::band (drag, "gain") });
        }
        else if (dragRange >= 0) begin ({ dragRange == 0 ? id::lo : id::hi });
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag >= 0)
        {
            set (id::band (drag, "freq"), fOf (e.position.x));
            if (! e.mods.isShiftDown()) set (id::band (drag, "gain"), curveOfY (e.position.y));
        }
        else if (dragRange >= 0)
        {
            const float f = fOf (e.position.x);
            if (dragRange == 0) set (id::lo, juce::jmin (f, proc.value (id::hi)));
            else set (id::hi, juce::jmax (f, proc.value (id::lo)));
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { end(); drag = -1; dragRange = -1; repaint(); }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int b = nodeAt (e.position);
        if (b < 0) return;
        const auto pid = id::band (b, "on");
        begin ({ pid });
        set (pid, proc.value (pid) > 0.5f ? 0.0f : 1.0f);
        end();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        int b = nodeAt (e.position);
        if (b < 0) b = selected;
        const auto pid = id::band (b, "q");
        begin ({ pid });
        set (pid, juce::jlimit (0.2f, 6.0f, proc.value (pid) * std::pow (1.25f, w.deltaY * 4.0f)));
        end();
    }

    int nodeAt (juce::Point<float> pt) const
    {
        int best = -1; float bestD = 14.0f;
        for (int b = 0; b < kNumBands; ++b)
        {
            const float d = nodePos (b).getDistanceFrom (pt);
            if (d < bestD || (d <= bestD + 0.01f && b == selected)) { bestD = d; best = b; }
        }
        return best;
    }

    int rangeAt (juce::Point<float> pt) const
    {
        const float dl = std::abs (pt.x - xOf (proc.value (id::lo))), dh = std::abs (pt.x - xOf (proc.value (id::hi)));
        if (juce::jmin (dl, dh) > 7.0f) return -1;
        return dl <= dh ? 0 : 1;
    }

private:
    void begin (std::initializer_list<juce::String> ids)
    {
        end();
        for (auto& i : ids) if (auto* p = proc.state.getParameter (i)) { p->beginChangeGesture(); gesture.add (p); }
    }
    void end()
    {
        for (auto* p : gesture) p->endChangeGesture();
        gesture.clear();
    }
    void set (const juce::String& pid, float plain)
    {
        if (auto* p = proc.state.getParameter (pid)) p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }

    SmoothProcessor& proc;
    std::array<float, kDisplayPoints> shownIn {}, shownGr {};
    juce::Array<juce::RangedAudioParameter*> gesture;
    int selected = 2, hover = -1, hoverRange = -1, drag = -1, dragRange = -1, lastFrames = -1, stale = 0;
};
} // namespace smooth::ui
