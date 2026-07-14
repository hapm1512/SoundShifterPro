cmake_minimum_required(VERSION 3.22)

project(SoundShifterPro VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE installation on the user's machine.
set(JUCE_DIR "D:/JUCE8" CACHE PATH "Path to JUCE")
add_subdirectory("${JUCE_DIR}" JUCE)

juce_add_plugin(SoundShifterPro
    COMPANY_NAME "Hai Pham Audio"
    BUNDLE_ID "com.haiphamaudio.soundshifterpro"
    PLUGIN_MANUFACTURER_CODE HPAu
    PLUGIN_CODE SsP1
    FORMATS VST3 Standalone
    PRODUCT_NAME "SoundShifter Pro"
    PLUGIN_NAME "SoundShifter Pro"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(SoundShifterPro PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/Presets/PresetManager.cpp
    Source/Presets/PresetManager.h
    Source/Presets/FactoryPresets.cpp
    Source/Presets/FactoryPresets.h

    DSP/DSPConfig.h
    DSP/AudioBlock.h
    DSP/RingBuffer.h
    DSP/WindowProcessor.h
    DSP/FFTProcessor.h
    DSP/PeakDetector.h
    DSP/PeakDetector.cpp
    DSP/PhaseLock.h
    DSP/PhaseLock.cpp
    DSP/IdentityPhaseLock.h
    DSP/IdentityPhaseLock.cpp
    DSP/OverlapAdd.h
    DSP/TransientDetector.h
    DSP/TransientDetector.cpp
    DSP/PitchShiftEngine.h
    DSP/PitchShiftEngine.cpp
    GUI/Theme.h
    GUI/SoundShifterLookAndFeel.h
    GUI/StereoMeter.h
)

target_compile_definitions(SoundShifterPro PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SoundShifterPro PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

juce_generate_juce_header(SoundShifterPro)
