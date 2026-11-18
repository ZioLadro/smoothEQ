#pragma once
#include "SmoothProcessor.h"
#include "SmoothUI.h"

/** The whole interface, laid out on a fixed 1280 x 800 grid. SmoothEditor scales it to the window. */
class SmoothView : public juce::Component, private juce::Timer
{
public:
    static constexpr int kW = 1280, kH = 800;

    explicit SmoothView (SmoothProcessor& p)
        : proc (p), spectrum (p),
          inMeter (p.engine->ui.inPeak, smooth::ui::Meter::Level), outMeter (p.engine->ui.outPeak, smooth::ui::Meter::Level),
          grMeter (p.engine->ui.grPeak, smooth::ui::Meter::Reduction)
    {
        using namespace smooth;
        using namespace smooth::ui;
        using K = Control::Kind;
        backdrop.draw = [this] (juce::Graphics& g) { paintBackdrop (g); };
        backdrop.setInterceptsMouseClicks (false, false);
        backdrop.setOpaque (true);
        backdrop.setBufferedToImage (true);
        addAndMakeVisible (backdrop);
        setLookAndFeel (&lnf);

        auto add = [&] (const char* pid, const char* label, K kind, juce::Rectangle<int> r, bool dark = false) {
            auto* ctl = controls.add (new Control (p.state, pid, label, kind));
            if (dark) { ctl->getProperties().set ("sgDark", true); ctl->setCaptionColour (c::onPlate); }
            addAndMakeVisible (ctl);
            ctl->setBounds (r);
        };
        // detector
        add (id::depth, "Depth", K::Knob, { 32, 600, 100, 106 });
        add (id::sharp, "Sharpness", K::Knob, { 134, 600, 100, 106 });
        add (id::select, "Selectivity", K::Knob, { 236, 600, 100, 106 });
        add (id::attack, "Attack", K::Knob, { 338, 600, 100, 106 });
        add (id::release, "Release", K::Knob, { 440, 600, 100, 106 });
        add (id::mode, "MODE", K::Menu, { 32, 718, 140, 40 });
        add (id::res, "RESOLUTION", K::Menu, { 184, 718, 140, 40 });
        // range, under the band
        add (id::lo, "LOW CUT", K::HBar, { 700, 718, 96, 38 });
        add (id::hi, "HIGH CUT", K::HBar, { 804, 718, 88, 38 });
        // output deck
        add (id::link, "Link", K::Knob, { 940, 600, 100, 106 }, true);
        add (id::mix, "Mix", K::Knob, { 1044, 600, 100, 106 }, true);
        add (id::trim, "Output", K::Knob, { 1148, 600, 100, 106 }, true);
        add (id::stereo, "STEREO", K::Menu, { 940, 718, 120, 40 }, true);
        add (id::delta, "DELTA", K::Toggle, { 800, 20, 110, 30 });

        addAndMakeVisible (spectrum);
        spectrum.onSelectBand = [this] (int b) { selectBand (b); };
        for (auto* m : { &inMeter, &outMeter, &grMeter }) addAndMakeVisible (m);

        for (int b = 0; b < kNumBands; ++b)
        {
            auto* chip = chips.add (new juce::TextButton (bandShort (b)));
            chip->getProperties().set ("sgNoLed", true);
            chip->setClickingTogglesState (false);
            chip->onClick = [this, b] { selectBand (b); };
            addAndMakeVisible (chip);
        }

        for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &delBtn }) addAndMakeVisible (b);
        prevBtn.setButtonText ("<"); nextBtn.setButtonText (">");
        saveBtn.setButtonText ("SAVE"); delBtn.setButtonText ("DEL");
        addAndMakeVisible (presetBox);
        presetBox.getProperties().set ("sgDark", true);
        refreshPresets();
        presetBox.onChange = [this] {
            const int i = presetBox.getSelectedItemIndex();
            if (i >= 0 && i != proc.presets.getCurrent()) proc.presets.load (i);
        };
        prevBtn.onClick = [this] { proc.presets.prev(); refreshPresets(); };
        nextBtn.onClick = [this] { proc.presets.next(); refreshPresets(); };
        delBtn.onClick = [this] { proc.presets.removeCurrent(); refreshPresets(); };
        saveBtn.onClick = [this] {
            auto* w = new juce::AlertWindow ("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon, this);
            w->addTextEditor ("name", proc.presets.getCurrent() >= proc.presets.numFactory() ? proc.presets.getNames()[proc.presets.getCurrent()] : "My preset");
            w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r) {
                if (r == 1) { proc.presets.save (w->getTextEditorContents ("name")); refreshPresets(); }
            }), true);
        };

        sendLookAndFeelChange();
        setSize (kW, kH);
        selectBand (proc.uiBand.load());
        startTimerHz (30);
    }
    ~SmoothView() override { setLookAndFeel (nullptr); }

    void selectBand (int b)
    {
        using namespace smooth;
        using namespace smooth::ui;
        using K = Control::Kind;
        band = juce::jlimit (0, kNumBands - 1, b);
        proc.uiBand = band;
        spectrum.setSelected (band);
        for (int i = 0; i < chips.size(); ++i) chips[i]->setToggleState (i == band, juce::dontSendNotification);
        bandControls.clear();
        auto add = [&] (const juce::String& pid, const char* label, K kind, juce::Rectangle<int> r) {
            auto* ctl = bandControls.add (new Control (proc.state, pid, label, kind));
            addAndMakeVisible (ctl);
            ctl->sendLookAndFeelChange();
            ctl->setBounds (r);
        };
        add (id::band (band, "freq"), "Freq", K::Knob, { 588, 600, 100, 106 });
        add (id::band (band, "gain"), "Sensitivity", K::Knob, { 692, 600, 100, 106 });
        add (id::band (band, "q"), band == 0 || band == kNumBands - 1 ? "Slope" : "Q", K::Knob, { 796, 600, 100, 106 });
        add (id::band (band, "on"), "ON", K::Toggle, { 584, 722, 96, 30 });
        backdrop.repaint();
    }

    void tickForSnapshot (int n = 8) { for (int i = 0; i < n; ++i) timerCallback(); }

    void paintBackdrop (juce::Graphics& g)
    {
        using namespace smooth;
        using namespace smooth::ui;
        const float W = (float) getWidth(), H = (float) getHeight();
        // ground: violet, a mint slab with a plate edge, a light checkerboard over both, silk ribbons in the gaps
        g.fillAll (c::ground);
        juce::Path slabPath;
        slabPath.addQuadrilateral (700, kHeaderH, W, kHeaderH, W, H, 1020, H);
        g.setColour (c::slab);
        g.fillPath (slabPath);
        g.setColour (c::checker);
        for (int y = kHeaderH; y < getHeight(); y += 32)
            for (int x = (((y - kHeaderH) / 32) % 2) * 32; x < getWidth(); x += 64)
                g.fillRect (x, y, 32, 32);
        g.setColour (c::plate);
        g.drawLine (700, kHeaderH, 1020, H, 6.0f);
        ribbon (g, -60.0f, 1340.0f, 536.0f, 22.0f, 640.0f, 0.4f, 46.0f);
        ribbon (g, -60.0f, 1340.0f, 792.0f, 14.0f, 520.0f, 2.1f, 36.0f);

        // header
        g.setColour (c::groundAlt);
        g.fillRect (0.0f, 0.0f, W, (float) kHeaderH);
        silkBand (g, { 0.0f, (float) kHeaderH - 6.0f, W, 6.0f });
        g.setFont (black (34.0f, -0.01f));
        const juce::String name ("SMOOTHEQ");
        g.setColour (c::go);      g.drawText (name, 22, 15, 280, 40, juce::Justification::centredLeft);
        g.setColour (c::signal);  g.drawText (name, 19, 12, 280, 40, juce::Justification::centredLeft);
        g.setColour (c::onPlate); g.drawText (name, 16, 9, 280, 40, juce::Justification::centredLeft);
        taggedPlate (g, { 330, 17, 300, 34 }, c::tag);

        // console: latency and the reduction right now
        auto console = juce::Rectangle<float> (kConsoleX, 12.0f, W - kMargin - kConsoleX, 46.0f);
        g.setColour (c::plate);
        g.fillRoundedRectangle (console, 4.0f);
        g.setColour (c::onPlate.withAlpha (0.12f));
        g.drawRoundedRectangle (console.reduced (0.5f), 4.0f, 1.0f);
        g.fillRect (kConsoleX + 112.0f, 20.0f, 1.0f, 30.0f);
        g.setColour (c::onPlate.withAlpha (0.8f));
        g.setFont (bold (11.0f, 0.06f));
        g.drawText ("LATENCY", (int) kConsoleX + 12, 16, 96, 14, juce::Justification::left);
        g.drawText ("REDUCTION", (int) kConsoleX + 126, 16, 96, 14, juce::Justification::left);
        g.setFont (pixel (10.0f));
        const double sr = proc.uiSampleRate.load();
        const int lat = proc.engine->latency();
        g.drawText (juce::String (lat) + " SMP", (int) kConsoleX + 12, 38, 96, 12, juce::Justification::left);
        g.drawText ("PEAK DB", (int) kConsoleX + 126, 38, 80, 12, juce::Justification::left);
        g.setColour (c::accent);
        g.setFont (pixel (20.0f));
        g.drawText (juce::String (juce::roundToInt (1000.0 * lat / sr)) + " MS", (int) kConsoleX + 30, 23, 76, 24, juce::Justification::right);
        g.drawText (shownGr < 0.05f ? juce::String ("0.0") : "-" + juce::String (shownGr, 1), (int) (W - kMargin) - 100, 23, 88, 24, juce::Justification::right);

        // spectrum panel
        glassPanel (g, { 16, 88, 1248, 440 }, "Spectral detector", "Resonances");
        g.setColour (c::inkSoft);
        g.setFont (medium (11.5f));
        g.drawText ("Drag a node: frequency and sensitivity (Shift: frequency only). Wheel: width. Double-click: on / off. Drag LO and HI for the range.",
                    360, 112, 888, 16, juce::Justification::right);

        // detector, band, output
        glassPanel (g, { 16, 544, 540, 240 }, "Detector", "Resonance");
        g.setColour (c::inkSoft);
        g.setFont (pixel (10.0f));
        g.drawText ("FFT " + juce::String (proc.engine->fftSize()) + "  " + juce::String (proc.engine->sampleRate() / proc.engine->fftSize(), 1) + " HZ/BIN",
                    340, 740, 200, 12, juce::Justification::right);
        glassPanel (g, { 572, 544, 336, 240 }, "Curve", "Band");

        const auto deck = juce::Rectangle<float> (924.0f, 544.0f, W - 924.0f - kMargin, 240.0f);
        {
            juce::Path body;
            body.addRoundedRectangle (deck, 10.0f);
            juce::DropShadow (c::panelShadow, 28, { 0, 12 }).drawForPath (g, body);
            g.setColour (c::modGround);
            g.fillPath (body);
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (body);
            silkBand (g, deck.withHeight (8.0f));
        }
        sectionTitle (g, deck.withTrimmedTop (4.0f), "Output", "Mix", true);
        g.setColour (c::onPlate);
        g.setFont (bold (11.0f, 0.06f));
        g.drawText ("IN", 1076, 716, 40, 14, juce::Justification::left);
        g.drawText ("OUT", 1076, 734, 40, 14, juce::Justification::left);
        g.drawText ("GR", 1076, 752, 40, 14, juce::Justification::left);
    }

    void resized() override
    {
        backdrop.setBounds (getLocalBounds());
        prevBtn.setBounds (288, 20, 36, 30);
        presetBox.setBounds (356, 23, 244, 22);
        nextBtn.setBounds (634, 20, 36, 30);
        saveBtn.setBounds (676, 20, 62, 30);
        delBtn.setBounds (740, 20, 54, 30);
        spectrum.setBounds (32, 146, 1216, 366);
        inMeter.setBounds (1112, 719, 136, 8);
        outMeter.setBounds (1112, 737, 136, 8);
        grMeter.setBounds (1112, 755, 136, 8);
        for (int i = 0; i < chips.size(); ++i) chips[i]->setBounds (700 + i * 33, 558, 35, 26);
    }

