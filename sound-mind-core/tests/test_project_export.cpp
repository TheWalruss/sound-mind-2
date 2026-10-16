#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numbers>

#include "sound_mind/codec/audio_file.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/video_export.h"
#include "sound_mind/core/compositor.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_export.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::CompressedAudioFormat;
using sound_mind::codec::encode;
using sound_mind::core::CompositeCancelled;
using sound_mind::core::exportProjectAudio;
using sound_mind::core::exportProjectVideo;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::renderProjectForExport;

namespace {

AudioBuffer makeSineTone(float frequencyHz, float durationSeconds, std::uint32_t sampleRateHz, float amplitude = 1.0f) {
    AudioBuffer audio;
    audio.sampleRateHz = sampleRateHz;
    const auto sampleCount = static_cast<std::size_t>(durationSeconds * static_cast<float>(sampleRateHz));
    audio.left.resize(sampleCount);
    audio.right.resize(sampleCount);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const float sample = amplitude * std::sin(2.0f * std::numbers::pi_v<float> * frequencyHz *
                                                  static_cast<float>(i) / static_cast<float>(sampleRateHz));
        audio.left[i] = sample;
        audio.right[i] = sample;
    }
    return audio;
}

float peakOf(const AudioBuffer& audio) {
    float peak = 0.0f;
    for (const float sample : audio.left) {
        peak = std::max(peak, std::abs(sample));
    }
    return peak;
}

ProjectSettings smallExportSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 64;
    return settings;
}

/// @brief Adds a Normal layer holding an encoded sine tone.
LayerId addToneLayer(Project& project, float frequencyHz, float amplitude = 1.0f) {
    const auto config = sound_mind::core::streamCodecConfigFor(project.settings());
    const LayerId id = project.addLayer(Layer(0, "Tone", LayerType::Normal));
    project.layerById(id)->setContent(
        encode(makeSineTone(frequencyHz, 0.5f, config.sampleRateHz, amplitude), config));
    return id;
}

}  // namespace

TEST_CASE("renderProjectForExport returns nullopt when no layer has content", "[core][project_export]") {
    const Project project = Project::createNew(smallExportSettings());
    CHECK_FALSE(renderProjectForExport(project).has_value());
}

TEST_CASE("renderProjectForExport mixes every visible layer, not just the topmost one", "[core][project_export]") {
    Project project = Project::createNew(smallExportSettings());
    addToneLayer(project, 1000.0f);
    // A much quieter layer above it - exporting only the topmost layer
    // with content would come out nearly silent.
    addToneLayer(project, 3000.0f, 0.001f);

    const auto media = renderProjectForExport(project);

    REQUIRE(media.has_value());
    CHECK(peakOf(media->audio) > 0.3f);
    CHECK(media->canvas.width == project.settings().canvasWidth);
}

TEST_CASE("renderProjectForExport leaves out muted layers, like Playback", "[core][project_export]") {
    Project project = Project::createNew(smallExportSettings());
    const LayerId muted = addToneLayer(project, 1000.0f);
    project.layerById(muted)->setMuted(true);

    CHECK_FALSE(renderProjectForExport(project).has_value());
}

TEST_CASE("renderProjectForExport throws CompositeCancelled when shouldCancel says so", "[core][project_export]") {
    Project project = Project::createNew(smallExportSettings());
    addToneLayer(project, 1000.0f);

    CHECK_THROWS_AS(renderProjectForExport(project, []() { return true; }), CompositeCancelled);
}

TEST_CASE("exportProjectAudio writes nothing for a project with no content", "[core][project_export]") {
    const Project project = Project::createNew(smallExportSettings());
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-project-export-empty.flac";

    CHECK_FALSE(exportProjectAudio(project, path, CompressedAudioFormat::Flac));
    CHECK_FALSE(std::filesystem::exists(path));
}

TEST_CASE("exportProjectAudio writes a real file for a project with content", "[core][project_export]") {
    Project project = Project::createNew(smallExportSettings());
    addToneLayer(project, 1000.0f);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-project-export.flac";

    const bool exported = exportProjectAudio(project, path, CompressedAudioFormat::Flac);
    const bool exists = std::filesystem::exists(path);
    std::filesystem::remove(path);

    CHECK(exported);
    CHECK(exists);
}

TEST_CASE("exportProjectVideo writes nothing for a project with no content", "[core][project_export]") {
    const Project project = Project::createNew(smallExportSettings());
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-project-export-empty.mp4";

    CHECK_FALSE(exportProjectVideo(project, path));
    CHECK_FALSE(std::filesystem::exists(path));
}

TEST_CASE("exportProjectVideo writes a real file for a project with content", "[core][project_export]") {
    Project project = Project::createNew(smallExportSettings());
    addToneLayer(project, 1000.0f);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-project-export.mp4";

    const bool exported = exportProjectVideo(project, path, /*frameRate=*/24);
    const bool exists = std::filesystem::exists(path);
    std::filesystem::remove(path);

    CHECK(exported);
    CHECK(exists);
}

TEST_CASE("exportProjectAudio limits a mix that goes over full scale, unless told not to", "[core][project_export]") {
    Project project = Project::createNew(smallExportSettings());
    // Three loud layers together sum well past full scale.
    addToneLayer(project, 500.0f, 0.9f);
    addToneLayer(project, 500.0f, 0.9f);
    addToneLayer(project, 500.0f, 0.9f);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-project-export-limited.flac";

    REQUIRE(exportProjectAudio(project, path, CompressedAudioFormat::Flac));
    const float limitedPeak = peakOf(sound_mind::codec::readAudioFile(path));
    REQUIRE(exportProjectAudio(project, path, CompressedAudioFormat::Flac, nullptr, std::nullopt));
    const float unlimitedPeak = peakOf(sound_mind::codec::readAudioFile(path));
    std::filesystem::remove(path);

    CHECK(limitedPeak <= std::pow(10.0f, -1.0f / 20.0f) + 1e-3f);
    // FLAC stores fixed point, so an unlimited over-full-scale mix clips to 1.
    CHECK(unlimitedPeak > 0.95f);
}

