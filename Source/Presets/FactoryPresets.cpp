#include "FactoryPresets.h"

namespace SoundShifterFactoryPresets
{
    const std::vector<Preset>& getPresets()
    {
        static const std::vector<Preset> presets {
            { "Default",      "Utility",  "Hai Pham", "Neutral pitch shifting.",             0.0f,   0.0f, 100.0f,  0.0f, true,  false },
            { "Tone Up",      "Utility",  "Hai Pham", "Raises the song by one semitone.",     1.0f,   0.0f, 100.0f,  0.0f, true,  false },
            { "Tone Down",    "Utility",  "Hai Pham", "Lowers the song by one semitone.",    -1.0f,   0.0f, 100.0f,  0.0f, true,  false },
            { "Harmony Up",   "Creative", "Hai Pham", "Creates an upper harmony blend.",      7.0f,   0.0f,  70.0f, -1.0f, true,  false },
            { "Harmony Down", "Creative", "Hai Pham", "Creates a lower harmony blend.",      -5.0f,   0.0f,  70.0f, -1.0f, true,  false },
            { "Octave Up",    "Creative", "Hai Pham", "Shifts the signal one octave up.",     12.0f,   0.0f, 100.0f, -2.0f, true,  false },
            { "Octave Down",  "Creative", "Hai Pham", "Shifts the signal one octave down.",  -12.0f,   0.0f, 100.0f, -2.0f, true,  false },
            { "Detune Wide",  "Creative", "Hai Pham", "Adds subtle stereo detune width.",      0.0f,  18.0f,  55.0f, -0.5f, true,  false },
            { "Fast Preview", "Utility",  "Hai Pham", "Low CPU preview mode.",                 0.0f,   0.0f, 100.0f,  0.0f, false, false }
        };
        return presets;
    }

    const Preset* findByName(const juce::String& name) noexcept
    {
        const auto& presets = getPresets();
        const auto iterator = std::find_if(
            presets.begin(), presets.end(),
            [&name](const Preset& preset) { return preset.name.equalsIgnoreCase(name); });
        return iterator != presets.end() ? &*iterator : nullptr;
    }
}
