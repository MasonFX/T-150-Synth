#include "PluginEditor.h"

namespace
{
    // Palette: blacked-out hardware, bone-white legends, white accents
    const juce::Colour bone        (0xffd9d2c3);
    const juce::Colour boneDim     (0xff9c968a);
    const juce::Colour accent      (0xfff2f0eb);
    const juce::Colour accentDim   (0xffbdbab4);
    const juce::Colour tolexBase   (0xff0a0a0a);
    const juce::Colour lampWhite   (0xfffff6e8);

    constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.2f;
    constexpr float kEndAngle   = juce::MathConstants<float>::pi * 2.8f;

    juce::Font labelFont (float size)
    {
        return juce::Font (juce::FontOptions (size, juce::Font::bold)).withExtraKerningFactor (0.18f);
    }

}

//==============================================================================
// Look and feel
AmpLookAndFeel::AmpLookAndFeel()
{
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xf00c0c0c));
    setColour (juce::BubbleComponent::outlineColourId,    accent);
    setColour (juce::TooltipWindow::textColourId,         bone);
}

void AmpLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider&)
{
    using namespace juce;
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r  = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX(), cy = bounds.getCentreY();

    // Shadow
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillEllipse (cx - r + 1.5f, cy - r + 3.5f, r * 2.0f, r * 2.0f);

    // Steel edge ring + knurled black skirt
    g.setGradientFill (ColourGradient (Colour (0xff6a6a6c), cx, cy - r, Colour (0xff121213), cx, cy + r, false));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    const float rs = r - 1.6f;
    g.setGradientFill (ColourGradient (Colour (0xff222223), cx, cy - rs, Colour (0xff030303), cx, cy + rs, false));
    g.fillEllipse (cx - rs, cy - rs, rs * 2.0f, rs * 2.0f);
    g.setColour (Colours::white.withAlpha (0.06f));
    for (int i = 0; i < 60; ++i)
    {
        const float a = MathConstants<float>::twoPi * (float) i / 60.0f;
        g.drawLine (cx + std::cos (a) * rs * 0.78f, cy + std::sin (a) * rs * 0.78f,
                    cx + std::cos (a) * rs * 0.98f, cy + std::sin (a) * rs * 0.98f, 1.0f);
    }

    // Cap
    const float rc = r * 0.68f;
    g.setGradientFill (ColourGradient (Colour (0xff1c1c1d), cx, cy - rc, Colour (0xff000000), cx, cy + rc, false));
    g.fillEllipse (cx - rc, cy - rc, rc * 2.0f, rc * 2.0f);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawEllipse (cx - rc, cy - rc, rc * 2.0f, rc * 2.0f, 0.8f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f), cx, cy - rc,
                                       Colours::transparentWhite, cx, cy, false));
    g.fillEllipse (cx - rc * 0.8f, cy - rc * 0.95f, rc * 1.6f, rc * 1.0f);

    // Pointer: white with a faint glow
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float sx = std::sin (angle), sy = -std::cos (angle);
    Path p;
    p.startNewSubPath (cx + sx * r * 0.18f, cy + sy * r * 0.18f);
    p.lineTo          (cx + sx * r * 0.97f, cy + sy * r * 0.97f);
    g.setColour (accent.withAlpha (0.2f));
    g.strokePath (p, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colour (0xfffafafa));
    g.strokePath (p, PathStrokeType (2.6f, PathStrokeType::curved, PathStrokeType::rounded));

    // Small dark-steel centre bolt
    const float ri = r * 0.17f;
    g.setGradientFill (ColourGradient (Colour (0xff9a9a9c), cx - ri, cy - ri, Colour (0xff2a2a2b), cx + ri, cy + ri, false));
    g.fillEllipse (cx - ri, cy - ri, ri * 2.0f, ri * 2.0f);
}

void AmpLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    using namespace juce;
    const auto area = b.getLocalBounds().toFloat();
    const float cx = area.getCentreX(), cy = area.getCentreY();
    const float nut = jmin (area.getWidth(), area.getHeight()) * 0.22f;

    // Hex nut (dark steel)
    Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a = MathConstants<float>::pi / 6.0f + MathConstants<float>::twoPi * (float) i / 6.0f;
        const Point<float> pt (cx + std::cos (a) * nut * 1.25f, cy + std::sin (a) * nut * 1.25f);
        if (i == 0) hex.startNewSubPath (pt); else hex.lineTo (pt);
    }
    hex.closeSubPath();
    g.setGradientFill (ColourGradient (Colour (0xffa8a8aa), cx, cy - nut, Colour (0xff2c2c2e), cx, cy + nut, false));
    g.fillPath (hex);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.strokePath (hex, PathStrokeType (1.0f));

    // Bat-handle lever: up when on, down when off
    const bool on = b.getToggleState();
    const float len = area.getHeight() * 0.42f;
    const float tipY = on ? cy - len : cy + len;
    Path lever;
    lever.startNewSubPath (cx, cy);
    lever.lineTo (cx, tipY);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.strokePath (lever, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded),
                  AffineTransform::translation (1.5f, 2.0f));
    g.setGradientFill (ColourGradient (Colour (0xff5a5a5c), cx - 4, cy, Colour (0xffe6e6e8), cx + 1, cy, false));
    g.strokePath (lever, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colour (0xffd6d6d8));
    g.fillEllipse (cx - 4.5f, tipY - 4.5f, 9.0f, 9.0f);

    g.setColour (Colour (0xff050505));
    g.fillEllipse (cx - nut * 0.45f, cy - nut * 0.45f, nut * 0.9f, nut * 0.9f);
}

void AmpLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                           bool highlighted, bool down)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRect (r.translated (0.0f, 1.5f));
    const Colour top = down ? Colour (0xff0a0a0a) : (highlighted ? Colour (0xff2a2a2b) : Colour (0xff1a1a1b));
    g.setGradientFill (ColourGradient (top, 0, r.getY(), Colour (0xff050505), 0, r.getBottom(), false));
    g.fillRect (r);
    g.setColour (highlighted ? accent : accentDim);
    g.drawRect (r, 1.2f);
}

void AmpLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (bone);
    g.setFont (labelFont (11.0f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

//==============================================================================
// Editor
namespace
{
    constexpr int kCellW   = 70;   // width of a knob column
    constexpr int kToggleW = 62;   // width of a switch column
    constexpr int kKnob    = 46;   // knob diameter
    constexpr int kRowH    = 116;  // one row of the synth panel
    constexpr int kGroupGap = 14;
    constexpr int kGroupPad = 8;
}

T150SynthEditor::Ctl* T150SynthEditor::makeKnob (Ctl::Kind kind, const juce::String& id, const juce::String& name)
{
    auto c = std::make_unique<Ctl>();
    c->kind = kind; c->id = id; c->name = name;
    c->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    c->slider.setRotaryParameters (kStartAngle, kEndAngle, true);
    c->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    c->slider.setPopupDisplayEnabled (true, true, this);
    c->slider.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (c->slider);
    c->sliderAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, c->slider);
    if (auto* param = processor.apvts.getParameter (id))
        c->slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    controls.push_back (std::move (c));
    return controls.back().get();
}

T150SynthEditor::Ctl* T150SynthEditor::addDial (const juce::String& id, const juce::String& name)
{
    return makeKnob (Ctl::dial, id, name);
}

T150SynthEditor::Ctl* T150SynthEditor::addBipolar (const juce::String& id, const juce::String& name)
{
    return makeKnob (Ctl::bipolar, id, name);
}

T150SynthEditor::Ctl* T150SynthEditor::addSelector (const juce::String& id, const juce::String& name, juce::StringArray positions)
{
    auto* c = makeKnob (Ctl::selector, id, name);
    c->positions = positions;
    c->slider.setVelocityBasedMode (false);
    c->slider.setMouseDragSensitivity (120);   // easy to click between positions
    return c;
}

T150SynthEditor::Ctl* T150SynthEditor::addToggle (const juce::String& id, const juce::String& name,
                                                 const juce::String& on, const juce::String& off)
{
    auto c = std::make_unique<Ctl>();
    c->kind = Ctl::toggle; c->id = id; c->name = name; c->onLabel = on; c->offLabel = off;
    addAndMakeVisible (c->button);
    c->buttonAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, id, c->button);
    auto* raw = c.get();
    c->button.onStateChange = [this, raw] { repaint (raw->cell); };
    controls.push_back (std::move (c));
    return controls.back().get();
}

