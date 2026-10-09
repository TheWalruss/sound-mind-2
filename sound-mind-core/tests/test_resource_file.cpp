#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/resource_file.h"
#include "sound_mind/core/tool_configuration.h"

using namespace sound_mind::core;

namespace {

/// @brief A fresh, unique scratch path under the system temp directory -
/// every test below writes a real file, so each needs its own name to
/// avoid colliding with another test running in parallel.
std::filesystem::path scratchPath(const std::string& nameWithExtension) {
    return std::filesystem::temp_directory_path() / ("sound-mind-resource-file-test-" + nameWithExtension);
}

}  // namespace

TEST_CASE("exportMindWave/importMindWave round-trip a NamedMindWave, dropping its id", "[core][resource_file]") {
    NamedMindWave original;
    original.id = 42;
    original.name = "My Fractal";
    original.wave.setType(GeneratorType::Fractal);
    original.wave.setAxis(MindWaveAxis::Time);

    const std::filesystem::path path = scratchPath("mindwave.smwave");
    exportMindWave(original, path);
    const NamedMindWave restored = importMindWave(path);
    std::filesystem::remove(path);

    REQUIRE(restored.id == 0);
    REQUIRE(restored.name == original.name);
    REQUIRE(restored.wave.type() == original.wave.type());
    REQUIRE(restored.wave.axis() == original.wave.axis());
}

TEST_CASE("importMindWave rejects a file with the wrong resourceType", "[core][resource_file]") {
    NamedConvolutionKernel kernel;
    kernel.name = "Not A MindWave";
    const std::filesystem::path path = scratchPath("wrong-type.smwave");
    exportConvolutionKernel(kernel, path);

    REQUIRE_THROWS_AS(importMindWave(path), std::invalid_argument);

    std::filesystem::remove(path);
}

TEST_CASE("importMindWave rejects a plain JSON file with no portable-resource envelope",
          "[core][resource_file]") {
    const std::filesystem::path path = scratchPath("not-a-resource-file.smwave");
    {
        std::ofstream file(path);
        file << nlohmann::json{{"hello", "world"}}.dump();
    }

    REQUIRE_THROWS_AS(importMindWave(path), std::invalid_argument);

    std::filesystem::remove(path);
}

TEST_CASE("exportToolPreset/importToolPreset round-trip a NamedToolPreset, dropping its id",
          "[core][resource_file]") {
    NamedToolPreset original;
    original.id = 7;
    original.name = "My Procedural Brush";
    original.config = std::make_unique<ProceduralConfiguration>();

    const std::filesystem::path path = scratchPath("toolpreset.sminst");
    exportToolPreset(original, path);
    const NamedToolPreset restored = importToolPreset(path);
    std::filesystem::remove(path);

    REQUIRE(restored.id == 0);
    REQUIRE(restored.name == original.name);
    REQUIRE(restored.config != nullptr);
    REQUIRE(restored.config->type() == original.config->type());
}

TEST_CASE("exportMindShot/importMindShot round-trip a NamedMindShot, dropping its id", "[core][resource_file]") {
    NamedMindShot original;
    original.id = 3;
    original.name = "My Capture";
    original.clip.frameCount = 2;
    original.clip.binCount = 2;
    original.clip.leftMagnitudeDb = {1.0f, 2.0f, 3.0f, 4.0f};
    original.clip.rightMagnitudeDb = {5.0f, 6.0f, 7.0f, 8.0f};
    original.clip.sharedPhaseRadians = {0.1f, 0.2f, 0.3f, 0.4f};
    original.fundamentalFrequencyHz = 440.0;
    original.startTimeOffsetSeconds = 0.05;

    const std::filesystem::path path = scratchPath("mindshot.smshot");
    exportMindShot(original, path);
    const NamedMindShot restored = importMindShot(path);
    std::filesystem::remove(path);

    REQUIRE(restored.id == 0);
    REQUIRE(restored.name == original.name);
    REQUIRE(restored.clip.frameCount == original.clip.frameCount);
    REQUIRE(restored.clip.binCount == original.clip.binCount);
    REQUIRE(restored.clip.leftMagnitudeDb == original.clip.leftMagnitudeDb);
    REQUIRE(restored.clip.rightMagnitudeDb == original.clip.rightMagnitudeDb);
    REQUIRE(restored.clip.sharedPhaseRadians == original.clip.sharedPhaseRadians);
    REQUIRE(restored.fundamentalFrequencyHz == original.fundamentalFrequencyHz);
    REQUIRE(restored.startTimeOffsetSeconds == original.startTimeOffsetSeconds);
}

