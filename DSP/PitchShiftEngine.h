#pragma once

#include <JuceHeader.h>

// Milestone 1: transparent pass-through engine.
// The real pitch-shifting algorithm is added in Milestone 2.
class PitchShiftEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& newSpec)
    {
        spec = newSpec;
        reset();
    }

    void reset() noexcept
    {
    }

    void setPitchSemitones(float newSemitones) noexcept
    {
        pitchSemitones = juce::jlimit(-12.0f, 12.0f, newSemitones);
    }

    void setFineCents(float newCents) noexcept
    {
        fineCents = juce::jlimit(-100.0f, 100.0f, newCents);
    }

    void process(juce::AudioBuffer<float>& buffer) noexcept
    {
        juce::ignoreUnused(buffer, spec, pitchSemitones, fineCents);
        // Intentionally bypassed in Milestone 1.
    }

private:
    juce::dsp::ProcessSpec spec {};
    float pitchSemitones = 0.0f;
    float fineCents = 0.0f;
};