T150SynthEditor::T150SynthEditor (T150SynthProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&look);

    // ---------------- Synth panel, laid out like the MS-20 ----------------
    synthRows.resize (3);
    synthRows[0] = {
        { "VCO 1", { addSelector ("v1scale", "SCALE", { "32'", "16'", "8'", "4'" }),
                     addSelector ("v1wave",  "WAVE",  { "TRI", "SAW", "PULSE", "NOISE" }),
                     addDial     ("v1pw",    "PW"),
                     addDial     ("v1pwm",   "PWM") } },
        { "VCO 2", { addSelector ("v2scale", "SCALE", { "16'", "8'", "4'", "2'" }),
                     addSelector ("v2wave",  "WAVE",  { "SAW", "SQR", "PULSE", "RING" }),
                     addBipolar  ("v2pitch", "PITCH") } },
        { "MIXER", { addDial ("mix1", "VCO 1"), addDial ("mix2", "VCO 2") } },
        { "FREQ MOD", { addBipolar ("fmMg", "MG"), addDial ("fmEg1", "EG 1") } },
        { "MASTER", { addBipolar ("tune", "TUNE"), addDial ("porta", "GLIDE") } }
    };
    synthRows[1] = {
        { "HIGH PASS", { addDial ("hpCut", "CUTOFF"), addDial ("hpPeak", "PEAK"),
                         addBipolar ("hpMg", "MG"), addBipolar ("hpEg", "EG 2") } },
        { "LOW PASS",  { addDial ("lpCut", "CUTOFF"), addDial ("lpPeak", "PEAK"),
                         addBipolar ("lpMg", "MG"), addBipolar ("lpEg", "EG 2") } },
        { "FILTER", { addToggle ("rev2", "TYPE", "LATE", "EARLY") } },
        { "EG 1", { addDial ("eg1Delay", "DELAY"), addDial ("eg1Attack", "ATTACK"), addDial ("eg1Release", "RELEASE") } }
    };
    synthRows[2] = {
        { "EG 2", { addDial ("eg2Hold", "HOLD"), addDial ("eg2Attack", "ATTACK"), addDial ("eg2Decay", "DECAY"),
                    addDial ("eg2Sustain", "SUSTAIN"), addDial ("eg2Release", "RELEASE") } },
        { "MG", { addDial ("mgWave", "WAVE"), addDial ("mgFreq", "FREQ") } },
        { "PATCH", { addToggle ("sh", "FILTER MOD", "S&H", "MG"), addDial ("feedback", "AMP FB") } },
        { "OUT", { addDial ("level", "LEVEL") } }
    };

    // ---------------- Amp head ----------------
    ampRow = {
        { "", { addDial ("volume", "VOLUME"), addDial ("bass", "BASS"), addDial ("mid", "MIDDLE"),
                addDial ("treble", "TREBLE"), addDial ("master", "MASTER"), addDial ("presence", "PRESENCE"),
                addDial ("sag", "SAG"), addDial ("output", "OUTPUT") } }
    };
    ampOnCtl = addToggle ("ampOn", "AMP", "ON", "OFF");
    cabCtl   = addToggle ("cabOn", "CAB", "ON", "OFF");

    addAndMakeVisible (loadIRButton);
    loadIRButton.onClick = [this] { chooseIR(); };
    addAndMakeVisible (irLabel);
    irLabel.setText (processor.getImpulseResponseName(), juce::dontSendNotification);
    irLabel.setFont (labelFont (10.0f));
    irLabel.setJustificationType (juce::Justification::centred);
    irLabel.setColour (juce::Label::textColourId, boneDim);

    setSize (1240, 616);
    startTimerHz (30);
}

T150SynthEditor::~T150SynthEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void T150SynthEditor::timerCallback()
{
    const bool g = processor.gateLit.load();
    if (g != lastGate)
    {
        lastGate = g;
        repaint (trigLamp.expanded (24));
    }
}

void T150SynthEditor::chooseIR()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Choose a cabinet impulse response",
                                                       juce::File(), "*.wav;*.aif;*.aiff");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  if (file.existsAsFile())
                                  {
                                      processor.loadImpulseResponse (file);
                                      irLabel.setText (processor.getImpulseResponseName(), juce::dontSendNotification);
                                  }
                              });
}

//==============================================================================
// Layout

