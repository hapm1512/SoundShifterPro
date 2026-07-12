#pragma once

#include <JuceHeader.h>
#include "AudioBlock.h"
#include "DSPConfig.h"
#include "FFTProcessor.h"
#include "OverlapAdd.h"
#include "RingBuffer.h"

class PitchShiftEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& newSpec);
    void reset() noexcept;

    void setPitchSemitones(float newSemitones) noexcept;
    void setFineCents(float newCents) noexcept;
    void setHighQuality(bool shouldUseHighQuality) noexcept;

    void process(juce::AudioBuffer<float>& buffer) noexcept;

    [[nodiscard]] int getAnalysisLatencySamples() const noexcept;
    [[nodiscard]] bool isPrepared() const noexcept { return prepared; }

private:
    void captureAnalysisFrames(const juce::AudioBuffer<float>& buffer) noexcept;
    void analyseAvailableFrame(int channel) noexcept;

    juce::dsp::ProcessSpec spec {};
    std::array<RingBuffer, SoundShifterDSP::Config::maxChannels> inputRings;
    std::array<FFTProcessor, SoundShifterDSP::Config::maxChannels> fftProcessors;
    std::array<std::vector<float>, SoundShifterDSP::Config::maxChannels> frameScratch;
    OverlapAdd overlapAdd;
    FixedAudioBlock dryScratch;

    std::array<int, SoundShifterDSP::Config::maxChannels> samplesUntilAnalysis {
        SoundShifterDSP::Config::fftSize,
        SoundShifterDSP::Config::fftSize
    };

    float pitchSemitones = 0.0f;
    float fineCents = 0.0f;
    bool highQuality = true;
    bool prepared = false;
    int activeChannels = 0;
};
