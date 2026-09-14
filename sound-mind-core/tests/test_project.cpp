#include <filesystem>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/codec/pool_codec.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/project.h"

using sound_mind::codec::PoolImage;
using sound_mind::codec::StreamImage;
using sound_mind::core::FilterType;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::MindWave;
using sound_mind::core::MindWaveId;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;

TEST_CASE("A new Project has a Background layer at the bottom and an Equalizer layer at the top",
          "[core][project]") {
    const Project project = Project::createNew(ProjectSettings{});

    REQUIRE(project.layers().size() == 2);
    REQUIRE(project.layers().front().type() == LayerType::Background);
    REQUIRE(project.layers().front().name() == "Background");
    REQUIRE(project.layers().back().type() == LayerType::Equalizer);
    REQUIRE(project.layers().back().name() == "Equalizer");
}

TEST_CASE("A new Project's Equalizer layer starts as a Frequency-Axis Gradient Cut filter with no effect",
          "[core][project]") {
    const Project project = Project::createNew(ProjectSettings{});
    const auto& config = project.layers().back().filterConfiguration();

    REQUIRE(config.type() == FilterType::FrequencyAxisGradient);
    const auto& stops = config.frequencyGradient().stops();
    // Intensity pinned to the silence floor on both stops/channels (the
    // Equalizer's own "Cut" only ever cuts toward silence) - opacity
    // ("Cut") left at 0 on both, so a fresh Equalizer has no audible
    // effect until a Cut is deliberately raised.
    for (const auto& stop : stops) {
        CHECK(stop.leftIntensity == -96.0f);
        CHECK(stop.rightIntensity == -96.0f);
        CHECK(stop.leftOpacity == 0.0f);
        CHECK(stop.rightOpacity == 0.0f);
    }
}

TEST_CASE("A new Project has no operations logged yet", "[core][project]") {
    const Project project = Project::createNew(ProjectSettings{});
    REQUIRE(project.operationLog().size() == 0);
}

TEST_CASE("A new Project carries the settings it was created with", "[core][project]") {
    ProjectSettings settings;
    settings.canvasWidth = 640;
    settings.canvasHeight = 480;

    const Project project = Project::createNew(settings);

    REQUIRE(project.settings().canvasWidth == 640);
    REQUIRE(project.settings().canvasHeight == 480);
}

TEST_CASE("A Project round-trips through JSON", "[core][project]") {
    ProjectSettings settings;
    settings.canvasWidth = 800;
    const Project original = Project::createNew(settings);

    const nlohmann::json json = original;
    const Project restored = json.get<Project>();

    REQUIRE(restored.settings().canvasWidth == original.settings().canvasWidth);
    REQUIRE(restored.layers().size() == original.layers().size());
    REQUIRE(restored.layers().front().id() == original.layers().front().id());
    REQUIRE(restored.layers().front().type() == original.layers().front().type());
}

TEST_CASE("A Project round-trips through a file on disk", "[core][project]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-project.smproj";

    ProjectSettings settings;
    settings.canvasWidth = 1234;
    const Project original = Project::createNew(settings);
    original.save(path);

    const Project restored = Project::load(path);

    REQUIRE(restored.settings().canvasWidth == 1234);
    REQUIRE(restored.layers().size() == 2);
    REQUIRE(restored.layers().front().name() == "Background");
    REQUIRE(restored.layers().back().name() == "Equalizer");

    std::filesystem::remove(path);
}

TEST_CASE("addLayer appends a layer with a fresh, unique id, just below the Equalizer", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const auto backgroundId = project.layers().front().id();
    const auto equalizerId = project.layers().back().id();

    const auto newId = project.addLayer(Layer(999, "Imported", LayerType::Normal));

    REQUIRE(project.layers().size() == 3);
    CHECK(newId != backgroundId);
    CHECK(newId != equalizerId);
    // Inserted just below the Equalizer - see addLayer()'s own docs -
    // not unconditionally at the very top.
    CHECK(project.layers().at(1).id() == newId);
    CHECK(project.layers().at(1).name() == "Imported");
    CHECK(project.layers().back().id() == equalizerId);
}