void T150SynthEditor::layoutRow (std::vector<Group>& row, juce::Rectangle<int> rowArea, bool boxes)
{
    auto groupWidth = [] (const Group& g)
    {
        int w = 2 * kGroupPad;
        for (auto* c : g.items) w += (c->kind == Ctl::toggle ? kToggleW : kCellW);
        return w;
    };

    int total = 0;
    for (auto& g : row) total += groupWidth (g);
    total += kGroupGap * ((int) row.size() - 1);

    int x = boxes ? rowArea.getCentreX() - total / 2 : rowArea.getX();
    for (auto& g : row)
    {
        const int w = groupWidth (g);
        g.box = { x, rowArea.getY(), w, rowArea.getHeight() };
        int cx = x + kGroupPad;
        for (auto* c : g.items)
        {
            const int cw = (c->kind == Ctl::toggle ? kToggleW : kCellW);
            c->cell = { cx, rowArea.getY(), cw, rowArea.getHeight() };
            const int knobY = rowArea.getY() + 46;
            if (c->kind == Ctl::toggle)
                c->button.setBounds (c->cell.getCentreX() - 18 - 8, knobY - 2, 36, 50);
            else
                c->slider.setBounds (c->cell.getCentreX() - kKnob / 2, knobY, kKnob, kKnob);
            cx += cw;
        }
        x += w + kGroupGap;
    }
}

void T150SynthEditor::resized()
{
    auto area = getLocalBounds().reduced (30, 24);

    synthPanel = area.removeFromTop (44 + 3 * kRowH + 8);
    area.removeFromTop (14);
    ampPanel = area;

    trigLamp = { synthPanel.getRight() - 64, synthPanel.getY() + 14, 16, 16 };

    for (int r = 0; r < 3; ++r)
        layoutRow (synthRows[(size_t) r], { synthPanel.getX(), synthPanel.getY() + 40 + r * kRowH,
                                            synthPanel.getWidth(), kRowH - 8 }, true);

    // Amp head: knobs on the left, switches and IR on the right
    auto amp = ampPanel.reduced (22, 12);
    auto right = amp.removeFromRight (370);
    amp.removeFromLeft (10);
    layoutRow (ampRow, { amp.getX(), amp.getY() + 6, amp.getWidth(), kRowH - 8 }, false);

    // Right side: AMP switch, CAB switch, power lamp, IR loader
    auto rTop = right.withHeight (kRowH - 8).translated (0, 6);
    ampOnCtl->cell = rTop.removeFromLeft (kToggleW + 6);
    cabCtl->cell   = rTop.removeFromLeft (kToggleW + 6);
    for (auto* c : { ampOnCtl, cabCtl })
        c->button.setBounds (c->cell.getCentreX() - 18 - 8, c->cell.getY() + 44, 36, 50);
    auto lampCol = rTop.removeFromLeft (80);
    powerLamp = lampCol.withSizeKeepingCentre (30, 30).withY (lampCol.getY() + 56);
    auto irCol = rTop.reduced (8, 0);
    loadIRButton.setBounds (irCol.getX(), irCol.getY() + 50, irCol.getWidth(), 24);
    irLabel.setBounds (irCol.getX(), irCol.getY() + 78, irCol.getWidth(), 18);

    renderBackground();
}

//==============================================================================
// Artwork

