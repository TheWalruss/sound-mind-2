#include <filesystem>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"

using sound_mind::codec::StreamImage;
using sound_mind::core::Layer;
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