private:
    static constexpr int kHeaderH = 72, kMargin = 16;
    static constexpr float kConsoleX = 1020.0f;

    void timerCallback() override
    {
        spectrum.tick();
        inMeter.tick(); outMeter.tick(); grMeter.tick();
        if (++slowTick >= 4)
        {
            slowTick = 0;
            const float gr = std::round (grMeter.shownValue() * 10.0f) / 10.0f;
            const int lat = proc.engine->latency();
            if (! juce::exactlyEqual (gr, shownGr) || lat != shownLatency)
            {
                const bool latChanged = lat != shownLatency;
                shownGr = gr; shownLatency = lat;
                backdrop.repaint ((int) kConsoleX, 8, kW - (int) kConsoleX, 56);
                if (latChanged) backdrop.repaint (16, 544, 540, 240);
            }
            if (presetBox.getSelectedItemIndex() != proc.presets.getCurrent()) refreshPresets();
        }
    }

    void refreshPresets()
    {
        presetBox.clear (juce::dontSendNotification);
        presetBox.addItemList (proc.presets.getNames(), 1);
        presetBox.setSelectedItemIndex (proc.presets.getCurrent(), juce::dontSendNotification);
    }

    /** A child that draws through its owner: the static artwork, cached so animated parts never redraw it. */
    struct Layer : juce::Component
    {
        std::function<void (juce::Graphics&)> draw;
        void paint (juce::Graphics& g) override { draw (g); }
    };
    Layer backdrop;

    SmoothProcessor& proc;
    smooth::ui::LookAndFeel lnf;
    smooth::ui::SpectrumView spectrum;
    smooth::ui::Meter inMeter, outMeter, grMeter;
    juce::OwnedArray<smooth::ui::Control> controls, bandControls;
    juce::OwnedArray<juce::TextButton> chips;
    juce::TextButton prevBtn, nextBtn, saveBtn, delBtn;
    juce::ComboBox presetBox;
    juce::TooltipWindow tooltips { this, 600 };
    int band = 2, slowTick = 0, shownLatency = -1;
    float shownGr = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmoothView)
};

