#include <filesystem>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/codec/pool_codec.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"

using sound_mind::codec::PoolImage;
using sound_mind::codec::StreamImage;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;

TEST_CASE("A new Project has exactly one Background layer", "[core][project]") {
    const Project project = Project::createNew(ProjectSettings{});

    REQUIRE(project.layers().size() == 1);
    REQUIRE(project.layers().front().type() == LayerType::Background);
    REQUIRE(project.layers().front().name() == "Background");
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
    REQUIRE(restored.layers().size() == 1);
    REQUIRE(restored.layers().front().name() == "Background");

    std::filesystem::remove(path);
}

TEST_CASE("addLayer appends a layer with a fresh, unique id", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const auto backgroundId = project.layers().front().id();

    const auto newId = project.addLayer(Layer(999, "Imported", LayerType::Normal));

    REQUIRE(project.layers().size() == 2);
    CHECK(newId != backgroundId);
    CHECK(project.layers().back().id() == newId);
    CHECK(project.layers().back().name() == "Imported");
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
    REQUIRE(project.layers().size() == 2);

    const bool removed = project.removeLayer(newId);

    REQUIRE(removed);
    REQUIRE(project.layers().size() == 1);
    REQUIRE(project.layers().front().type() == LayerType::Background);
}

TEST_CASE("removeLayer returns false and changes nothing for an unknown id", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});

    const bool removed = project.removeLayer(999999);

    REQUIRE_FALSE(removed);
    REQUIRE(project.layers().size() == 1);
}

TEST_CASE("reorderLayers applies a valid permutation of the current layer ids", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const auto backgroundId = project.layers().front().id();
    const auto middleId = project.addLayer(Layer(0, "Middle", LayerType::Normal));
    const auto topId = project.addLayer(Layer(0, "Top", LayerType::Normal));

    const bool ok = project.reorderLayers({backgroundId, topId, middleId});

    REQUIRE(ok);
    REQUIRE(project.layers().at(0).id() == backgroundId);
    REQUIRE(project.layers().at(1).id() == topId);
    REQUIRE(project.layers().at(2).id() == middleId);
}

TEST_CASE("reorderLayers rejects an order that's missing a layer id, changing nothing", "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const auto backgroundId = project.layers().front().id();
    const auto newId = project.addLayer(Layer(0, "Imported", LayerType::Normal));

    const bool ok = project.reorderLayers({backgroundId});  // missing newId.

    REQUIRE_FALSE(ok);
    REQUIRE(project.layers().size() == 2);
    REQUIRE(project.layers().at(1).id() == newId);
}

TEST_CASE("reorderLayers rejects an order with an id that isn't a current layer, changing nothing",
          "[core][project]") {
    Project project = Project::createNew(ProjectSettings{});
    const auto backgroundId = project.layers().front().id();

    const bool ok = project.reorderLayers({backgroundId, 999999});

    REQUIRE_FALSE(ok);
    REQUIRE(project.layers().size() == 1);
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
    REQUIRE(project.layers().size() == 2);
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

    const auto& restoredLayer = restored.layers().back();
    REQUIRE(restoredLayer.id() == importedId);
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

    const auto& restoredLayer = restored.layers().back();
    REQUIRE(restoredLayer.id() == importedId);
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
