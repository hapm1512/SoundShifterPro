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
    constexpr auto midiPitchDownCc = "midiPitchDownCc";
    constexpr auto midiPitchUpCc = "midiPitchUpCc";
    constexpr auto midiPitchResetCc = "midiPitchResetCc";
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

    parameters.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParameterIDs::midiPitchDownCc, 1 },
        "MIDI Pitch Down CC",
        0,
        127,
        30));

    parameters.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParameterIDs::midiPitchUpCc, 1 },
        "MIDI Pitch Up CC",
        0,
        127,
        31));

    parameters.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParameterIDs::midiPitchResetCc, 1 },
        "MIDI Pitch Reset CC",
        0,
        127,
        32));

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
    midiPitchDownCcParameter =
        apvts.getRawParameterValue(ParameterIDs::midiPitchDownCc);
    midiPitchUpCcParameter =
        apvts.getRawParameterValue(ParameterIDs::midiPitchUpCc);
    midiPitchResetCcParameter =
        apvts.getRawParameterValue(ParameterIDs::midiPitchResetCc);

    jassert(pitchParameter != nullptr);
    jassert(fineParameter != nullptr);
    jassert(mixParameter != nullptr);
    jassert(outputParameter != nullptr);
    jassert(bypassParameter != nullptr);
    jassert(hqParameter != nullptr);
    jassert(midiPitchDownCcParameter != nullptr);
    jassert(midiPitchUpCcParameter != nullptr);
    jassert(midiPitchResetCcParameter != nullptr);
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

    const bool highQuality =
        hqParameter == nullptr || hqParameter->load() > 0.5f;

    static bool lastHQ = true;
    if (lastHQ != highQuality)
        lastHQ = highQuality;

    static float lastPitch = 999.0f;
    static float lastFine = 999.0f;

    if (pitch != lastPitch)
    {
        pitchShiftEngine.setPitchSemitones(pitch);
        lastPitch = pitch;
    }

    if (fine != lastFine)
    {
        pitchShiftEngine.setFineCents(fine);
        lastFine = fine;
    }

    pitchShiftEngine.setHighQuality(highQuality);

    bypassWetSmoothed.setTargetValue(bypassed ? 0.0f : 1.0f);

    const bool unityPitch =
        std::abs(pitch) < 0.001f && std::abs(fine) < 0.001f;

    const auto shouldProcessWet =
        (!unityPitch)
        && (!bypassed
            || bypassWetSmoothed.isSmoothing()
            || bypassWetSmoothed.getCurrentValue() > 0.0001f);

    if (shouldProcessWet)
        pitchShiftEngine.process(buffer);

    applyDryWetMix(
        buffer,
        numSamples,
        requestedMix,
        bypassed);

    static bool lastBypassState = false;

    if (lastBypassState != bypassed)
    {
        bypassWetSmoothed.reset(currentSampleRate.load(), 0.02);
        bypassWetSmoothed.setTargetValue(bypassed ? 0.0f : 1.0f);
        lastBypassState = bypassed;
    }

    applyOutputGain(buffer, numSamples);

    if (highQuality || (numSamples >= 128))
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

    if (effectiveStart <= 0.0001f && effectiveEnd <= 0.0001f)
    {
        wetBuffer.makeCopyOf(delayedDryBlock, true);
        return;
    }

    if (effectiveStart >= 0.9999f && effectiveEnd >= 0.9999f)
        return;

    const auto startAngle =
        effectiveStart * juce::MathConstants<float>::halfPi;
    const auto endAngle =
        effectiveEnd * juce::MathConstants<float>::halfPi;

    const auto dryStart = std::cos(startAngle);
    const auto dryEnd = std::cos(endAngle);

    const auto wetStart = std::sin(startAngle);
    const auto wetEnd = std::sin(endAngle);

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

    const auto targetGain =
        juce::jlimit(
            0.0631f,
            3.9811f,
            juce::Decibels::decibelsToGain(outputDb));

    outputGainLinear.setTargetValue(targetGain);

    const auto startGain = outputGainLinear.getCurrentValue();
    const auto endGain = outputGainLinear.skip(numSamples);

    if (std::abs(startGain - 1.0f) < 0.0005f
        && std::abs(endGain - 1.0f) < 0.0005f)
        return;

    constexpr float safetyThreshold = 0.965f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        buffer.applyGainRamp(
            channel,
            0,
            numSamples,
            startGain,
            endGain);

        auto* samples = buffer.getWritePointer(channel);

        for (int i = 0; i < numSamples; ++i)
        {
            const float x = samples[i];

            if (std::abs(x) > safetyThreshold)
                samples[i] = std::tanh(x);
        }
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

void SoundShifterProAudioProcessor::toneUp()
{
    changePitchBySemitones(1.0f);
}

void SoundShifterProAudioProcessor::toneDown()
{
    changePitchBySemitones(-1.0f);
}

void SoundShifterProAudioProcessor::toneReset()
{
    setPitchFromMidi(0.0f);
}

void SoundShifterProAudioProcessor::setPitchSemitones(float semitones)
{
    setPitchFromMidi(semitones);
}

