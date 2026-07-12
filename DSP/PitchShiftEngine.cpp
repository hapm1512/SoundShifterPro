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
        fftProcessors[static_cast<size_t>(channel)].prepare();
        frameScratch[static_cast<size_t>(channel)].assign(
            static_cast<size_t>(SoundShifterDSP::Config::fftSize), 0.0f);
    }

    overlapAdd.prepare(activeChannels);
    dryScratch.prepare(activeChannels, static_cast<int>(spec.maximumBlockSize));
    prepared = true;
    reset();
}

void PitchShiftEngine::reset() noexcept
{
    for (int channel = 0; channel < SoundShifterDSP::Config::maxChannels; ++channel)
    {
        inputRings[static_cast<size_t>(channel)].reset();
        fftProcessors[static_cast<size_t>(channel)].reset();
        std::fill(frameScratch[static_cast<size_t>(channel)].begin(),
                  frameScratch[static_cast<size_t>(channel)].end(),
                  0.0f);
        samplesUntilAnalysis[static_cast<size_t>(channel)] = SoundShifterDSP::Config::fftSize;
    }

    overlapAdd.reset();
    dryScratch.reset();
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

    captureAnalysisFrames(buffer);

    // Milestone 2B deliberately leaves the audio untouched.
    // FFT analysis infrastructure is active and allocation-free here.
    juce::ignoreUnused(pitchSemitones, fineCents, highQuality);
}

int PitchShiftEngine::getAnalysisLatencySamples() const noexcept
{
    return SoundShifterDSP::Config::fftSize - SoundShifterDSP::Config::hopSize;
}

void PitchShiftEngine::captureAnalysisFrames(const juce::AudioBuffer<float>& buffer) noexcept
{
    const auto channelsToProcess = juce::jmin(activeChannels, buffer.getNumChannels());
    const auto samples = buffer.getNumSamples();

    for (int channel = 0; channel < channelsToProcess; ++channel)
    {
        const auto* source = buffer.getReadPointer(channel);
        auto& ring = inputRings[static_cast<size_t>(channel)];
        auto& countdown = samplesUntilAnalysis[static_cast<size_t>(channel)];

        for (int sample = 0; sample < samples; ++sample)
        {
            ring.push(source[sample]);
            --countdown;

            if (countdown <= 0)
            {
                if (ring.isFull())
                    analyseAvailableFrame(channel);

                countdown = SoundShifterDSP::Config::hopSize;
            }
        }
    }
}

void PitchShiftEngine::analyseAvailableFrame(int channel) noexcept
{
    auto& scratch = frameScratch[static_cast<size_t>(channel)];
    inputRings[static_cast<size_t>(channel)].copyOldestToNewest(
        scratch.data(),
        SoundShifterDSP::Config::fftSize);
    fftProcessors[static_cast<size_t>(channel)].analyseFrame(scratch.data());
}
