#include "PluginProcessor.h"

PseudoStereoProcessor::PseudoStereoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    sourceParam = apvts.getRawParameterValue ("source");
    delayParam  = apvts.getRawParameterValue ("delay");
    mixParam    = apvts.getRawParameterValue ("mix");
}

juce::AudioProcessorValueTreeState::ParameterLayout PseudoStereoProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Same controls and defaults as the Nyquist script
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "source", 1 }, "Source channel",
                                                        StringArray { "Left (upper)", "Right (lower)" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "delay", 1 }, "Delay factor",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Effect mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 80.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));
    return layout;
}

void PseudoStereoProcessor::prepareToPlay (double sampleRate, int)
{
    dsp.prepare (sampleRate);
}

bool PseudoStereoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // The original requires a 2-channel track, so: stereo in, stereo out.
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PseudoStereoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    dsp.setParameters ((int) std::lround (sourceParam->load()), delayParam->load(), mixParam->load());
    dsp.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());
}

juce::AudioProcessorEditor* PseudoStereoProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void PseudoStereoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PseudoStereoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PseudoStereoProcessor();
}