/** Resizable window with the proportions locked (60 % to 200 %): the view is drawn as vectors at the window's own scale. */
class SmoothEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr int kW = SmoothView::kW, kH = SmoothView::kH;
    static constexpr double kMinScale = 0.6, kMaxScale = 2.0;

    explicit SmoothEditor (SmoothProcessor& p) : AudioProcessorEditor (p), proc (p), ui (p)
    {
        addAndMakeVisible (ui);
        ui.setBounds (0, 0, kW, kH);
        setResizable (true, true);
        setResizeLimits (juce::roundToInt (kW * kMinScale), juce::roundToInt (kH * kMinScale),
                         juce::roundToInt (kW * kMaxScale), juce::roundToInt (kH * kMaxScale));
        getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
        const double s = juce::jlimit (kMinScale, kMaxScale, (double) p.uiScale.load());
        setSize (juce::roundToInt (kW * s), juce::roundToInt (kH * s));
    }

    void resized() override
    {
        const float s = (float) getWidth() / (float) kW;
        ui.setTransform (juce::AffineTransform::scale (s));
        proc.uiScale = s;
    }

    void paint (juce::Graphics& g) override { g.fillAll (smooth::ui::c::ground); }

    void selectBand (int b) { ui.selectBand (b); }
    void tickForSnapshot (int n = 8) { ui.tickForSnapshot (n); }

private:
    SmoothProcessor& proc;
    SmoothView ui;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmoothEditor)
};
