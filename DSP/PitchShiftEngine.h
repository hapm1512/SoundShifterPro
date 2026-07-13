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
    [[nodiscard]] bool isPrepared() const noexcept { return prepared; }

private:
    void processAvailableFrames() noexcept;
    void applyStereoEnergyLink() noexcept;

    [[nodiscard]] static float calculateRms(const float* data, int numSamples) noexcept;
    [[nodiscard]] static float calculateCorrelation(const float* left,
                                                    const float* right,
                                                    int numSamples) noexcept;

    juce::dsp::ProcessSpec spec {};
    std::array<RingBuffer, SoundShifterDSP::Config::maxChannels> inputRings;
    std::array<FFTProcessor, SoundShifterDSP::Config::maxChannels> fftProcessors;
    std::array<TransientDetector, SoundShifterDSP::Config::maxChannels> transientDetectors;
    std::array<std::vector<float>, SoundShifterDSP::Config::maxChannels> inputFrames;
    std::array<std::vector<float>, SoundShifterDSP::Config::maxChannels> outputFrames;
    OverlapAdd overlapAdd;

    int samplesUntilFrame = SoundShifterDSP::Config::fftSize;
    float pitchSemitones = 0.0f;
    float fineCents = 0.0f;
    float smoothedLeftGain = 1.0f;
    float smoothedRightGain = 1.0f;
    float smoothedSideGain = 1.0f;
    bool highQuality = true;
    bool prepared = false;
    int activeChannels = 0;
};