TEST_CASE("exportResonanceProfile/importResonanceProfile round-trip a NamedResonanceProfile, dropping its id",
          "[core][resource_file]") {
    NamedResonanceProfile original;
    original.id = 9;
    original.name = "My Resonance";
    original.spectrum = {0.1f, 0.5f, 1.0f, 0.25f};

    const std::filesystem::path path = scratchPath("resonance.smresonance");
    exportResonanceProfile(original, path);
    const NamedResonanceProfile restored = importResonanceProfile(path);
    std::filesystem::remove(path);

    REQUIRE(restored.id == 0);
    REQUIRE(restored.name == original.name);
    REQUIRE(restored.spectrum == original.spectrum);
}

TEST_CASE(
    "exportConvolutionKernel/importConvolutionKernel round-trip a NamedConvolutionKernel, dropping its id",
    "[core][resource_file]") {
    NamedConvolutionKernel original;
    original.id = 11;
    original.name = "My Sharpen";
    original.size = 3;
    original.coefficients = {0.0f, -1.0f, 0.0f, -1.0f, 5.0f, -1.0f, 0.0f, -1.0f, 0.0f};
    original.normalize = true;

    const std::filesystem::path path = scratchPath("kernel.smfilter");
    exportConvolutionKernel(original, path);
    const NamedConvolutionKernel restored = importConvolutionKernel(path);
    std::filesystem::remove(path);

    REQUIRE(restored.id == 0);
    REQUIRE(restored.name == original.name);
    REQUIRE(restored.size == original.size);
    REQUIRE(restored.coefficients == original.coefficients);
    REQUIRE(restored.normalize == original.normalize);
}

TEST_CASE("portableResourceFileExtension returns each type's conventional extension", "[core][resource_file]") {
    REQUIRE(portableResourceFileExtension(PortableResourceType::MindWave) == ".smwave");
    REQUIRE(portableResourceFileExtension(PortableResourceType::ToolPreset) == ".sminst");
    REQUIRE(portableResourceFileExtension(PortableResourceType::MindShot) == ".smshot");
    REQUIRE(portableResourceFileExtension(PortableResourceType::ResonanceProfile) == ".smresonance");
    REQUIRE(portableResourceFileExtension(PortableResourceType::ConvolutionKernel) == ".smfilter");
}

TEST_CASE("importConvolutionKernel surfaces a missing file as std::ios_base::failure",
          "[core][resource_file]") {
    REQUIRE_THROWS_AS(importConvolutionKernel(scratchPath("does-not-exist.smfilter")), std::ios_base::failure);
}

TEST_CASE("exportToolkit/importToolkit round-trip a bundle of mixed resource types, dropping every id",
          "[core][resource_file]") {
    NamedMindWave wave;
    wave.id = 5;
    wave.name = "Bundled Wave";

    NamedConvolutionKernel kernel;
    kernel.id = 9;
    kernel.name = "Bundled Kernel";
    kernel.size = 3;
    kernel.coefficients = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    std::vector<ToolkitEntry> entries;
    entries.push_back({PortableResourceType::MindWave, nlohmann::json(wave)});
    entries.push_back({PortableResourceType::ConvolutionKernel, nlohmann::json(kernel)});

    const std::filesystem::path path = scratchPath("bundle.smtoolkit");
    exportToolkit("My Toolkit", entries, path);
    const ImportedToolkit restored = importToolkit(path);
    std::filesystem::remove(path);

    REQUIRE(restored.name == "My Toolkit");
    REQUIRE(restored.entries.size() == 2);

    REQUIRE(restored.entries[0].type == PortableResourceType::MindWave);
    const auto restoredWave = restored.entries[0].resource.get<NamedMindWave>();
    REQUIRE(restoredWave.id == 0);
    REQUIRE(restoredWave.name == wave.name);

    REQUIRE(restored.entries[1].type == PortableResourceType::ConvolutionKernel);
    const auto restoredKernel = restored.entries[1].resource.get<NamedConvolutionKernel>();
    REQUIRE(restoredKernel.id == 0);
    REQUIRE(restoredKernel.name == kernel.name);
    REQUIRE(restoredKernel.coefficients == kernel.coefficients);
}

TEST_CASE("importToolkit rejects a file with no soundMindToolkit envelope marker", "[core][resource_file]") {
    const std::filesystem::path path = scratchPath("not-a-toolkit.smtoolkit");
    {
        std::ofstream file(path);
        file << nlohmann::json{{"hello", "world"}}.dump();
    }

    REQUIRE_THROWS_AS(importToolkit(path), std::invalid_argument);

    std::filesystem::remove(path);
}

TEST_CASE("exportToolkit writes an empty entries array for an empty bundle", "[core][resource_file]") {
    const std::filesystem::path path = scratchPath("empty.smtoolkit");
    exportToolkit("Empty Toolkit", {}, path);
    const ImportedToolkit restored = importToolkit(path);
    std::filesystem::remove(path);

    REQUIRE(restored.name == "Empty Toolkit");
    REQUIRE(restored.entries.empty());
}
