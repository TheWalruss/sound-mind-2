#include "sound_mind/core/audio_device_list.h"

namespace sound_mind::core {

std::vector<std::string> availableAudioDeviceNames(juce::AudioDeviceManager& deviceManager, bool wantInputNames) {
    const auto& types = deviceManager.getAvailableDeviceTypes();
    if (types.isEmpty()) {
        return {};
    }

    juce::AudioIODeviceType* type = types.getFirst();
    type->scanForDevices();
    const juce::StringArray names = type->getDeviceNames(wantInputNames);

    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(names.size()));
    for (const juce::String& name : names) {
        result.push_back(name.toStdString());
    }
    return result;
}

}  // namespace sound_mind::core