void T150SynthEditor::drawControlArt (juce::Graphics& g, const Ctl& c, juce::Colour ink, juce::Colour inkDim) const
{
    using namespace juce;
    // Name above the control
    g.setColour (ink);
    g.setFont (labelFont (10.0f));
    g.drawText (c.name, Rectangle<float> ((float) c.cell.getX() - 6, (float) c.cell.getY() + 14,
                                          (float) c.cell.getWidth() + 12, 13), Justification::centred);

    if (c.kind == Ctl::toggle)
    {
        const auto b = c.button.getBounds().toFloat();
        g.setFont (labelFont (8.0f));
        g.setColour (inkDim);
        g.drawText (c.onLabel,  Rectangle<float> (b.getRight() - 2, b.getY() + 2, 44, 10), Justification::centredLeft);
        g.drawText (c.offLabel, Rectangle<float> (b.getRight() - 2, b.getBottom() - 12, 44, 10), Justification::centredLeft);
        return;
    }

    const auto b = c.slider.getBounds().toFloat();
    const float cx = b.getCentreX(), cy = b.getCentreY();
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f - 3.0f;

    auto tick = [&] (float frac, float len)
    {
        const float a = kStartAngle + (kEndAngle - kStartAngle) * frac;
        const float sx = std::sin (a), sy = -std::cos (a);
        g.drawLine (cx + sx * (r + 2.0f), cy + sy * (r + 2.0f), cx + sx * (r + 2.0f + len), cy + sy * (r + 2.0f + len), 1.0f);
    };
    auto text = [&] (float frac, const String& s, float dist, float w)
    {
        const float a = kStartAngle + (kEndAngle - kStartAngle) * frac;
        const float sx = std::sin (a), sy = -std::cos (a);
        g.drawText (s, Rectangle<float> (cx + sx * (r + dist) - w * 0.5f, cy + sy * (r + dist) - 5.5f, w, 11),
                    Justification::centred);
    };

    g.setColour (inkDim);
    g.setFont (Font (FontOptions (8.5f, Font::bold)));
    if (c.kind == Ctl::dial)
    {
        for (int i = 0; i <= 10; ++i) { tick (i / 10.0f, 3.0f); text (i / 10.0f, String (i), 10.0f, 14); }
    }
    else if (c.kind == Ctl::bipolar)
    {
        for (int i = 0; i <= 10; ++i) tick (i / 10.0f, i == 5 ? 5.0f : 3.0f);
        text (0.0f, "-5", 11.0f, 16); text (0.5f, "0", 11.0f, 12); text (1.0f, "+5", 11.0f, 16);
    }
    else // selector: labels at each switch position
    {
        g.setFont (Font (FontOptions (8.0f, Font::bold)));
        const int n = c.positions.size();
        for (int i = 0; i < n; ++i)
        {
            const float frac = n > 1 ? (float) i / (float) (n - 1) : 0.0f;
            tick (frac, 4.0f);
            text (frac, c.positions[i], 15.0f, 32);
        }
    }
}

