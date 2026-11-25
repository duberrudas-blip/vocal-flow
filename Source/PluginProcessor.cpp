#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout VocalFlowProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "pitch", 1 }, "Pitch",
                                                       NormalisableRange<float> (-1200.f, 1200.f, 0.1f), 0.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "formant", 1 }, "Formant",
                                                       NormalisableRange<float> (-1200.f, 1200.f, 0.1f), 0.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                                                       NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "link", 1 }, "Link", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "fine", 1 }, "Fine", false));
    return layout;
}

VocalFlowProcessor::VocalFlowProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    pitchParam   = apvts.getRawParameterValue ("pitch");
    formantParam = apvts.getRawParameterValue ("formant");
    mixParam     = apvts.getRawParameterValue ("mix");
    fineParam    = apvts.getRawParameterValue ("fine");
    setLatencySamples (PitchFormantShifter::latency);
}

bool VocalFlowProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void VocalFlowProcessor::prepareToPlay (double sampleRate, int)
{
    for (size_t c = 0; c < 2; ++c)
    {
        shifters[c].prepare (sampleRate);
        dryDelay[c].assign ((size_t) PitchFormantShifter::latency, 0.f);
        dryPos[c] = 0;
    }
    setLatencySamples (PitchFormantShifter::latency);
}

void VocalFlowProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    for (int c = numCh; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, buffer.getNumSamples());

    const float fineScale = fineParam->load() > 0.5f ? (1.0f / 12.0f) : 1.0f;
    const float pr  = std::pow (2.0f, pitchParam->load()   * fineScale / 1200.0f);
    const float fr  = std::pow (2.0f, formantParam->load() * fineScale / 1200.0f);
    const float mix = mixParam->load() * 0.01f;
    const int   len = PitchFormantShifter::latency;

    for (int c = 0; c < numCh; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        auto& delay = dryDelay[(size_t) c];
        int&  pos   = dryPos[(size_t) c];
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float in  = d[n];
            const float wet = shifters[(size_t) c].process (in, pr, fr);
            const float dry = delay[(size_t) pos];
            delay[(size_t) pos] = in;
            if (++pos >= len) pos = 0;
            d[n] = dry + (wet - dry) * mix;
        }
    }
}

juce::AudioProcessorEditor* VocalFlowProcessor::createEditor() { return new VocalFlowEditor (*this); }

void VocalFlowProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void VocalFlowProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalFlowProcessor(); }
