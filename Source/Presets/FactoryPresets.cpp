#include "FactoryPresets.h"

namespace SoundShifterFactoryPresets
{
    const std::vector<Preset>& getPresets()
    {
        static const std::vector<Preset> presets {
            { "Default",       "Utility",   0.0f,   0.0f, 100.0f,  0.0f, true, false },
            { "Tone Up",       "Utility",   1.0f,   0.0f, 100.0f,  0.0f, true, false },
            { "Tone Down",     "Utility",  -1.0f,   0.0f, 100.0f,  0.0f, true, false },
            { "Harmony Up",    "Creative",  7.0f,   0.0f,  70.0f, -1.0f, true, false },
            { "Harmony Down",  "Creative", -5.0f,   0.0f,  70.0f, -1.0f, true, false },
            { "Octave Up",     "Creative", 12.0f,   0.0f, 100.0f, -2.0f, true, false },
            { "Octave Down",   "Creative",-12.0f,   0.0f, 100.0f, -2.0f, true, false },
            { "Detune Wide",   "Creative",  0.0f,  18.0f,  55.0f, -0.5f, true, false },
            { "Fast Preview",  "Utility",   0.0f,   0.0f, 100.0f,  0.0f, false, false }
        };

        return presets;
    }

    const Preset* findByName(const juce::String& name) noexcept
    {
        const auto& presets = getPresets();

        const auto iterator = std::find_if(
            presets.begin(),
            presets.end(),
            [&name](const Preset& preset)
            {
                return preset.name.equalsIgnoreCase(name);
            });

        return iterator != presets.end() ? &*iterator : nullptr;
    }
}