void T150SynthEditor::renderBackground()
{
    using namespace juce;
    const float scale = 2.0f;
    background = Image (Image::ARGB, roundToInt (getWidth() * scale), roundToInt (getHeight() * scale), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (scale));
    const auto full = getLocalBounds().toFloat();
    Random rng (1978);

    // --- Black tolex ---
    g.setColour (tolexBase);
    g.fillRoundedRectangle (full, 10.0f);
    for (int i = 0; i < 16000; ++i)
    {
        const float s = 0.8f + rng.nextFloat() * 1.6f;
        g.setColour ((rng.nextBool() ? Colours::white : Colours::black).withAlpha (0.025f + rng.nextFloat() * 0.035f));
        g.fillEllipse (rng.nextFloat() * full.getWidth(), rng.nextFloat() * full.getHeight(), s, s);
    }
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.04f), 0, 0, Colours::black.withAlpha (0.4f), 0, full.getBottom(), false));
    g.fillRoundedRectangle (full, 10.0f);

    // --- Synth panel: matte black, MS-20 style white section boxes ---
    const auto sp = synthPanel.toFloat();
    g.setColour (Colours::black.withAlpha (0.8f));
    g.fillRect (sp.translated (0.0f, 3.0f));
    g.setGradientFill (ColourGradient (Colour (0xff19191a), 0, sp.getY(), Colour (0xff0e0e0f), 0, sp.getBottom(), false));
    g.fillRect (sp);
    for (int i = 0; i < 9000; ++i)                                           // fine matte grain
    {
        g.setColour ((rng.nextBool() ? Colours::white : Colours::black).withAlpha (0.02f + rng.nextFloat() * 0.03f));
        g.fillRect (sp.getX() + rng.nextFloat() * sp.getWidth(), sp.getY() + rng.nextFloat() * sp.getHeight(), 1.0f, 1.0f);
    }
    g.setColour (Colours::white.withAlpha (0.14f));
    g.drawHorizontalLine ((int) sp.getY() + 1, sp.getX() + 2.0f, sp.getRight() - 2.0f);
    g.setColour (Colours::black);
    g.drawRect (sp, 1.2f);
    g.setColour (accentDim);
    g.drawRect (sp.reduced (6.0f), 1.0f);

    // Header: name, trigger light label
    g.setColour (bone.withAlpha (0.85f));
    g.setFont (Font (FontOptions (20.0f, Font::bold)).withExtraKerningFactor (0.45f));
    g.drawText ("T-150 SYNTH", Rectangle<float> (sp.getX() + 24, sp.getY() + 10, 360, 24), Justification::centredLeft);
    g.setColour (boneDim);
    g.setFont (labelFont (9.0f));
    g.drawText (String (CharPointer_UTF8 ("MONOPHONIC SYNTHESIZER  \xc2\xb7  THROUGH 4 x 6550")), Rectangle<float> (sp.getX() + 250, sp.getY() + 14, 400, 16), Justification::centredLeft);
    g.setColour (bone);
    g.setFont (labelFont (9.0f));
    g.drawText ("TRIG", Rectangle<float> ((float) trigLamp.getRight() + 6, (float) trigLamp.getY(), 40, 16), Justification::centredLeft);

    // Section boxes with titles cut into the top line
    for (auto& row : synthRows)
        for (auto& grp : row)
        {
            const auto bx = grp.box.toFloat();
            g.setColour (accentDim.withAlpha (0.55f));
            g.drawRoundedRectangle (bx, 4.0f, 1.0f);
            g.setFont (labelFont (10.0f));
            const float tw = (float) GlyphArrangement::getStringWidthInt (labelFont (10.0f), grp.title) + 14.0f;
            const Rectangle<float> tab (bx.getCentreX() - tw * 0.5f, bx.getY() - 7.0f, tw, 14.0f);
            g.setColour (Colour (0xff151516));
            g.fillRect (tab);
            g.setColour (accent);
            g.drawText (grp.title, tab, Justification::centred);
            for (auto* c : grp.items)
                drawControlArt (g, *c, bone, boneDim);
        }

    // --- Amp head: gunmetal panel (same as the T-150) ---
    const auto panel = ampPanel.toFloat();
    g.setColour (Colours::black.withAlpha (0.8f));
    g.fillRect (panel.translated (0.0f, 3.0f));
    g.setGradientFill (ColourGradient (Colour (0xff2b2b2e), 0, panel.getY(), Colour (0xff121214), 0, panel.getBottom(), false));
    g.fillRect (panel);
    for (float yy = panel.getY() + 1.0f; yy < panel.getBottom() - 1.0f; yy += 1.0f)
    {
        g.setColour ((rng.nextBool() ? Colours::white : Colours::black).withAlpha (0.015f + rng.nextFloat() * 0.03f));
        g.drawHorizontalLine ((int) yy, panel.getX() + 2.0f, panel.getRight() - 2.0f);
    }
    for (int i = 0; i < 60; ++i)
    {
        const float sx0 = panel.getX() + rng.nextFloat() * panel.getWidth();
        const float sy0 = panel.getY() + rng.nextFloat() * panel.getHeight();
        const float len = 6.0f + rng.nextFloat() * 40.0f, a = (rng.nextFloat() - 0.5f) * 0.9f;
        g.setColour (Colours::white.withAlpha (0.03f + rng.nextFloat() * 0.07f));
        g.drawLine (sx0, sy0, sx0 + std::cos (a) * len, sy0 + std::sin (a) * len, 0.6f);
    }
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawHorizontalLine ((int) panel.getY() + 1, panel.getX() + 2.0f, panel.getRight() - 2.0f);
    g.setColour (Colours::black);
    g.drawRect (panel, 1.2f);
    g.setColour (accentDim);
    g.drawRect (panel.reduced (6.0f), 1.0f);

    for (auto pt : { panel.getTopLeft() + Point<float> (16, 16), panel.getTopRight() + Point<float> (-16, 16),
                     panel.getBottomLeft() + Point<float> (16, -16), panel.getBottomRight() + Point<float> (-16, -16) })
    {
        Path hx;
        for (int i = 0; i < 6; ++i)
        {
            const float a = MathConstants<float>::twoPi * (float) i / 6.0f;
            const Point<float> q (pt.x + std::cos (a) * 4.2f, pt.y + std::sin (a) * 4.2f);
            if (i == 0) hx.startNewSubPath (q); else hx.lineTo (q);
        }
        hx.closeSubPath();
        g.setGradientFill (ColourGradient (Colour (0xff9d9da0), pt.x - 3, pt.y - 3, Colour (0xff2a2a2c), pt.x + 3, pt.y + 3, false));
        g.fillPath (hx);
        g.setColour (Colours::black);
        g.fillEllipse (pt.x - 1.4f, pt.y - 1.4f, 2.8f, 2.8f);
    }

    for (auto& grp : ampRow)
        for (auto* c : grp.items)
            drawControlArt (g, *c, bone, boneDim);
    drawControlArt (g, *ampOnCtl, bone, boneDim);
    drawControlArt (g, *cabCtl, bone, boneDim);
    g.setColour (bone);
    g.setFont (labelFont (10.0f));
    g.drawText ("POWER", powerLamp.toFloat().withSizeKeepingCentre (80, 13).withY ((float) ampOnCtl->cell.getY() + 14),
                Justification::centred);

    g.setFont (labelFont (9.0f));
    g.setColour (accent.withAlpha (0.75f));
    g.drawText (String (CharPointer_UTF8 ("T-150  \xc2\xb7  150 WATTS  \xc2\xb7  4 x 6550  \xc2\xb7  ULTRALINEAR")),
                Rectangle<float> (panel.getX(), panel.getBottom() - 26, panel.getWidth() - 380, 14), Justification::centred);

    // --- Steel corner protectors ---
    auto drawCorner = [&] (float x0, float y0, float sxDir, float syDir)
    {
        const float s = 30.0f;
        Path c;
        c.startNewSubPath (x0, y0);
        c.lineTo (x0 + sxDir * s, y0);
        c.lineTo (x0 + sxDir * s, y0 + syDir * 9.0f);
        c.lineTo (x0 + sxDir * 9.0f, y0 + syDir * s);
        c.lineTo (x0, y0 + syDir * s);
        c.closeSubPath();
        g.setColour (Colours::black.withAlpha (0.7f));
        g.fillPath (c, AffineTransform::translation (sxDir * 1.5f, syDir * 1.5f + 1.5f));
        g.setGradientFill (ColourGradient (Colour (0xffb4b4b8), x0, y0, Colour (0xff2e2e31), x0 + sxDir * s, y0 + syDir * s, false));
        g.fillPath (c);
        g.setColour (Colours::black.withAlpha (0.8f));
        g.strokePath (c, PathStrokeType (1.0f));
        const float rx = x0 + sxDir * 9.5f, ry = y0 + syDir * 9.5f;
        g.setGradientFill (ColourGradient (Colour (0xffe0e0e3), rx - 2, ry - 2, Colour (0xff3a3a3d), rx + 2, ry + 2, false));
        g.fillEllipse (rx - 2.5f, ry - 2.5f, 5.0f, 5.0f);
    };
    drawCorner (full.getX(),     full.getY(),       1.0f,  1.0f);
    drawCorner (full.getRight(), full.getY(),      -1.0f,  1.0f);
    drawCorner (full.getX(),     full.getBottom(),  1.0f, -1.0f);
    drawCorner (full.getRight(), full.getBottom(), -1.0f, -1.0f);
}

