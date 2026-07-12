#pragma once

#include <JuceHeader.h>
#include "../DSP/PitchShiftEngine.h"

class SoundShifterProAudioProcessor final : public juce::AudioProcessor
{
public:
    SoundShifterProAudioProcessor();
    ~SoundShifterProAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    const juce::AudioProcessorValueTreeState& getAPVTS() const noexcept { return apvts; }

    float getInputLeftDb() const noexcept { return inputLeftDb.load(); }
    float getInputRightDb() const noexcept { return inputRightDb.load(); }
    float getOutputLeftDb() const noexcept { return outputLeftDb.load(); }
    float getOutputRightDb() const noexcept { return outputRightDb.load(); }
    double getCurrentSampleRateHz() const noexcept { return currentSampleRate.load(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void handleMidiControl(const juce::MidiBuffer& midiMessages);
    void changePitchBySemitones(float amount);
    void setPitchFromMidi(float semitones);
    void prepareDryDelay(int channels, int maximumBlockSize);
    void createDelayedDry(const juce::AudioBuffer<float>& input, int numSamples);
    juce::AudioProcessorValueTreeState apvts;
    PitchShiftEngine pitchShiftEngine;

    juce::LinearSmoothedValue<float> outputGainLinear;
    juce::AudioBuffer<float> dryDelayBuffer;
    juce::AudioBuffer<float> delayedDryBlock;
    int dryDelayWritePosition = 0;
    std::array<bool, 128> ccPressed {};
    std::atomic<float> inputLeftDb { -100.0f };
    std::atomic<float> inputRightDb { -100.0f };
    std::atomic<float> outputLeftDb { -100.0f };
    std::atomic<float> outputRightDb { -100.0f };
    std::atomic<double> currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundShifterProAudioProcessor)
};