TEST_CASE("addLayer keeps the Equalizer at the top across multiple additions", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const auto equalizerId = project.layers().back().id();

    project.addLayer(Layer(0, "First", LayerType::Normal));
    project.addLayer(Layer(0, "Second", LayerType::Normal));
    project.addLayer(Layer(0, "Third", LayerType::Normal));

    REQUIRE(project.layers().size() == 5);
    CHECK(project.layers().back().id() == equalizerId);
    CHECK(project.layers().at(1).name() == "First");
    CHECK(project.layers().at(2).name() == "Second");
    CHECK(project.layers().at(3).name() == "Third");
}

TEST_CASE("layerById finds the layer with a matching id", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const auto newId = project.addLayer(Layer(999, "Imported", LayerType::Normal));

    const Layer* found = project.layerById(newId);

    REQUIRE(found != nullptr);
    CHECK(found->name() == "Imported");
}

TEST_CASE("layerById returns nullptr for an unknown id", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});

    REQUIRE(project.layerById(LayerId{999}) == nullptr);
}

TEST_CASE("layerById's mutable overload allows in-place edits", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const auto backgroundId = project.layers().front().id();

    Layer* found = project.layerById(backgroundId);
    REQUIRE(found != nullptr);
    found->setOpacity(0.5f);

    CHECK(project.layers().front().opacity() == 0.5f);
}

TEST_CASE("removeLayer removes the layer with the given id and returns true", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const auto newId = project.addLayer(Layer(999, "Imported", LayerType::Normal));
    REQUIRE(project.layers().size() == 3);

    const bool removed = project.removeLayer(newId);

    REQUIRE(removed);
    REQUIRE(project.layers().size() == 2);
    REQUIRE(project.layers().front().type() == LayerType::Background);
    REQUIRE(project.layers().back().type() == LayerType::Equalizer);
}

TEST_CASE("removeLayer returns false and changes nothing for an unknown id", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});

    const bool removed = project.removeLayer(999999);

    REQUIRE_FALSE(removed);
    REQUIRE(project.layers().size() == 2);
}

TEST_CASE("A new Project has no MindWaves", "[core][project]") {
    const Project project = Project::createNew(ProjectSettings{});
    REQUIRE(project.mindWaves().empty());
}

TEST_CASE("addMindWave appends a named MindWave with a fresh, unique id", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});

    const MindWaveId firstId = project.addMindWave("Slow Pulse", MindWave{});
    const MindWaveId secondId = project.addMindWave("Fast Pulse", MindWave{});

    REQUIRE(firstId != secondId);
    REQUIRE(project.mindWaves().size() == 2);
    REQUIRE(project.mindWaves()[0].id == firstId);
    REQUIRE(project.mindWaves()[0].name == "Slow Pulse");
    REQUIRE(project.mindWaves()[1].id == secondId);
    REQUIRE(project.mindWaves()[1].name == "Fast Pulse");
}

TEST_CASE("mindWaveById finds the entry with a matching id", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const MindWaveId id = project.addMindWave("Slow Pulse", MindWave{});

    const auto* found = project.mindWaveById(id);

    REQUIRE(found != nullptr);
    REQUIRE(found->name == "Slow Pulse");
}

TEST_CASE("mindWaveById returns nullptr for an unknown id", "[core][project]") {
    const Project project = Project::createNew(ProjectSettings{});
    REQUIRE(project.mindWaveById(MindWaveId{999}) == nullptr);
}

TEST_CASE("mindWaveById's mutable overload allows in-place edits", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const MindWaveId id = project.addMindWave("Slow Pulse", MindWave{});

    auto* found = project.mindWaveById(id);
    REQUIRE(found != nullptr);
    found->name = "Renamed";
    found->wave.setPeriod(5.0);

    REQUIRE(project.mindWaveById(id)->name == "Renamed");
    REQUIRE(project.mindWaveById(id)->wave.period() == 5.0);
}

TEST_CASE("removeMindWave removes the entry with the given id and returns true", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const MindWaveId id = project.addMindWave("Slow Pulse", MindWave{});

    const bool removed = project.removeMindWave(id);

    REQUIRE(removed);
    REQUIRE(project.mindWaves().empty());
}

