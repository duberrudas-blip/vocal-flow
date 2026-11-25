#include "PluginEditor.h"

using namespace Palette;
namespace
{
    constexpr float kStart = juce::MathConstants<float>::pi * 1.25f;
    constexpr float kEnd   = juce::MathConstants<float>::pi * 2.75f;
    const juce::Point<float> kPitchC { 175.f, 170.f }, kFormantC { 385.f, 170.f }, kMixC { 558.f, 178.f };
    constexpr float kRingR = 112.f;

    juce::Font boldFont (float h) { return juce::Font (juce::FontOptions (h, juce::Font::bold)); }

    float textWidth (const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.f, 0.f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    // Text laid out along a circle arc (centred on `centreAngle`, clockwise from 12 o'clock)
    void drawArcText (juce::Graphics& g, const juce::String& text, juce::Point<float> c, float R,
                      float centreAngle, const juce::Font& f, juce::Colour col)
    {
        g.setFont (f);
        g.setColour (col);
        std::vector<float> w;
        float total = 0.f;
        for (int i = 0; i < text.length(); ++i)
        {
            auto ch = text.substring (i, i + 1);
            float cw = ch == " " ? f.getHeight() * 0.35f : textWidth (f, ch) + 1.0f;
            w.push_back (cw);
            total += cw;
        }
        float pos = -total * 0.5f;
        for (int i = 0; i < text.length(); ++i)
        {
            const float mid = pos + w[(size_t) i] * 0.5f;
            pos += w[(size_t) i];
            juce::Graphics::ScopedSaveState s (g);
            g.addTransform (juce::AffineTransform::rotation (centreAngle + mid / R, c.x, c.y));
            g.drawText (text.substring (i, i + 1),
                        juce::Rectangle<float> (c.x - w[(size_t) i], c.y - R - f.getHeight() * 0.5f,
                                                w[(size_t) i] * 2.f, f.getHeight()),
                        juce::Justification::centred, false);
        }
    }
}

// ---------------------------------------------------------------- Look & feel
void FlowLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startA, float endA, juce::Slider& s)
{
    auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.f);
    auto c = b.getCentre();
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const float ang = startA + pos * (endA - startA);
    const bool small = s.getComponentID() == "mix";

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (b.translated (0.f, 4.f));

    if (! small)
    {
        g.setGradientFill (juce::ColourGradient (bezelTop, c.x, b.getY(), bezelBot, c.x, b.getBottom(), false));
        g.fillEllipse (b);
        auto hole = b.withSizeKeepingCentre (r * 1.34f, r * 1.34f);
        g.setGradientFill (juce::ColourGradient (holeTop, c.x, hole.getY(), holeBot, c.x, hole.getBottom(), false));
        g.fillEllipse (hole);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (hole, 2.f);

        juce::Path p;
        p.addRoundedRectangle (-2.5f, -r * 0.97f, 5.f, r * 0.24f, 2.f);
        g.setColour (cream);
        g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a4a66), c.x, b.getY(),
                                                 juce::Colour (0xff141a2a), c.x, b.getBottom(), false));
        g.fillEllipse (b);
        g.setColour (ringTop.withAlpha (0.5f));
        g.drawEllipse (b.reduced (1.f), 1.5f);
        juce::Path p;
        p.addRoundedRectangle (-1.8f, -r * 0.82f, 3.6f, r * 0.62f, 1.8f);
        g.setColour (cream);
        g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }

    if (s.isMouseOverOrDragging())
    {
        g.setColour (glow.withAlpha (0.9f));
        g.drawEllipse (b.reduced (1.f), 2.5f);
    }
}

void FlowLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& bt, bool hl, bool)
{
    auto b = bt.getLocalBounds().toFloat().reduced (1.f);
    auto c = b.getCentre();
    const bool on = bt.getToggleState();

    if (bt.getComponentID() == "fine")       // LED
    {
        if (on) { g.setColour (glow.withAlpha (0.35f)); g.fillEllipse (b.expanded (4.f)); }
        g.setColour (on ? glow : juce::Colour (0xff245560));
        g.fillEllipse (b.reduced (3.f));
        g.setColour (cream.withAlpha (hl ? 0.9f : 0.5f));
        g.drawEllipse (b.reduced (2.f), 1.5f);
        return;
    }

    // chain-link button
    g.setGradientFill (juce::ColourGradient (bezelTop, c.x, b.getY(), bezelBot, c.x, b.getBottom(), false));
    g.fillEllipse (b);
    g.setColour (ringTop.withAlpha (hl ? 1.f : 0.7f));
    g.drawEllipse (b.reduced (1.f), 2.f);
    g.setColour (on ? glow : cream.withAlpha (0.8f));
    g.drawRoundedRectangle (c.x - 11.f, c.y - 5.f, 14.f, 10.f, 5.f, 2.2f);
    g.drawRoundedRectangle (c.x - 3.f,  c.y - 5.f, 14.f, 10.f, 5.f, 2.2f);
}

// ---------------------------------------------------------------- Editor
VocalFlowEditor::VocalFlowEditor (VocalFlowProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setSize (640, 340);

    for (auto* s : { &pitch, &formant, &mix })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setLookAndFeel (&laf);
        s->setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (*s);
    }
    mix.setComponentID ("mix");
    link.setComponentID ("link");
    fine.setComponentID ("fine");
    for (auto* b : { &link, &fine }) { b->setLookAndFeel (&laf); addAndMakeVisible (*b); }

    pitchA   = std::make_unique<SA> (proc.apvts, "pitch",   pitch);
    formantA = std::make_unique<SA> (proc.apvts, "formant", formant);
    mixA     = std::make_unique<SA> (proc.apvts, "mix",     mix);
    linkA    = std::make_unique<BA> (proc.apvts, "link",    link);
    fineA    = std::make_unique<BA> (proc.apvts, "fine",    fine);

    for (auto* s : { &pitch, &formant, &mix })
        s->setRotaryParameters (kStart, kEnd, true);
    pitch.setDoubleClickReturnValue (true, 0.0);
    formant.setDoubleClickReturnValue (true, 0.0);
    mix.setDoubleClickReturnValue (true, 100.0);

    auto cents = [this] (double v) {
        const bool f = fine.getToggleState();
        return juce::String (f ? v / 12.0 : v, f ? 1 : 0) + " ct";
    };
    pitch.textFromValueFunction   = cents;
    formant.textFromValueFunction = cents;
    mix.textFromValueFunction     = [] (double v) { return juce::String (juce::roundToInt (v)) + " %"; };

    auto snap = [this] { lastPitch = pitch.getValue(); lastFormant = formant.getValue(); };
    pitch.onDragStart   = snap;
    formant.onDragStart = snap;
    pitch.onValueChange   = [this] { onKnob (pitch, formant, lastPitch); };
    formant.onValueChange = [this] { onKnob (formant, pitch, lastFormant); };
    fine.onClick = [this] { repaint(); };
    snap();
}

VocalFlowEditor::~VocalFlowEditor()
{
    for (auto* s : { &pitch, &formant, &mix }) s->setLookAndFeel (nullptr);
    link.setLookAndFeel (nullptr);
    fine.setLookAndFeel (nullptr);
}

void VocalFlowEditor::onKnob (juce::Slider& moved, juce::Slider& other, double& lastMoved)
{
    const double v = moved.getValue();
    const double d = v - lastMoved;
    lastMoved = v;
    if (updating || ! link.getToggleState() || d == 0.0) return;
    updating = true;
    other.setValue (other.getValue() + d, juce::sendNotificationSync);
    updating = false;
}

void VocalFlowEditor::resized()
{
    auto sq = [] (juce::Point<float> c, int r) { return juce::Rectangle<int> ((int) c.x - r, (int) c.y - r, r * 2, r * 2); };
    pitch.setBounds (sq (kPitchC, 64));
    formant.setBounds (sq (kFormantC, 64));
    mix.setBounds (sq (kMixC, 38));
    link.setBounds (280 - 17, 170 - 17, 34, 34);
    fine.setBounds (280 - 10, 248, 20, 20);
}

