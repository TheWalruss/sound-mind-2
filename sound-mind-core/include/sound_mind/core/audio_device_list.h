#pragma once

#include <string>
#include <vector>

#include <juce_audio_devices/juce_audio_devices.h>

namespace sound_mind::core {

/**
 * @brief Lists the currently available audio device names of one direction,
 *        for `deviceManager`'s current device type.
 *
 * Shared by `PlaybackEngine`, `RecordEngine`, and `LoopEngine`'s own
 * `availableOutputDeviceNames()`/`availableInputDeviceNames()` methods -
 * per the Transport Panels milestone (`v0.Y.16.1`), all three need the
 * same JUCE device-enumeration dance (`AudioIODeviceType::scanForDevices()`
 * then `getDeviceNames()`), just for different directions.
 *
 * Only the manager's *current* device type is queried (on Windows,
 * typically WASAPI - JUCE's usual default) - not every registered type
 * (WASAPI, DirectSound, ASIO, ...) - a deliberate first-pass
 * simplification that avoids showing the same physical device multiple
 * times under different backends.
 *
 * @param deviceManager The manager to query - safe to call whether or not
 *        it has ever had `initialise()` called on it; `getAvailableDeviceTypes()`
 *        populates its device type list lazily, on first use, regardless.
 * @param wantInputNames `true` for input device names, `false` for output.
 * @return Device names, in the order JUCE reports them - empty if no
 *         device type is available at all, or none of that direction
 *         exist (e.g. no microphone attached, when asking for inputs).
 */
[[nodiscard]] std::vector<std::string> availableAudioDeviceNames(juce::AudioDeviceManager& deviceManager,
                                                                   bool wantInputNames);

}  // namespace sound_mind::core
