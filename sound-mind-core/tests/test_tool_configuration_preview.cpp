#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/tool_configuration.h"
#include "sound_mind/core/tool_configuration_preview.h"

using namespace sound_mind::core;
using sound_mind::codec::decode;

TEST_CASE("toolConfigurationPreviewStreamImage paints a non-silent stroke",
          "[core][tool_configuration_preview]") {
    const Project project = Project::createNew(ProjectSettings{});
    const ProceduralConfiguration config;

    const sound_mind::codec::StreamImage image = toolConfigurationPreviewStreamImage(config, project);

    // Direct user feedback: "practically wherever there is a visual
    // preview of something, give the user the ability to play an audio
    // preview" - confirms there's real, audible content to decode (not
    // every magnitude cell left at the silence floor the backdrop
    // starts at), the same way a painted stroke always stands out
    // against it on screen.
    const bool anyAboveFloor =
        std::any_of(image.leftMagnitudeDb.begin(), image.leftMagnitudeDb.end(), [](float db) { return db > -96.0f; });
    REQUIRE(anyAboveFloor);
}

TEST_CASE("toolConfigurationPreviewStreamImage's result decodes to a real, 3-second-ish AudioBuffer",
          "[core][tool_configuration_preview]") {
    const Project project = Project::createNew(ProjectSettings{});
    const ProceduralConfiguration config;

    const sound_mind::codec::StreamImage image = toolConfigurationPreviewStreamImage(config, project);
    const sound_mind::codec::AudioBuffer audio = decode(image);

    REQUIRE(audio.frameCount() == image.sampleCount);
    REQUIRE(audio.left.size() == audio.right.size());
}

TEST_CASE("toolConfigurationPreviewStreamImage resolves a MindWave-bound Instrument's own vibrato",
          "[core][tool_configuration_preview]") {
    Project project = Project::createNew(ProjectSettings{});
    MindWave wave;
    wave.setAxis(MindWaveAxis::Time);
    const MindWaveId waveId = project.addMindWave("Vibrato Source", wave);

    InstrumentConfiguration withVibrato;
    withVibrato.setVibratoMindWave(waveId);
    InstrumentConfiguration withoutVibrato;

    const sound_mind::codec::StreamImage withImage = toolConfigurationPreviewStreamImage(withVibrato, project);
    const sound_mind::codec::StreamImage withoutImage = toolConfigurationPreviewStreamImage(withoutVibrato, project);

    // A real MindWave binding resolved (not silently ignored because the
    // preview never passed a resolver) changes the painted result -
    // doesn't assert exactly how, just that it's not byte-identical to
    // the unmodulated case.
    REQUIRE(withImage.leftMagnitudeDb != withoutImage.leftMagnitudeDb);
}