void VocalFlowEditor::drawRing (juce::Graphics& g, juce::Point<float> c, const juce::String& label)
{
    auto disc = juce::Rectangle<float> (kRingR * 2.f, kRingR * 2.f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillEllipse (disc.translated (0.f, 3.f));
    g.setGradientFill (juce::ColourGradient (ringTop, c.x - kRingR, c.y - kRingR, ringBot, c.x + kRingR, c.y + kRingR, false));
    g.fillEllipse (disc);
    g.setColour (ink.withAlpha (0.35f));
    g.drawEllipse (disc, 1.5f);

    for (int i = 0; i <= 40; ++i)           // tick dots
    {
        const float a = kStart + (float) i / 40.f * (kEnd - kStart);
        const float d = (i % 10 == 0) ? 3.4f : 1.9f;
        g.setColour (ink);
        g.fillEllipse (c.x + 78.f * std::sin (a) - d / 2, c.y - 78.f * std::cos (a) - d / 2, d, d);
    }

    drawArcText (g, label, c, 100.f, -0.45f, boldFont (20.f), ink);

    const bool f = fine.getToggleState();
    g.setFont (boldFont (10.f));
    g.setColour (ink);
    g.drawText (f ? "-100" : "-1200", juce::Rectangle<float> (60.f, 16.f).withCentre ({ c.x - 50.f, c.y + 88.f }), juce::Justification::centred);
    g.drawText (f ? "100"  : "1200",  juce::Rectangle<float> (60.f, 16.f).withCentre ({ c.x + 50.f, c.y + 88.f }), juce::Justification::centred);
}

void VocalFlowEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    auto body = juce::Rectangle<float> (20.f, 45.f, 600.f, 250.f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (body.translated (0.f, 5.f), 125.f);
    g.setGradientFill (juce::ColourGradient (bodyTop, 0.f, body.getY(), bodyBot, 0.f, body.getBottom(), false));
    g.fillRoundedRectangle (body, 125.f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (body.reduced (2.f), 123.f, 1.5f);

    juce::Path tri;
    tri.addTriangle (268.f, 58.f, 292.f, 58.f, 280.f, 74.f);
    g.setColour (ringTop.withAlpha (0.55f));
    g.fillPath (tri);

    drawRing (g, kPitchC, "PITCH");
    drawRing (g, kFormantC, "FORMANT");

    // title
    {
        juce::Graphics::ScopedSaveState s (g);
        g.addTransform (juce::AffineTransform::rotation (-0.1f, 560.f, 85.f));
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.setFont (boldFont (24.f));
        g.drawText ("VOCAL", juce::Rectangle<float> (500.f, 62.f, 120.f, 26.f).translated (1.5f, 2.f), juce::Justification::centred);
        g.drawText ("FLOW",  juce::Rectangle<float> (500.f, 86.f, 120.f, 26.f).translated (1.5f, 2.f), juce::Justification::centred);
        g.setColour (cream);
        g.drawText ("VOCAL", juce::Rectangle<float> (500.f, 62.f, 120.f, 26.f), juce::Justification::centred);
        g.drawText ("FLOW",  juce::Rectangle<float> (500.f, 86.f, 120.f, 26.f), juce::Justification::centred);
    }

    // mix labels
    drawArcText (g, "MIX", kMixC, 52.f, 0.f, boldFont (13.f), cream);
    g.setFont (boldFont (10.f));
    g.setColour (cream);
    g.drawText ("0",   juce::Rectangle<float> (30.f, 14.f).withCentre ({ kMixC.x - 26.f, kMixC.y + 48.f }), juce::Justification::centred);
    g.drawText ("100", juce::Rectangle<float> (30.f, 14.f).withCentre ({ kMixC.x + 26.f, kMixC.y + 48.f }), juce::Justification::centred);

    g.setFont (boldFont (11.f));
    g.setColour (cream);
    g.drawText ("FINE", juce::Rectangle<float> (60.f, 14.f).withCentre ({ 280.f, 280.f }), juce::Justification::centred);
}