TEST_CASE("removeMindWave returns false and changes nothing for an unknown id", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    project.addMindWave("Slow Pulse", MindWave{});

    const bool removed = project.removeMindWave(MindWaveId{999999});

    REQUIRE_FALSE(removed);
    REQUIRE(project.mindWaves().size() == 1);
}

TEST_CASE("A Project's MindWave library round-trips through JSON", "[core][project]") {
    Project original = Project::createNew(ProjectSettings{});
    MindWave wave;
    wave.setPeriod(2.5);
    const MindWaveId id = original.addMindWave("Slow Pulse", wave);

    const nlohmann::json json = original;
    const Project restored = json.get<Project>();

    REQUIRE(restored.mindWaves().size() == 1);
    REQUIRE(restored.mindWaves()[0].id == id);
    REQUIRE(restored.mindWaves()[0].name == "Slow Pulse");
    REQUIRE(restored.mindWaves()[0].wave.period() == 2.5);
}

TEST_CASE("A Project loads from JSON missing mindWaves (a project saved before v0.Y.31.1 "
          "Installment C1) with an empty library",
          "[core][project]") {
    Project original = Project::createNew(ProjectSettings{});
    nlohmann::json json = original;
    json.erase("mindWaves");

    const Project restored = json.get<Project>();

    REQUIRE(restored.mindWaves().empty());
}

TEST_CASE("reorderLayers applies a valid permutation of the current layer ids", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const auto backgroundId = project.layers().front().id();
    const auto equalizerId = project.layers().back().id();
    const auto middleId = project.addLayer(Layer(0, "Middle", LayerType::Normal));
    const auto topId = project.addLayer(Layer(0, "Top", LayerType::Normal));

    // reorderLayers() itself has no Equalizer-position enforcement -
    // that's a UI-level rule (see its own docs) - so every layer,
    // Equalizer included, must appear exactly once in the requested order.
    const bool ok = project.reorderLayers({backgroundId, topId, middleId, equalizerId});

    REQUIRE(ok);
    REQUIRE(project.layers().at(0).id() == backgroundId);
    REQUIRE(project.layers().at(1).id() == topId);
    REQUIRE(project.layers().at(2).id() == middleId);
    REQUIRE(project.layers().at(3).id() == equalizerId);
}

TEST_CASE("reorderLayers rejects an order that's missing a layer id, changing nothing", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const auto backgroundId = project.layers().front().id();
    const auto newId = project.addLayer(Layer(0, "Imported", LayerType::Normal));

    const bool ok = project.reorderLayers({backgroundId});  // missing newId and the Equalizer.

    REQUIRE_FALSE(ok);
    REQUIRE(project.layers().size() == 3);
    REQUIRE(project.layers().at(1).id() == newId);
}

TEST_CASE("reorderLayers rejects an order with an id that isn't a current layer, changing nothing",
          "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const auto backgroundId = project.layers().front().id();

    const bool ok = project.reorderLayers({backgroundId, 999999});

    REQUIRE_FALSE(ok);
    REQUIRE(project.layers().size() == 2);
}

TEST_CASE("reorderLayers rejects an order with a duplicated id, changing nothing", "[core][project]") {
    // A duplicate must not be allowed to silently stand in for a missing
    // id - that would corrupt the stack (losing a real layer) rather than
    // being rejected outright.
    Project project = Project::createNew(ProjectSettings{});
    const auto backgroundId = project.layers().front().id();
    const auto newId = project.addLayer(Layer(0, "Imported", LayerType::Normal));

    const bool ok = project.reorderLayers({backgroundId, backgroundId});  // newId missing, backgroundId doubled.

    REQUIRE_FALSE(ok);
    REQUIRE(project.layers().size() == 3);
    REQUIRE(project.layers().at(1).id() == newId);
}

