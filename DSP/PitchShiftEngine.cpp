#include "PitchShiftEngine.h"

void PitchShiftEngine::prepare(const juce::dsp::ProcessSpec& newSpec)
{
    spec = newSpec;
    activeChannels = juce::jlimit(1,
                                  SoundShifterDSP::Config::maxChannels,
                                  static_cast<int>(spec.numChannels));

    for (int channel = 0; channel < SoundShifterDSP::Config::maxChannels; ++channel)
    {
        inputRings[static_cast<size_t>(channel)].prepare(SoundShifterDSP::Config::fftSize);
        fftProcessors[static_cast<size_t>(channel)].prepare(spec.sampleRate);
        inputFrames[static_cast<size_t>(channel)].assign(
            static_cast<size_t>(SoundShifterDSP::Config::fftSize), 0.0f);
        outputFrames[static_cast<size_t>(channel)].assign(
            static_cast<size_t>(SoundShifterDSP::Config::fftSize), 0.0f);
    }

    overlapAdd.prepare(activeChannels);
    prepared = true;
    reset();
}

void PitchShiftEngine::reset() noexcept
{
    for (int channel = 0; channel < SoundShifterDSP::Config::maxChannels; ++channel)
    {
        inputRings[static_cast<size_t>(channel)].reset();
        fftProcessors[static_cast<size_t>(channel)].reset();
        transientDetectors[static_cast<size_t>(channel)].reset();

        std::fill(inputFrames[static_cast<size_t>(channel)].begin(),
                  inputFrames[static_cast<size_t>(channel)].end(), 0.0f);

        std::fill(outputFrames[static_cast<size_t>(channel)].begin(),
                  outputFrames[static_cast<size_t>(channel)].end(), 0.0f);
    }

    overlapAdd.reset();
    samplesUntilFrame = SoundShifterDSP::Config::fftSize;
    smoothedLeftGain = 1.0f;
    smoothedRightGain = 1.0f;
    smoothedSideGain = 1.0f;
}

void PitchShiftEngine::setPitchSemitones(float newSemitones) noexcept
{
    pitchSemitones = juce::jlimit(-12.0f, 12.0f, newSemitones);
}

void PitchShiftEngine::setFineCents(float newCents) noexcept
{
    fineCents = juce::jlimit(-100.0f, 100.0f, newCents);
}

void PitchShiftEngine::setHighQuality(bool shouldUseHighQuality) noexcept
{
    highQuality = shouldUseHighQuality;
}

void PitchShiftEngine::process(juce::AudioBuffer<float>& buffer) noexcept
{
    if (!prepared || buffer.getNumSamples() <= 0)
        return;

    const auto channelsToProcess = juce::jmin(activeChannels, buffer.getNumChannels());
    const auto numberOfSamples = buffer.getNumSamples();

    for (int sample = 0; sample < numberOfSamples; ++sample)
    {
        for (int channel = 0; channel < channelsToProcess; ++channel)
            inputRings[static_cast<size_t>(channel)].push(buffer.getSample(channel, sample));

        if (--samplesUntilFrame <= 0)
        {
            processAvailableFrames();
            samplesUntilFrame = SoundShifterDSP::Config::hopSize;
        }

        for (int channel = 0; channel < channelsToProcess; ++channel)
            buffer.setSample(channel, sample, overlapAdd.popSample(channel));

        overlapAdd.advance();
    }
}

int PitchShiftEngine::getLatencySamples() const noexcept
{
    return SoundShifterDSP::Config::fftSize;
}

void PitchShiftEngine::processAvailableFrames() noexcept
{
    const auto totalSemitones = pitchSemitones + fineCents * 0.01f;
    const auto pitchRatio = std::pow(2.0f, totalSemitones / 12.0f);

    bool allFramesReady = true;
    std::array<float, SoundShifterDSP::Config::maxChannels> transientAmounts {};

    for (int channel = 0; channel < activeChannels; ++channel)
    {
        auto& ring = inputRings[static_cast<size_t>(channel)];

        if (!ring.isFull())
        {
            allFramesReady = false;
            continue;
        }

        auto& input = inputFrames[static_cast<size_t>(channel)];

        ring.copyOldestToNewest(
            input.data(),
            SoundShifterDSP::Config::fftSize);

        transientAmounts[static_cast<size_t>(channel)] =
            SoundShifterDSP::Config::enableTransient
                ? transientDetectors[static_cast<size_t>(channel)].process(
                      input.data(),
                      SoundShifterDSP::Config::fftSize)
                : 0.0f;
    }

    if (!allFramesReady)
        return;

    if (activeChannels == 2)
    {
        const auto linkedTransient = juce::jlimit(
            0.0f,
            1.0f,
            0.65f * juce::jmax(transientAmounts[0], transientAmounts[1])
                + 0.35f * 0.5f
                  * (transientAmounts[0] + transientAmounts[1]));

        transientAmounts[0] = linkedTransient;
        transientAmounts[1] = linkedTransient;
    }

    for (int channel = 0; channel < activeChannels; ++channel)
    {
        auto& input = inputFrames[static_cast<size_t>(channel)];
        auto& output = outputFrames[static_cast<size_t>(channel)];

        fftProcessors[static_cast<size_t>(channel)].processPitchFrame(
            input.data(),
            output.data(),
            pitchRatio,
            transientAmounts[static_cast<size_t>(channel)],
            highQuality);
    }

    if (activeChannels == 2)
        applyStereoEnergyLink();

    for (int channel = 0; channel < activeChannels; ++channel)
    {
        overlapAdd.addFrame(
            channel,
            outputFrames[static_cast<size_t>(channel)].data());
    }
}

