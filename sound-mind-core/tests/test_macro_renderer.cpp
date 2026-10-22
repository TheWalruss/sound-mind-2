#include <cmath>
#include <filesystem>
#include <memory>
#include <numbers>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/compositor.h"
#include "sound_mind/core/macro_renderer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::core::HistoryEntry;
using sound_mind::core::Layer;
using sound_mind::core::LayerBase;
using sound_mind::core::LayerId;
using sound_mind::core::Macro;
using sound_mind::core::MacroEvent;
using sound_mind::core::MacroEventKind;
using sound_mind::core::MacroRenderer;
using sound_mind::core::MacroRenderOptions;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;

namespace {

ProjectSettings smallSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 100;  // one second at 10 ms.
    settings.binCount = 64;
    settings.canvasHeight = 64;
    return settings;
}

/// A tone layer: recipe {"kind": "tone", "hz": f}.
sound_mind::codec::StreamImage tone(const ProjectSettings& settings, double hz) {
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    sound_mind::codec::AudioBuffer audio;
    audio.sampleRateHz = config.sampleRateHz;
    const std::size_t samples = static_cast<std::size_t>(settings.canvasWidth) * config.hopLength;
    for (std::size_t i = 0; i < samples; ++i) {
        const float value = 0.3f * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * i / config.sampleRateHz));
        audio.left.push_back(value);
        audio.right.push_back(value);
    }
    return sound_mind::codec::encode(audio, config);
}

MacroRenderOptions toneOptions() {
    MacroRenderOptions options;
    options.limiterCeilingDb.reset();
    options.runRecipe = [](const nlohmann::json& recipe, LayerId, const Project& project,
                           const sound_mind::core::ReplayRenderedLookup&, std::string*) -> std::optional<LayerBase> {
        return LayerBase{std::make_shared<const sound_mind::codec::StreamImage>(
                             tone(project.settings(), recipe.at("hz").get<double>())),
                         recipe, 0};
    };
    return options;
}

LayerId addTone(Project& project, double hz) {
    Layer layer(0, "Tone", sound_mind::core::LayerType::Normal);
    layer.setBaseContent(tone(project.settings(), hz), nlohmann::json{{"kind", "tone"}, {"hz", hz}});
    return project.addLayer(std::move(layer));
}

double rms(const sound_mind::codec::AudioBuffer& audio) {
    double sum = 0.0;
    for (const float value : audio.left) {
        sum += value * value;
    }
    return audio.left.empty() ? 0.0 : std::sqrt(sum / audio.left.size());
}

/// The project's composite audio, as the renderer would decode it.
sound_mind::codec::AudioBuffer playedBy(const Project& project) {
    return sound_mind::codec::decode(*sound_mind::core::compositeProject(project, nullptr, true));
}

}  // namespace

TEST_CASE("A Macro renders one segment per change, each complete from the moment it was made", "[core][macro_render]") {
    Project live = Project::createNew(smallSettings());
    Macro macro;
    macro.settings = live.settings();
    macro.historyStartsAtCreation = true;
    addTone(live, 440.0);
    live.history().commit(live, "Imported");  // during the Macro, at 0.5 s.
    macro.events = {{0.0, MacroEventKind::Play, std::nullopt, 0.0},
                    {0.5, MacroEventKind::Entry, live.history().entry(0), 0.0}};
    macro.durationSeconds = 0.9;

    MacroRenderer renderer(live.settings(), {}, macro, toneOptions());
    std::string error;
    REQUIRE(renderer.prepare(&error));
    const auto first = renderer.next(&error);
    REQUIRE(first.has_value());
    REQUIRE(first->startSeconds == 0.0);
    REQUIRE(first->durationSeconds == 0.5);
    REQUIRE(rms(first->audio) < 1e-3);  // nothing imported yet: silence.
    const auto second = renderer.next(&error);
    REQUIRE(second.has_value());
    REQUIRE(second->startSeconds == 0.5);
    REQUIRE(std::abs(second->durationSeconds - 0.4) < 1e-9);
    REQUIRE(rms(second->audio) > 0.003);  // the tone (quiet at 64 bins), not silence.
    REQUIRE(second->audio.frameCount() == static_cast<std::size_t>(std::llround(0.4 * 44100)));
    REQUIRE_FALSE(renderer.next(&error).has_value());
    REQUIRE(error.empty());
}

TEST_CASE("Rendered audio follows the playhead through a seek", "[core][macro_render]") {
    Project live = Project::createNew(smallSettings());
    addTone(live, 300.0);
    live.history().commit(live, "Imported");
    Macro macro;
    macro.settings = live.settings();
    macro.historyStartsAtCreation = true;
    macro.startAfterEntry = live.history().entry(0).id;
    macro.events = {{0.0, MacroEventKind::Play, std::nullopt, 0.25}, {0.2, MacroEventKind::Seek, std::nullopt, 0.6}};
    macro.durationSeconds = 0.3;

    MacroRenderer renderer(live.settings(), {live.history().entry(0)}, macro, toneOptions());
    REQUIRE(renderer.prepare());
    const auto segment = renderer.next();
    REQUIRE(segment.has_value());
    const auto played = playedBy(live);
    const std::size_t rate = 44100;
    // Before the seek: from 0.25 s; after it: from 0.6 s.
    REQUIRE(segment->audio.left[100] == played.left[static_cast<std::size_t>(0.25 * rate) + 100]);
    const std::size_t afterSeek = static_cast<std::size_t>(0.2 * rate) + 50;
    REQUIRE(segment->audio.left[afterSeek] == played.left[static_cast<std::size_t>(0.6 * rate) + 50]);
}

