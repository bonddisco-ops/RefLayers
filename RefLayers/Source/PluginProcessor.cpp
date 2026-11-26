#include "PluginProcessor.h"
#include "PluginEditor.h"

RefLayersProcessor::RefLayersProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void RefLayersProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

bool RefLayersProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void RefLayersProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();

    for (int c = numIn; c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    engine.push (buffer.getArrayOfReadPointers(), std::min (numIn, buffer.getNumChannels()), buffer.getNumSamples());
}

juce::AudioProcessorEditor* RefLayersProcessor::createEditor()
{
    return new RefLayersEditor (*this);
}

void RefLayersProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = uiState.createXml())
        copyXmlToBinary (*xml, destData);
}

void RefLayersProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        uiState.copyPropertiesFrom (juce::ValueTree::fromXml (*xml), nullptr);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RefLayersProcessor();
}