TEST_CASE("A layer's cached content round-trips through a project file's media folder", "[core][project]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-project-with-media.smproj";
    const auto projectFolder = std::filesystem::temp_directory_path() / "sound-mind-test-project-with-media";
    std::filesystem::remove_all(projectFolder);

    Project original = Project::createNew(ProjectSettings{});
    StreamImage content;
    content.config.binCount = 8;
    content.frameCount = 4;
    content.leftMagnitudeDb.assign(32, -10.0f);
    content.rightMagnitudeDb.assign(32, -20.0f);
    content.sharedPhaseRadians.assign(32, 1.0f);

    Layer imported(0, "Imported", LayerType::Normal);
    imported.setContent(content);
    const auto importedId = original.addLayer(std::move(imported));

    original.save(path);
    const Project restored = Project::load(path);

    // Not restored.layers().back() - the Equalizer occupies that slot now.
    const Layer* restoredLayerPtr = restored.layerById(importedId);
    REQUIRE(restoredLayerPtr != nullptr);
    const Layer& restoredLayer = *restoredLayerPtr;
    REQUIRE(restoredLayer.content().has_value());
    CHECK(restoredLayer.content()->config.binCount == 8);
    CHECK(restoredLayer.content()->frameCount == 4);
    CHECK(restoredLayer.content()->leftMagnitudeDb == content.leftMagnitudeDb);
    CHECK(restoredLayer.content()->rightMagnitudeDb == content.rightMagnitudeDb);
    CHECK(restoredLayer.content()->sharedPhaseRadians == content.sharedPhaseRadians);

    // The Background layer never had content set, so it should round-trip
    // with none - no phantom media file should have been created for it.
    CHECK_FALSE(restored.layers().front().content().has_value());

    std::filesystem::remove(path);
    std::filesystem::remove_all(projectFolder);
}

TEST_CASE("A layer's cached Pool content round-trips through a project file's pool folder", "[core][project]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-project-with-pool.smproj";
    const auto projectFolder = std::filesystem::temp_directory_path() / "sound-mind-test-project-with-pool";
    std::filesystem::remove_all(projectFolder);

    Project original = Project::createNew(ProjectSettings{});
    PoolImage content;
    content.config.binCount = 8;
    content.frameCount = 4;
    content.leftMagnitudeDb.assign(32, -10.0f);
    content.rightMagnitudeDb.assign(32, -20.0f);
    content.leftPhaseRadians.assign(32, 1.0f);
    content.rightPhaseRadians.assign(32, -1.0f);

    Layer imported(0, "Imported", LayerType::Normal);
    imported.setPoolContent(content);
    const auto importedId = original.addLayer(std::move(imported));

    original.save(path);
    const Project restored = Project::load(path);

    // Not restored.layers().back() - the Equalizer occupies that slot now.
    const Layer* restoredLayerPtr = restored.layerById(importedId);
    REQUIRE(restoredLayerPtr != nullptr);
    const Layer& restoredLayer = *restoredLayerPtr;
    REQUIRE(restoredLayer.poolContent().has_value());
    CHECK(restoredLayer.poolContent()->config.binCount == 8);
    CHECK(restoredLayer.poolContent()->frameCount == 4);
    // A Pool file quantizes to 16-bit integers (see writePoolFile()'s docs)
    // - a real, if very fine, precision loss on top of the in-memory
    // values, unlike Stream's raw-float media files. Compare within that
    // tolerance rather than expecting bit-exact equality.
    constexpr float kDbTolerance = 96.0f / 65535.0f;
    constexpr float kPhaseTolerance = 2.0f * 3.14159265f / 65535.0f;
    for (std::size_t i = 0; i < content.leftMagnitudeDb.size(); ++i) {
        CHECK(restoredLayer.poolContent()->leftMagnitudeDb[i] == Catch::Approx(content.leftMagnitudeDb[i]).margin(kDbTolerance));
        CHECK(restoredLayer.poolContent()->rightMagnitudeDb[i] == Catch::Approx(content.rightMagnitudeDb[i]).margin(kDbTolerance));
        CHECK(restoredLayer.poolContent()->leftPhaseRadians[i] == Catch::Approx(content.leftPhaseRadians[i]).margin(kPhaseTolerance));
        CHECK(restoredLayer.poolContent()->rightPhaseRadians[i] == Catch::Approx(content.rightPhaseRadians[i]).margin(kPhaseTolerance));
    }

    CHECK_FALSE(restored.layers().front().poolContent().has_value());

    std::filesystem::remove(path);
    std::filesystem::remove_all(projectFolder);
}
