#pragma once
#include "PluginProcessor.h"

namespace Palette
{
    // Teal body + mint rings + coral knobs (colour twist on the reference look)
    inline const juce::Colour bg       { 0xff08141b };
    inline const juce::Colour bodyTop  { 0xff2b8e92 };
    inline const juce::Colour bodyBot  { 0xff15525c };
    inline const juce::Colour ringTop  { 0xff8cf0cd };
    inline const juce::Colour ringBot  { 0xff3db596 };
    inline const juce::Colour ink      { 0xff0b3b3f };
    inline const juce::Colour bezelTop { 0xffff9a76 };
    inline const juce::Colour bezelBot { 0xffb4503c };
    inline const juce::Colour holeTop  { 0xff1d2228 };
    inline const juce::Colour holeBot  { 0xff0b0e12 };
    inline const juce::Colour glow     { 0xff5cf2ff };
    inline const juce::Colour cream    { 0xfffff3e0 };
}

class FlowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class VocalFlowEditor : public juce::AudioProcessorEditor
{
public:
    explicit VocalFlowEditor (VocalFlowProcessor&);
    ~VocalFlowEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void drawRing (juce::Graphics&, juce::Point<float> centre, const juce::String& label);
    void onKnob (juce::Slider& moved, juce::Slider& other, double& lastMoved);

    VocalFlowProcessor& proc;
    FlowLookAndFeel laf;
    juce::Slider pitch, formant, mix;
    juce::ToggleButton link, fine;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> pitchA, formantA, mixA;
    std::unique_ptr<BA> linkA, fineA;

    double lastPitch = 0.0, lastFormant = 0.0;
    bool updating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalFlowEditor)
};
