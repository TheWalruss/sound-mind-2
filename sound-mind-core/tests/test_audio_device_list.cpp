#include <catch2/catch_test_macros.hpp>

#include <juce_audio_devices/juce_audio_devices.h>

#include "sound_mind/core/audio_device_list.h"

TEST_CASE("availableAudioDeviceNames is callable, before initialise(), and idempotent", "[audio_device_list]") {
    juce::AudioDeviceManager manager;

    // No real device/device type is guaranteed in a CI/test environment -
    // this only confirms the call never crashes (even though initialise()
    // was never called on this manager) and returns a stable, well-formed
    // list across repeated calls, not that any particular device exists.
    const auto inputsFirstCall = sound_mind::core::availableAudioDeviceNames(manager, true);
    const auto inputsSecondCall = sound_mind::core::availableAudioDeviceNames(manager, true);
    CHECK(inputsFirstCall == inputsSecondCall);

    const auto outputs = sound_mind::core::availableAudioDeviceNames(manager, false);
    CHECK(outputs == sound_mind::core::availableAudioDeviceNames(manager, false));
}
