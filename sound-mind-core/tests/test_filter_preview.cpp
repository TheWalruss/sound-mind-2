#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/filter_preview.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::core::FilterConfiguration;
using sound_mind::core::FilterPreviewRequest;
using sound_mind::core::FilterPreviewSelection;
using sound_mind::core::FilterType;
using sound_mind::core::filterPreviewWindow;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::renderFilterPreview;
using sound_mind::core::TimeFrequencyRect;

namespace {

ProjectSettings previewSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 128;
    return settings;
}

double canvasSecondsOf(const Project& project) {
    const auto config = sound_mind::core::streamCodecConfigFor(project.settings());
    return static_cast<double>(project.settings().canvasWidth) * config.hopLength / config.sampleRateHz;
}

/// @brief Adds a Normal layer holding a 1 kHz tone, equal in both channels, filling the canvas.
LayerId addToneLayer(Project& project) {
    const auto config = sound_mind::core::streamCodecConfigFor(project.settings());
    AudioBuffer audio;
    audio.sampleRateHz = config.sampleRateHz;
    const auto samples = static_cast<std::size_t>(canvasSecondsOf(project) * config.sampleRateHz);
    audio.left.resize(samples);
    for (std::size_t i = 0; i < samples; ++i) {
        audio.left[i] = 0.5f * std::sin(2.0f * std::numbers::pi_v<float> * 1000.0f * static_cast<float>(i) /
                                         static_cast<float>(config.sampleRateHz));
    }
    audio.right = audio.left;
    const LayerId id = project.addLayer(Layer(0, "Tone", LayerType::Normal));
    project.layerById(id)->setContent(sound_mind::codec::encode(audio, config));
    return id;
}

double rmsOf(const std::vector<float>& samples) {
    double sum = 0.0;
    for (const float sample : samples) {
        sum += static_cast<double>(sample) * sample;
    }
    return samples.empty() ? 0.0 : std::sqrt(sum / static_cast<double>(samples.size()));
}

/// @brief Everything to the left channel - the right one goes silent.
FilterConfiguration allLeft() {
    FilterConfiguration config;
    config.setType(FilterType::ChannelBalance);
    config.setChannelBalance(0.0f);
    return config;
}

FilterPreviewRequest wholeCanvasRequest(const Project& project, FilterConfiguration config) {
    FilterPreviewRequest request;
    request.config = std::move(config);
    request.startSeconds = 0.0;
    request.endSeconds = canvasSecondsOf(project);
    return request;
}

}  // namespace

TEST_CASE("filterPreviewWindow loops a few seconds from the playback position", "[core][filter_preview]") {
    const auto [start, end] = filterPreviewWindow(1.0, std::nullopt, 10.0);
    CHECK(start == Catch::Approx(1.0));
    CHECK(end == Catch::Approx(1.0 + sound_mind::core::kDefaultFilterPreviewSeconds));
}

TEST_CASE("filterPreviewWindow moves back to end at the canvas end", "[core][filter_preview]") {
    const auto [start, end] = filterPreviewWindow(8.0, std::nullopt, 10.0);
    CHECK(start == Catch::Approx(10.0 - sound_mind::core::kDefaultFilterPreviewSeconds));
    CHECK(end == Catch::Approx(10.0));
}

TEST_CASE("filterPreviewWindow loops a canvas shorter than the default length whole", "[core][filter_preview]") {
    const auto [start, end] = filterPreviewWindow(1.0, std::nullopt, 2.0);
    CHECK(start == Catch::Approx(0.0));
    CHECK(end == Catch::Approx(2.0));
}

TEST_CASE("filterPreviewWindow uses a selection's time range, lengthening a short one", "[core][filter_preview]") {
    TimeFrequencyRect selection{2.0, 3.5, 100.0, 1000.0};
    auto [start, end] = filterPreviewWindow(7.0, selection, 10.0);
    CHECK(start == Catch::Approx(2.0));
    CHECK(end == Catch::Approx(3.5));

    selection = TimeFrequencyRect{4.0, 4.1, 100.0, 1000.0};
    std::tie(start, end) = filterPreviewWindow(0.0, selection, 10.0);
    CHECK(start == Catch::Approx(4.0));
    CHECK(end == Catch::Approx(4.0 + sound_mind::core::kMinimumFilterPreviewSeconds));

    selection = TimeFrequencyRect{-1.0, 12.0, 100.0, 1000.0};
    std::tie(start, end) = filterPreviewWindow(0.0, selection, 10.0);
    CHECK(start == Catch::Approx(0.0));
    CHECK(end == Catch::Approx(10.0));
}

