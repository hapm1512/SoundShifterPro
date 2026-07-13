#pragma once

#include <JuceHeader.h>

#include "DSPConfig.h"
#include "FFTProcessor.h"
#include "OverlapAdd.h"
#include "RingBuffer.h"
#include "TransientDetector.h"

class PitchShiftEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& newSpec);
    void reset() noexcept;

    void setPitchSemitones(float newSemitones) noexcept;
    void setFineCents(float newCents) noexcept;
    void setHighQuality(bool shouldUseHighQuality) noexcept;

    void process(juce::AudioBuffer<float>& buffer) noexcept;

    [[nodiscard]] int getLatencySamples() const noexcept;
    [[nodiscard]] bool isPrepared() const noexcept
    {
        return prepared;
    }

private:
    void processAvailableFrames() noexcept;

    juce::dsp::ProcessSpec spec {};

    std::array<RingBuffer,
               SoundShifterDSP::Config::maxChannels>
        inputRings;

    std::array<FFTProcessor,
               SoundShifterDSP::Config::maxChannels>
        fftProcessors;

    // ===== Epic 3D =====

    std::array<TransientDetector,
               SoundShifterDSP::Config::maxChannels>
        transientDetectors;

    std::array<float,
               SoundShifterDSP::Config::maxChannels>
        transientStrength {};

    std::array<std::vector<float>,
               SoundShifterDSP::Config::maxChannels>
        inputFrames;

    std::array<std::vector<float>,
               SoundShifterDSP::Config::maxChannels>
        outputFrames;

    OverlapAdd overlapAdd;

    int samplesUntilFrame =
        SoundShifterDSP::Config::fftSize;

    float pitchSemitones = 0.0f;
    float fineCents = 0.0f;

    bool highQuality = true;
    bool prepared = false;

    int activeChannels = 0;
};