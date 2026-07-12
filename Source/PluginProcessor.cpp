#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParameterIDs
{
    constexpr auto pitch = "pitch";
    constexpr auto fine = "fine";
    constexpr auto mix = "mix";
    constexpr auto output = "output";
    constexpr auto bypass = "bypass";
}

SoundShifterProAudioProcessor::SoundShifterProAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
SoundShifterProAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::pitch, 1 },
        "Pitch",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("st")));

    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::fine, 1 },
        "Fine",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("ct")));

    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::mix, 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::output, 1 },
        "Output",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParameterIDs::bypass, 1 },
        "Bypass",
        false));

    return { parameters.begin(), parameters.end() };
}

void SoundShifterProAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        static_cast<juce::uint32>(juce::jmax(1, getTotalNumOutputChannels()))
    };

    pitchShiftEngine.prepare(spec);
    outputGainLinear.reset(sampleRate, 0.02);
    outputGainLinear.setCurrentAndTargetValue(1.0f);
}

void SoundShifterProAudioProcessor::releaseResources()
{
    pitchShiftEngine.reset();
}

bool SoundShifterProAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void SoundShifterProAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                 juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    const auto inputMagnitude = buffer.getMagnitude(0, buffer.getNumSamples());
    inputLevelDb.store(juce::Decibels::gainToDecibels(inputMagnitude, -100.0f));

    const auto bypassed = apvts.getRawParameterValue(ParameterIDs::bypass)->load() > 0.5f;

    if (!bypassed)
    {
        const auto pitch = apvts.getRawParameterValue(ParameterIDs::pitch)->load();
        const auto fine = apvts.getRawParameterValue(ParameterIDs::fine)->load();

        pitchShiftEngine.setPitchSemitones(pitch);
        pitchShiftEngine.setFineCents(fine);
        pitchShiftEngine.process(buffer);

        // Mix remains transparent until Milestone 2 provides wet audio.
        juce::ignoreUnused(apvts.getRawParameterValue(ParameterIDs::mix)->load());
    }

    const auto outputDb = apvts.getRawParameterValue(ParameterIDs::output)->load();
    outputGainLinear.setTargetValue(juce::Decibels::decibelsToGain(outputDb));

    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto gain = outputGainLinear.getNextValue();
        for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sample, buffer.getSample(channel, sample) * gain);
    }

    const auto outputMagnitude = buffer.getMagnitude(0, buffer.getNumSamples());
    outputLevelDb.store(juce::Decibels::gainToDecibels(outputMagnitude, -100.0f));
}

void SoundShifterProAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void SoundShifterProAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* SoundShifterProAudioProcessor::createEditor()
{
    return new SoundShifterProAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoundShifterProAudioProcessor();
}