TEST_CASE("renderFilterPreview applies the configuration to the whole mix with no target", "[core][filter_preview]") {
    Project project = Project::createNew(previewSettings());
    addToneLayer(project);

    const AudioBuffer plain = renderFilterPreview(project, wholeCanvasRequest(project, FilterConfiguration{}));
    const AudioBuffer filtered = renderFilterPreview(project, wholeCanvasRequest(project, allLeft()));

    REQUIRE(plain.frameCount() > 0);
    CHECK(rmsOf(plain.right) == Catch::Approx(rmsOf(plain.left)).epsilon(0.05));
    CHECK(rmsOf(filtered.left) > 0.1);
    CHECK(rmsOf(filtered.right) < 0.01 * rmsOf(filtered.left));
}

TEST_CASE("renderFilterPreview cuts out the window as whole frames, the same length every time", "[core][filter_preview]") {
    Project project = Project::createNew(previewSettings());
    addToneLayer(project);
    const auto config = sound_mind::core::streamCodecConfigFor(project.settings());
    FilterPreviewRequest request = wholeCanvasRequest(project, allLeft());
    request.startSeconds = 0.25;
    request.endSeconds = 0.75;

    const AudioBuffer first = renderFilterPreview(project, request);
    request.config = FilterConfiguration{};
    const AudioBuffer second = renderFilterPreview(project, request);

    const auto frames = static_cast<std::size_t>(std::lround(0.5 * config.sampleRateHz / config.hopLength));
    CHECK(first.frameCount() == frames * config.hopLength);
    CHECK(second.frameCount() == first.frameCount());
    CHECK(first.sampleRateHz == config.sampleRateHz);
}

TEST_CASE("renderFilterPreview fades both ends so the loop seam doesn't click", "[core][filter_preview]") {
    Project project = Project::createNew(previewSettings());
    addToneLayer(project);
    const AudioBuffer audio = renderFilterPreview(project, wholeCanvasRequest(project, FilterConfiguration{}));

    REQUIRE(audio.frameCount() > 2);
    CHECK(std::abs(audio.left.front()) < 1e-3f);
    CHECK(std::abs(audio.left.back()) < 1e-3f);
    CHECK(std::abs(audio.right.back()) < 1e-3f);
}

TEST_CASE("renderFilterPreview hears a selected Filter layer with the in-progress configuration", "[core][filter_preview]") {
    Project project = Project::createNew(previewSettings());
    addToneLayer(project);
    Layer filterLayer(0, "Balance", LayerType::Filter);
    FilterConfiguration centered;
    centered.setType(FilterType::ChannelBalance);
    centered.setChannelBalance(0.5f);
    filterLayer.setFilterConfiguration(centered);
    const LayerId filterId = project.addLayer(std::move(filterLayer));

    FilterPreviewRequest request = wholeCanvasRequest(project, allLeft());
    request.filterLayer = filterId;
    const AudioBuffer audio = renderFilterPreview(project, request);

    CHECK(rmsOf(audio.right) < 0.01 * rmsOf(audio.left));
    // The project itself is untouched - only its copy was reconfigured.
    CHECK(project.layerById(filterId)->filterConfiguration().channelBalance() == Catch::Approx(0.5f));
}

TEST_CASE("renderFilterPreview bakes the configuration into the selection only", "[core][filter_preview]") {
    Project project = Project::createNew(previewSettings());
    const LayerId toneId = addToneLayer(project);
    const double canvasSeconds = canvasSecondsOf(project);
    const auto& settings = project.settings();

    FilterPreviewRequest request = wholeCanvasRequest(project, allLeft());
    request.selection = FilterPreviewSelection{
        toneId, TimeFrequencyRect{0.0, canvasSeconds, settings.minFrequencyHz, settings.maxFrequencyHz}, std::nullopt};
    const AudioBuffer inside = renderFilterPreview(project, request);
    CHECK(rmsOf(inside.right) < 0.01 * rmsOf(inside.left));

    // A selection in the second half only: the first half plays unchanged.
    request.selection->bounds.startTimeSeconds = canvasSeconds / 2.0;
    request.endSeconds = canvasSeconds / 2.0 - 0.05;
    const AudioBuffer outside = renderFilterPreview(project, request);
    CHECK(rmsOf(outside.right) == Catch::Approx(rmsOf(outside.left)).epsilon(0.05));
}

TEST_CASE("renderFilterPreview returns an empty buffer for a project with no content", "[core][filter_preview]") {
    const Project project = Project::createNew(previewSettings());
    CHECK(renderFilterPreview(project, wholeCanvasRequest(project, allLeft())).frameCount() == 0);
}
