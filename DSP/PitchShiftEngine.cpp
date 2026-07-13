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
        transientStrength[static_cast<size_t>(channel)] = 0.0f;
        std::fill(inputFrames[static_cast<size_t>(channel)].begin(),
                  inputFrames[static_cast<size_t>(channel)].end(), 0.0f);
        std::fill(outputFrames[static_cast<size_t>(channel)].begin(),
                  outputFrames[static_cast<size_t>(channel)].end(), 0.0f);
    }

    overlapAdd.reset();
    samplesUntilFrame = SoundShifterDSP::Config::fftSize;
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

    for (int channel = 0; channel < activeChannels; ++channel)
    {
        auto& ring = inputRings[static_cast<size_t>(channel)];
        if (!ring.isFull())
            continue;

        auto& input = inputFrames[static_cast<size_t>(channel)];
        auto& output = outputFrames[static_cast<size_t>(channel)];

        ring.copyOldestToNewest(input.data(), SoundShifterDSP::Config::fftSize);

        if constexpr (SoundShifterDSP::Config::enableTransient)
        {
            transientStrength[static_cast<size_t>(channel)] =
                transientDetectors[static_cast<size_t>(channel)].process(
                    input.data(), SoundShifterDSP::Config::fftSize);
        }
        else
        {
            transientStrength[static_cast<size_t>(channel)] = 0.0f;
        }

        fftProcessors[static_cast<size_t>(channel)].processPitchFrame(
            input.data(), output.data(), pitchRatio);
        overlapAdd.addFrame(channel, output.data());
    }

    // Epic 3D.2 only analyses transients. Epic 3D.3 will use these
    // values to adapt phase locking and transient reconstruction.
    juce::ignoreUnused(highQuality, transientStrength);
}
