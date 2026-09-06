#include <filesystem>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/project.h"

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
