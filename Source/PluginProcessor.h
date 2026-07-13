#pragma once

#include <JuceHeader.h>
#include "../DSP/PitchShiftEngine.h"

class SoundShifterProAudioProcessor final : public juce::AudioProcessor
{
public:
    enum class MidiLearnTarget
    {
        none,
        pitchDown,
        pitchUp,
        pitchReset
    };

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

    void toneUp();
    void toneDown();
    void toneReset();
    void setPitchSemitones(float semitones);
    [[nodiscard]] float getPitchSemitones() const noexcept;

    void setMix(float percent);
    [[nodiscard]] float getMix() const noexcept;

    void setOutputGain(float decibels);
    [[nodiscard]] float getOutputGain() const noexcept;

    void setHighQuality(bool enabled);
    [[nodiscard]] bool getHighQuality() const noexcept;

    void setBypass(bool enabled);
    [[nodiscard]] bool getBypass() const noexcept;

    void beginMidiLearn(MidiLearnTarget target) noexcept;
    void cancelMidiLearn() noexcept;
    [[nodiscard]] bool isMidiLearning() const noexcept;
    [[nodiscard]] MidiLearnTarget getMidiLearnTarget() const noexcept;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void cacheParameterPointers() noexcept;
    void handleMidiControl(const juce::MidiBuffer& midiMessages);
    void changePitchBySemitones(float amount);
    void setPitchFromMidi(float semitones);
    void setParameterValue(const char* parameterId, float plainValue);

    void prepareDryDelay(int channels, int maximumBlockSize);
    void createDelayedDry(const juce::AudioBuffer<float>& input, int numSamples);
    void applyDryWetMix(juce::AudioBuffer<float>& wetBuffer,
                        int numSamples,
                        float requestedMix,
                        bool bypassed);
    void applyOutputGain(juce::AudioBuffer<float>& buffer, int numSamples);
    void updateMeters(const juce::AudioBuffer<float>& buffer,
                      bool inputMeters) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    PitchShiftEngine pitchShiftEngine;

    std::atomic<float>* pitchParameter = nullptr;
    std::atomic<float>* fineParameter = nullptr;
    std::atomic<float>* mixParameter = nullptr;
    std::atomic<float>* outputParameter = nullptr;
    std::atomic<float>* bypassParameter = nullptr;
    std::atomic<float>* hqParameter = nullptr;
    std::atomic<float>* midiPitchDownCcParameter = nullptr;
    std::atomic<float>* midiPitchUpCcParameter = nullptr;
    std::atomic<float>* midiPitchResetCcParameter = nullptr;

    juce::LinearSmoothedValue<float> outputGainLinear;
    juce::LinearSmoothedValue<float> mixSmoothed;
    juce::LinearSmoothedValue<float> bypassWetSmoothed;

    juce::AudioBuffer<float> dryDelayBuffer;
    juce::AudioBuffer<float> delayedDryBlock;
    int dryDelayWritePosition = 0;

    std::array<bool, 128> ccPressed {};
    std::atomic<MidiLearnTarget> midiLearnTarget { MidiLearnTarget::none };

    std::atomic<float> inputLeftDb { -100.0f };
    std::atomic<float> inputRightDb { -100.0f };
    std::atomic<float> outputLeftDb { -100.0f };
    std::atomic<float> outputRightDb { -100.0f };
    std::atomic<double> currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundShifterProAudioProcessor)
};