float SoundShifterProAudioProcessor::getPitchSemitones() const noexcept
{
    return pitchParameter != nullptr
        ? pitchParameter->load()
        : 0.0f;
}

void SoundShifterProAudioProcessor::setMix(float percent)
{
    setParameterValue(
        ParameterIDs::mix,
        juce::jlimit(0.0f, 100.0f, percent));
}

float SoundShifterProAudioProcessor::getMix() const noexcept
{
    return mixParameter != nullptr
        ? mixParameter->load()
        : 100.0f;
}

void SoundShifterProAudioProcessor::setOutputGain(float decibels)
{
    setParameterValue(
        ParameterIDs::output,
        juce::jlimit(-24.0f, 12.0f, decibels));
}

float SoundShifterProAudioProcessor::getOutputGain() const noexcept
{
    return outputParameter != nullptr
        ? outputParameter->load()
        : 0.0f;
}

void SoundShifterProAudioProcessor::setHighQuality(bool enabled)
{
    setParameterValue(
        ParameterIDs::hq,
        enabled ? 1.0f : 0.0f);
}

bool SoundShifterProAudioProcessor::getHighQuality() const noexcept
{
    return hqParameter == nullptr
        || hqParameter->load() > 0.5f;
}

void SoundShifterProAudioProcessor::setBypass(bool enabled)
{
    setParameterValue(
        ParameterIDs::bypass,
        enabled ? 1.0f : 0.0f);
}

bool SoundShifterProAudioProcessor::getBypass() const noexcept
{
    return bypassParameter != nullptr
        && bypassParameter->load() > 0.5f;
}

void SoundShifterProAudioProcessor::setParameterValue(
    const char* parameterId,
    float plainValue)
{
    if (auto* parameter = apvts.getParameter(parameterId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(
            parameter->convertTo0to1(plainValue));
        parameter->endChangeGesture();
    }
}

void SoundShifterProAudioProcessor::beginMidiLearn(
    MidiLearnTarget target) noexcept
{
    midiLearnTarget.store(target);
}

void SoundShifterProAudioProcessor::cancelMidiLearn() noexcept
{
    midiLearnTarget.store(MidiLearnTarget::none);
}

bool SoundShifterProAudioProcessor::isMidiLearning() const noexcept
{
    return midiLearnTarget.load() != MidiLearnTarget::none;
}

SoundShifterProAudioProcessor::MidiLearnTarget
SoundShifterProAudioProcessor::getMidiLearnTarget() const noexcept
{
    return midiLearnTarget.load();
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

            const auto learnTarget = midiLearnTarget.load();

            if (learnTarget != MidiLearnTarget::none)
            {
                const char* parameterId = nullptr;

                switch (learnTarget)
                {
                    case MidiLearnTarget::pitchDown:
                        parameterId = ParameterIDs::midiPitchDownCc;
                        break;

                    case MidiLearnTarget::pitchUp:
                        parameterId = ParameterIDs::midiPitchUpCc;
                        break;

                    case MidiLearnTarget::pitchReset:
                        parameterId = ParameterIDs::midiPitchResetCc;
                        break;

                    case MidiLearnTarget::none:
                    default:
                        break;
                }

                if (parameterId != nullptr)
                {
                    if (auto* parameter = apvts.getParameter(parameterId))
                    {
                        parameter->beginChangeGesture();
                        parameter->setValueNotifyingHost(
                            parameter->convertTo0to1(
                                static_cast<float>(cc)));
                        parameter->endChangeGesture();
                    }
                }

                midiLearnTarget.store(MidiLearnTarget::none);
                ccPressed[static_cast<size_t>(cc)] = pressed;
                continue;
            }

            const auto wasPressed =
                ccPressed[static_cast<size_t>(cc)];

            ccPressed[static_cast<size_t>(cc)] = pressed;

            if (pressed && !wasPressed)
            {
                const auto pitchDownCc = midiPitchDownCcParameter != nullptr
                    ? juce::jlimit(
                          0,
                          127,
                          juce::roundToInt(midiPitchDownCcParameter->load()))
                    : 30;

                const auto pitchUpCc = midiPitchUpCcParameter != nullptr
                    ? juce::jlimit(
                          0,
                          127,
                          juce::roundToInt(midiPitchUpCcParameter->load()))
                    : 31;

                const auto pitchResetCc = midiPitchResetCcParameter != nullptr
                    ? juce::jlimit(
                          0,
                          127,
                          juce::roundToInt(midiPitchResetCcParameter->load()))
                    : 32;

                if (cc == pitchDownCc)
                    changePitchBySemitones(-1.0f);
                else if (cc == pitchUpCc)
                    changePitchBySemitones(1.0f);
                else if (cc == pitchResetCc)
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

        int writePosition = dryDelayWritePosition;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            int readPosition = writePosition - delaySamples;
            readPosition += (readPosition < 0) ? ringSize : 0;

            dryData[sample] = delayData[readPosition];
            delayData[writePosition] = inputData[sample];

            ++writePosition;
            writePosition -= (writePosition == ringSize) ? ringSize : 0;
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
