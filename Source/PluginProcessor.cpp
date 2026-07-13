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
    cacheParameterPointers();
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

void SoundShifterProAudioProcessor::cacheParameterPointers() noexcept
{
    pitchParameter = apvts.getRawParameterValue(ParameterIDs::pitch);
    fineParameter = apvts.getRawParameterValue(ParameterIDs::fine);
    mixParameter = apvts.getRawParameterValue(ParameterIDs::mix);
    outputParameter = apvts.getRawParameterValue(ParameterIDs::output);
    bypassParameter = apvts.getRawParameterValue(ParameterIDs::bypass);
    hqParameter = apvts.getRawParameterValue(ParameterIDs::hq);

    jassert(pitchParameter != nullptr);
    jassert(fineParameter != nullptr);
    jassert(mixParameter != nullptr);
    jassert(outputParameter != nullptr);
    jassert(bypassParameter != nullptr);
    jassert(hqParameter != nullptr);
}

void SoundShifterProAudioProcessor::prepareToPlay(double sampleRate,
                                                  int samplesPerBlock)
{
    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        static_cast<juce::uint32>(
            juce::jmax(1, getTotalNumOutputChannels()))
    };

    currentSampleRate.store(sampleRate);

    pitchShiftEngine.prepare(spec);

    const auto latency = pitchShiftEngine.getLatencySamples();
    setLatencySamples(latency);

    prepareDryDelay(
        static_cast<int>(spec.numChannels),
        samplesPerBlock);

    outputGainLinear.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.02);
    bypassWetSmoothed.reset(sampleRate, 0.01);

    const auto initialOutput = outputParameter != nullptr
        ? juce::Decibels::decibelsToGain(outputParameter->load())
        : 1.0f;

    const auto initialMix = mixParameter != nullptr
        ? juce::jlimit(0.0f, 1.0f, mixParameter->load() * 0.01f)
        : 1.0f;

    const auto initialBypassWet = bypassParameter != nullptr
        && bypassParameter->load() > 0.5f
            ? 0.0f
            : 1.0f;

    outputGainLinear.setCurrentAndTargetValue(initialOutput);
    mixSmoothed.setCurrentAndTargetValue(initialMix);
    bypassWetSmoothed.setCurrentAndTargetValue(initialBypassWet);
}

void SoundShifterProAudioProcessor::releaseResources()
{
    pitchShiftEngine.reset();
    dryDelayBuffer.clear();
    delayedDryBlock.clear();
}

bool SoundShifterProAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void SoundShifterProAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    handleMidiControl(midiMessages);

    const auto numSamples = buffer.getNumSamples();

    for (auto channel = getTotalNumInputChannels();
         channel < getTotalNumOutputChannels();
         ++channel)
    {
        buffer.clear(channel, 0, numSamples);
    }

    updateMeters(buffer, true);
    createDelayedDry(buffer, numSamples);

    const auto pitch = pitchParameter != nullptr
        ? pitchParameter->load()
        : 0.0f;

    const auto fine = fineParameter != nullptr
        ? fineParameter->load()
        : 0.0f;

    const auto requestedMix = mixParameter != nullptr
        ? juce::jlimit(0.0f, 1.0f, mixParameter->load() * 0.01f)
        : 1.0f;

    const auto bypassed = bypassParameter != nullptr
        && bypassParameter->load() > 0.5f;

    const auto highQuality = hqParameter == nullptr
        || hqParameter->load() > 0.5f;

    pitchShiftEngine.setPitchSemitones(pitch);
    pitchShiftEngine.setFineCents(fine);
    pitchShiftEngine.setHighQuality(highQuality);

    bypassWetSmoothed.setTargetValue(bypassed ? 0.0f : 1.0f);

    const auto shouldProcessWet =
        !bypassed
        || bypassWetSmoothed.isSmoothing()
        || bypassWetSmoothed.getCurrentValue() > 0.0001f;

    if (shouldProcessWet)
        pitchShiftEngine.process(buffer);

    applyDryWetMix(
        buffer,
        numSamples,
        requestedMix,
        bypassed);

    applyOutputGain(buffer, numSamples);
    updateMeters(buffer, false);
}

void SoundShifterProAudioProcessor::applyDryWetMix(
    juce::AudioBuffer<float>& wetBuffer,
    int numSamples,
    float requestedMix,
    bool bypassed)
{
    juce::ignoreUnused(bypassed);

    mixSmoothed.setTargetValue(requestedMix);

    const auto mixStart = mixSmoothed.getCurrentValue();
    const auto bypassStart = bypassWetSmoothed.getCurrentValue();

    const auto mixEnd = mixSmoothed.skip(numSamples);
    const auto bypassEnd = bypassWetSmoothed.skip(numSamples);

    const auto effectiveStart =
        juce::jlimit(0.0f, 1.0f, mixStart * bypassStart);

    const auto effectiveEnd =
        juce::jlimit(0.0f, 1.0f, mixEnd * bypassEnd);

    const auto dryStart =
        std::cos(effectiveStart * juce::MathConstants<float>::halfPi);

    const auto dryEnd =
        std::cos(effectiveEnd * juce::MathConstants<float>::halfPi);

    const auto wetStart =
        std::sin(effectiveStart * juce::MathConstants<float>::halfPi);

    const auto wetEnd =
        std::sin(effectiveEnd * juce::MathConstants<float>::halfPi);

    const auto channels = juce::jmin(
        wetBuffer.getNumChannels(),
        delayedDryBlock.getNumChannels());

    for (int channel = 0; channel < channels; ++channel)
    {
        wetBuffer.applyGainRamp(
            channel,
            0,
            numSamples,
            wetStart,
            wetEnd);

        wetBuffer.addFromWithRamp(
            channel,
            0,
            delayedDryBlock.getReadPointer(channel),
            numSamples,
            dryStart,
            dryEnd);
    }
}