void T150SynthEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (Colours::black);
    if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat());

    auto jewel = [&] (Rectangle<float> area, bool lit, float r)
    {
        const float cx = area.getCentreX(), cy = area.getCentreY();
        if (lit)
        {
            g.setGradientFill (ColourGradient (lampWhite.withAlpha (0.55f), cx, cy, Colours::transparentBlack, cx + r * 3.2f, cy, true));
            g.fillEllipse (cx - r * 3.2f, cy - r * 3.2f, r * 6.4f, r * 6.4f);
        }
        g.setGradientFill (ColourGradient (Colour (0xffb0b0b4), cx, cy - r - 3, Colour (0xff2a2a2d), cx, cy + r + 3, false));
        g.fillEllipse (cx - r - 3, cy - r - 3, (r + 3) * 2, (r + 3) * 2);
        g.setGradientFill (ColourGradient (lit ? Colours::white : Colour (0xff5a5854), cx - 3, cy - 4,
                                           lit ? Colour (0xff8a8680) : Colour (0xff1a1918), cx + r, cy + r, true));
        g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
        g.setColour (Colours::black.withAlpha (0.3f));
        for (int i = 0; i < 6; ++i)
        {
            const float a = MathConstants<float>::twoPi * (float) i / 6.0f;
            g.drawLine (cx, cy, cx + std::cos (a) * r, cy + std::sin (a) * r, 0.8f);
        }
        g.setColour (Colours::white.withAlpha (lit ? 0.6f : 0.25f));
        g.fillEllipse (cx - r * 0.55f, cy - r * 0.6f, r * 0.45f, r * 0.35f);
    };

    jewel (powerLamp.toFloat(), true, 11.0f);
    jewel (trigLamp.toFloat(), lastGate, 6.0f);
}