void PitchShiftEngine::applyStereoEnergyLink() noexcept
{
    constexpr int frameSize = SoundShifterDSP::Config::fftSize;
    constexpr float epsilon = 1.0e-9f;

    auto* inputLeft = inputFrames[0].data();
    auto* inputRight = inputFrames[1].data();
    auto* outputLeft = outputFrames[0].data();
    auto* outputRight = outputFrames[1].data();

    const auto inputLeftRms = calculateRms(inputLeft, frameSize);
    const auto inputRightRms = calculateRms(inputRight, frameSize);
    const auto outputLeftRms = calculateRms(outputLeft, frameSize);
    const auto outputRightRms = calculateRms(outputRight, frameSize);

    const auto inputTotal = inputLeftRms + inputRightRms + epsilon;
    const auto outputTotal = outputLeftRms + outputRightRms + epsilon;

    const auto inputLeftShare = inputLeftRms / inputTotal;
    const auto inputRightShare = inputRightRms / inputTotal;
    const auto linkedOutputEnergy = 0.5f * outputTotal;

    const auto targetLeftRms = juce::jmax(
        epsilon,
        linkedOutputEnergy * (0.35f + 1.30f * inputLeftShare));

    const auto targetRightRms = juce::jmax(
        epsilon,
        linkedOutputEnergy * (0.35f + 1.30f * inputRightShare));

    const auto rawLeftGain = juce::jlimit(
        0.70f, 1.35f, targetLeftRms / (outputLeftRms + epsilon));

    const auto rawRightGain = juce::jlimit(
        0.70f, 1.35f, targetRightRms / (outputRightRms + epsilon));

    const auto smoothing = highQuality ? 0.16f : 0.28f;

    smoothedLeftGain += smoothing * (rawLeftGain - smoothedLeftGain);
    smoothedRightGain += smoothing * (rawRightGain - smoothedRightGain);

    const auto inputCorrelation = calculateCorrelation(
        inputLeft, inputRight, frameSize);

    const auto outputCorrelation = calculateCorrelation(
        outputLeft, outputRight, frameSize);

    auto targetSideGain = 1.0f;

    if (outputCorrelation > inputCorrelation + 0.08f)
    {
        const auto collapseAmount = juce::jlimit(
            0.0f, 1.0f, (outputCorrelation - inputCorrelation) * 2.5f);

        targetSideGain = 1.0f + collapseAmount * (highQuality ? 0.28f : 0.18f);
    }
    else if (outputCorrelation < inputCorrelation - 0.18f)
    {
        const auto excessiveWidth = juce::jlimit(
            0.0f, 1.0f, (inputCorrelation - outputCorrelation) * 1.8f);

        targetSideGain = 1.0f - excessiveWidth * 0.12f;
    }

    smoothedSideGain += smoothing * (targetSideGain - smoothedSideGain);
    smoothedSideGain = juce::jlimit(0.88f, 1.28f, smoothedSideGain);

    for (int sample = 0; sample < frameSize; ++sample)
    {
        const auto left = outputLeft[sample] * smoothedLeftGain;
        const auto right = outputRight[sample] * smoothedRightGain;

        const auto mid = 0.5f * (left + right);
        const auto side = 0.5f * (left - right) * smoothedSideGain;

        outputLeft[sample] = mid + side;
        outputRight[sample] = mid - side;
    }

    const auto correctedLeftRms = calculateRms(outputLeft, frameSize);
    const auto correctedRightRms = calculateRms(outputRight, frameSize);
    const auto correctedTotal = correctedLeftRms + correctedRightRms + epsilon;

    const auto originalOutputTotal = outputTotal;
    const auto balanceGain = juce::jlimit(
        0.85f, 1.15f, originalOutputTotal / correctedTotal);

    juce::FloatVectorOperations::multiply(outputLeft, balanceGain, frameSize);
    juce::FloatVectorOperations::multiply(outputRight, balanceGain, frameSize);
}

float PitchShiftEngine::calculateRms(const float* data, int numSamples) noexcept
{
    if (data == nullptr || numSamples <= 0)
        return 0.0f;

    double sumSquares = 0.0;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto value = static_cast<double>(data[sample]);
        sumSquares += value * value;
    }

    return static_cast<float>(
        std::sqrt(sumSquares / static_cast<double>(numSamples)));
}

float PitchShiftEngine::calculateCorrelation(const float* left,
                                             const float* right,
                                             int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return 0.0f;

    double cross = 0.0;
    double leftEnergy = 0.0;
    double rightEnergy = 0.0;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto l = static_cast<double>(left[sample]);
        const auto r = static_cast<double>(right[sample]);

        cross += l * r;
        leftEnergy += l * l;
        rightEnergy += r * r;
    }

    const auto denominator = std::sqrt(leftEnergy * rightEnergy);

    if (denominator <= 1.0e-12)
        return 0.0f;

    return juce::jlimit(
        -1.0f,
        1.0f,
        static_cast<float>(cross / denominator));
}