void SoundShifterProAudioProcessor::applyOutputGain(
    juce::AudioBuffer<float>& buffer,
    int numSamples)
{
    const auto outputDb = outputParameter != nullptr
        ? outputParameter->load()
        : 0.0f;

    outputGainLinear.setTargetValue(
        juce::Decibels::decibelsToGain(outputDb));

    const auto startGain = outputGainLinear.getCurrentValue();
    const auto endGain = outputGainLinear.skip(numSamples);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        buffer.applyGainRamp(
            channel,
            0,
            numSamples,
            startGain,
            endGain);
    }
}

void SoundShifterProAudioProcessor::updateMeters(
    const juce::AudioBuffer<float>& buffer,
    bool inputMeters) noexcept
{
    const auto numSamples = buffer.getNumSamples();

    const auto leftMagnitude = buffer.getNumChannels() > 0
        ? buffer.getMagnitude(0, 0, numSamples)
        : 0.0f;

    const auto rightMagnitude = buffer.getNumChannels() > 1
        ? buffer.getMagnitude(1, 0, numSamples)
        : leftMagnitude;

    const auto leftDb =
        juce::Decibels::gainToDecibels(leftMagnitude, -100.0f);

    const auto rightDb =
        juce::Decibels::gainToDecibels(rightMagnitude, -100.0f);

    if (inputMeters)
    {
        inputLeftDb.store(leftDb);
        inputRightDb.store(rightDb);
    }
    else
    {
        outputLeftDb.store(leftDb);
        outputRightDb.store(rightDb);
    }
}

void SoundShifterProAudioProcessor::handleMidiControl(
    const juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (message.isController())
        {
            const auto cc = message.getControllerNumber();
            const auto pressed = message.getControllerValue() >= 64;

            if (!juce::isPositiveAndBelow(
                    cc,
                    static_cast<int>(ccPressed.size())))
            {
                continue;
            }

            const auto wasPressed =
                ccPressed[static_cast<size_t>(cc)];

            ccPressed[static_cast<size_t>(cc)] = pressed;

            if (pressed && !wasPressed)
            {
                if (cc == 30)
                    changePitchBySemitones(-1.0f);
                else if (cc == 31)
                    changePitchBySemitones(1.0f);
                else if (cc == 32)
                    setPitchFromMidi(0.0f);
            }
        }
        else if (message.isNoteOn())
        {
            const auto note = message.getNoteNumber();

            if (note == 60)
                changePitchBySemitones(-1.0f);
            else if (note == 62)
                changePitchBySemitones(1.0f);
            else if (note == 64)
                setPitchFromMidi(0.0f);
        }
    }
}

void SoundShifterProAudioProcessor::changePitchBySemitones(float amount)
{
    const auto current = pitchParameter != nullptr
        ? pitchParameter->load()
        : 0.0f;

    setPitchFromMidi(current + amount);
}

void SoundShifterProAudioProcessor::setPitchFromMidi(float semitones)
{
    if (auto* parameter = apvts.getParameter(ParameterIDs::pitch))
    {
        const auto value = juce::jlimit(-12.0f, 12.0f, semitones);

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(
            parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
}

void SoundShifterProAudioProcessor::prepareDryDelay(
    int channels,
    int maximumBlockSize)
{
    const auto delaySamples =
        juce::jmax(1, getLatencySamples());

    const auto safeChannels =
        juce::jmax(1, channels);

    const auto safeBlockSize =
        juce::jmax(1, maximumBlockSize);

    dryDelayBuffer.setSize(
        safeChannels,
        delaySamples + safeBlockSize + 1,
        false,
        true,
        false);

    delayedDryBlock.setSize(
        safeChannels,
        safeBlockSize,
        false,
        true,
        false);

    dryDelayBuffer.clear();
    delayedDryBlock.clear();
    dryDelayWritePosition = 0;
}

void SoundShifterProAudioProcessor::createDelayedDry(
    const juce::AudioBuffer<float>& input,
    int numSamples)
{
    const auto delaySamples = getLatencySamples();
    const auto ringSize = dryDelayBuffer.getNumSamples();

    const auto channels = juce::jmin(
        input.getNumChannels(),
        dryDelayBuffer.getNumChannels());

    jassert(numSamples <= delayedDryBlock.getNumSamples());

    delayedDryBlock.clear();

    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* inputData = input.getReadPointer(channel);
        auto* delayData = dryDelayBuffer.getWritePointer(channel);
        auto* dryData = delayedDryBlock.getWritePointer(channel);

        auto writePosition = dryDelayWritePosition;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            auto readPosition = writePosition - delaySamples;

            if (readPosition < 0)
                readPosition += ringSize;

            dryData[sample] = delayData[readPosition];
            delayData[writePosition] = inputData[sample];

            if (++writePosition >= ringSize)
                writePosition = 0;
        }
    }

    dryDelayWritePosition += numSamples;
    dryDelayWritePosition %= ringSize;
}

void SoundShifterProAudioProcessor::getStateInformation(
    juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void SoundShifterProAudioProcessor::setStateInformation(
    const void* data,
    int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(
                juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorEditor*
SoundShifterProAudioProcessor::createEditor()
{
    return new SoundShifterProAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoundShifterProAudioProcessor();
}
