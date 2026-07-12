#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParameterIDs
{
    constexpr auto pitch = "pitch";
    constexpr auto fine = "fine";
    constexpr auto mix = "mix";
    constexpr auto output = "output";
    constexpr auto bypass = "bypass";
    constexpr auto hq = "hq";
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

    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParameterIDs::hq, 1 },
        "High Quality",
        true));

    return { parameters.begin(), parameters.end() };
}

void SoundShifterProAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        static_cast<juce::uint32>(juce::jmax(1, getTotalNumOutputChannels()))
    };

    currentSampleRate.store(sampleRate);
    pitchShiftEngine.prepare(spec);
    setLatencySamples(pitchShiftEngine.getLatencySamples());
    prepareDryDelay(static_cast<int>(spec.numChannels), samplesPerBlock);
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
    juce::ScopedNoDenormals noDenormals;
    handleMidiControl(midiMessages);

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    const auto inputLeftMagnitude = buffer.getNumChannels() > 0
                                      ? buffer.getMagnitude(0, 0, buffer.getNumSamples())
                                      : 0.0f;
    const auto inputRightMagnitude = buffer.getNumChannels() > 1
                                       ? buffer.getMagnitude(1, 0, buffer.getNumSamples())
                                       : inputLeftMagnitude;
    inputLeftDb.store(juce::Decibels::gainToDecibels(inputLeftMagnitude, -100.0f));
    inputRightDb.store(juce::Decibels::gainToDecibels(inputRightMagnitude, -100.0f));

    const auto bypassed = apvts.getRawParameterValue(ParameterIDs::bypass)->load() > 0.5f;
    createDelayedDry(buffer, buffer.getNumSamples());

    if (!bypassed)
    {
        const auto pitch = apvts.getRawParameterValue(ParameterIDs::pitch)->load();
        const auto fine = apvts.getRawParameterValue(ParameterIDs::fine)->load();
        const auto mix = juce::jlimit(0.0f, 1.0f,
            apvts.getRawParameterValue(ParameterIDs::mix)->load() * 0.01f);

        pitchShiftEngine.setPitchSemitones(pitch);
        pitchShiftEngine.setFineCents(fine);
        const auto highQuality = apvts.getRawParameterValue(ParameterIDs::hq)->load() > 0.5f;
        pitchShiftEngine.setHighQuality(highQuality);
        pitchShiftEngine.process(buffer);

        const auto dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
        const auto wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            buffer.applyGain(channel, 0, buffer.getNumSamples(), wetGain);
            buffer.addFrom(channel, 0, delayedDryBlock, channel, 0,
                           buffer.getNumSamples(), dryGain);
        }
    }
    else
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.copyFrom(channel, 0, delayedDryBlock, channel, 0, buffer.getNumSamples());
    }

    const auto outputDb = apvts.getRawParameterValue(ParameterIDs::output)->load();
    outputGainLinear.setTargetValue(juce::Decibels::decibelsToGain(outputDb));

    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto gain = outputGainLinear.getNextValue();
        for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sample, buffer.getSample(channel, sample) * gain);
    }

    const auto outputLeftMagnitude = buffer.getNumChannels() > 0
                                       ? buffer.getMagnitude(0, 0, buffer.getNumSamples())
                                       : 0.0f;
    const auto outputRightMagnitude = buffer.getNumChannels() > 1
                                        ? buffer.getMagnitude(1, 0, buffer.getNumSamples())
                                        : outputLeftMagnitude;
    outputLeftDb.store(juce::Decibels::gainToDecibels(outputLeftMagnitude, -100.0f));
    outputRightDb.store(juce::Decibels::gainToDecibels(outputRightMagnitude, -100.0f));
}


void SoundShifterProAudioProcessor::handleMidiControl(const juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (message.isController())
        {
            const auto cc = message.getControllerNumber();
            const auto pressed = message.getControllerValue() >= 64;
            const auto wasPressed = ccPressed[static_cast<size_t>(cc)];
            ccPressed[static_cast<size_t>(cc)] = pressed;

            if (pressed && !wasPressed)
            {
                if (cc == 30) changePitchBySemitones(-1.0f);
                if (cc == 31) changePitchBySemitones(1.0f);
                if (cc == 32) setPitchFromMidi(0.0f);
            }
        }
        else if (message.isNoteOn())
        {
            const auto note = message.getNoteNumber();
            if (note == 60) changePitchBySemitones(-1.0f);
            if (note == 62) changePitchBySemitones(1.0f);
            if (note == 64) setPitchFromMidi(0.0f);
        }
    }
}

void SoundShifterProAudioProcessor::changePitchBySemitones(float amount)
{
    const auto current = apvts.getRawParameterValue(ParameterIDs::pitch)->load();
    setPitchFromMidi(current + amount);
}

void SoundShifterProAudioProcessor::setPitchFromMidi(float semitones)
{
    if (auto* parameter = apvts.getParameter(ParameterIDs::pitch))
    {
        const auto value = juce::jlimit(-12.0f, 12.0f, semitones);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }
}

void SoundShifterProAudioProcessor::prepareDryDelay(int channels, int maximumBlockSize)
{
    const auto delaySamples = juce::jmax(1, getLatencySamples());
    dryDelayBuffer.setSize(juce::jmax(1, channels),
                           delaySamples + juce::jmax(1, maximumBlockSize) + 1,
                           false, true, false);
    delayedDryBlock.setSize(juce::jmax(1, channels),
                            juce::jmax(1, maximumBlockSize),
                            false, true, false);
    dryDelayBuffer.clear();
    delayedDryBlock.clear();
    dryDelayWritePosition = 0;
}

void SoundShifterProAudioProcessor::createDelayedDry(const juce::AudioBuffer<float>& input,
                                                      int numSamples)
{
    const auto delaySamples = getLatencySamples();
    const auto ringSize = dryDelayBuffer.getNumSamples();
    const auto channels = juce::jmin(input.getNumChannels(), dryDelayBuffer.getNumChannels());

    jassert(numSamples <= delayedDryBlock.getNumSamples());
    delayedDryBlock.clear();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto readPosition = dryDelayWritePosition - delaySamples;
        if (readPosition < 0)
            readPosition += ringSize;

        for (int channel = 0; channel < channels; ++channel)
        {
            delayedDryBlock.setSample(channel, sample,
                                      dryDelayBuffer.getSample(channel, readPosition));
            dryDelayBuffer.setSample(channel, dryDelayWritePosition,
                                     input.getSample(channel, sample));
        }

        dryDelayWritePosition = (dryDelayWritePosition + 1) % ringSize;
    }
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