TEST_CASE("A recorded undo is replayed, and its redo, without remaking anything", "[core][macro_render]") {
    Project live = Project::createNew(smallSettings());
    addTone(live, 500.0);
    live.history().commit(live, "Imported");
    Macro macro;
    macro.settings = live.settings();
    macro.historyStartsAtCreation = true;
    const HistoryEntry imported = live.history().entry(0);
    macro.events = {{0.0, MacroEventKind::Play, std::nullopt, 0.0},
                    {0.1, MacroEventKind::Entry, imported, 0.0},
                    {0.2, MacroEventKind::Undo, imported, 0.0},
                    {0.3, MacroEventKind::Redo, imported, 0.0}};
    macro.durationSeconds = 0.4;

    int recipeRuns = 0;
    MacroRenderOptions options = toneOptions();
    const auto runner = options.runRecipe;
    options.runRecipe = [&](const nlohmann::json& recipe, LayerId layer, const Project& project,
                            const sound_mind::core::ReplayRenderedLookup& rendered, std::string* error) {
        ++recipeRuns;
        return runner(recipe, layer, project, rendered, error);
    };
    MacroRenderer renderer(live.settings(), {}, macro, std::move(options));
    REQUIRE(renderer.prepare());
    std::vector<double> levels;
    while (auto segment = renderer.next()) {
        levels.push_back(rms(segment->audio));
    }
    REQUIRE(levels.size() == 4);
    REQUIRE(levels[0] < 1e-3);
    REQUIRE(levels[1] > 0.003);
    REQUIRE(levels[2] < 1e-3);  // undone.
    REQUIRE(levels[3] > 0.003);  // redone.
    REQUIRE(recipeRuns == 1);   // the redo reused what the import made.
}

TEST_CASE("Changes made at the same moment land together", "[core][macro_render]") {
    Project live = Project::createNew(smallSettings());
    addTone(live, 300.0);
    live.history().commit(live, "A");
    addTone(live, 700.0);
    live.history().commit(live, "B");
    Macro macro;
    macro.settings = live.settings();
    macro.historyStartsAtCreation = true;
    macro.events = {{0.0, MacroEventKind::Play, std::nullopt, 0.0},
                    {0.3, MacroEventKind::Entry, live.history().entry(0), 0.0},
                    {0.3, MacroEventKind::Entry, live.history().entry(1), 0.0}};
    macro.durationSeconds = 0.5;
    MacroRenderer renderer(live.settings(), {}, macro, toneOptions());
    REQUIRE(renderer.prepare());
    int segments = 0;
    while (renderer.next()) {
        ++segments;
    }
    REQUIRE(segments == 2);
}

TEST_CASE("exportMacroVideo writes an MP4 of the performance", "[core][macro_render]") {
    Project live = Project::createNew(smallSettings());
    addTone(live, 440.0);
    live.history().commit(live, "Imported");
    Macro macro;
    macro.settings = live.settings();
    macro.historyStartsAtCreation = true;
    macro.events = {{0.0, MacroEventKind::Play, std::nullopt, 0.0},
                    {0.3, MacroEventKind::Entry, live.history().entry(0), 0.0}};
    macro.durationSeconds = 0.8;
    MacroRenderer renderer(live.settings(), {}, macro, toneOptions());
    REQUIRE(renderer.prepare());
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-macro-video.mp4";
    std::filesystem::remove(path);
    double reached = 0.0;
    std::string error;
    REQUIRE(sound_mind::core::exportMacroVideo(renderer, path, {}, [&](double seconds) { reached = seconds; }, &error));
    REQUIRE(error.empty());
    REQUIRE(reached == 0.8);
    REQUIRE(std::filesystem::file_size(path) > 1000);
    std::filesystem::remove(path);
}

TEST_CASE("A cancelled render stops with a reason", "[core][macro_render]") {
    Project live = Project::createNew(smallSettings());
    Macro macro;
    macro.settings = live.settings();
    macro.historyStartsAtCreation = true;
    macro.events = {{0.0, MacroEventKind::Play, std::nullopt, 0.0}};
    macro.durationSeconds = 0.5;
    MacroRenderer renderer(live.settings(), {}, macro, toneOptions());
    REQUIRE(renderer.prepare());
    renderer.setShouldCancel([]() { return true; });
    std::string error;
    REQUIRE_FALSE(renderer.next(&error).has_value());
    REQUIRE(error == "Cancelled.");
}
