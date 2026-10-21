#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/codec/audio_file.h"
#include "sound_mind/codec/stream_file.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/project_sources.h"

using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::ProjectSource;

namespace {

std::filesystem::path freshFolder(const std::string& name) {
    const auto folder = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    return folder;
}

void writeText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream(path, std::ios::binary) << text;
}

ProjectSettings smallSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 16;
    settings.canvasHeight = 8;
    return settings;
}

}  // namespace

TEST_CASE("sha256Hex matches the standard test vectors", "[core][sources]") {
    REQUIRE(sound_mind::core::sha256Hex("", 0) ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    REQUIRE(sound_mind::core::sha256Hex("abc", 3) ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const std::string long56 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    REQUIRE(sound_mind::core::sha256Hex(long56.data(), long56.size()) ==
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("sha256OfFile hashes a file's bytes", "[core][sources]") {
    const auto folder = freshFolder("sound-mind-test-sources-hash");
    writeText(folder / "abc.txt", "abc");
    REQUIRE(sound_mind::core::sha256OfFile(folder / "abc.txt") ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    std::filesystem::remove_all(folder);
}

TEST_CASE("storeSourceFile copies a file in under its hash, once", "[core][sources]") {
    const auto folder = freshFolder("sound-mind-test-sources-store");
    writeText(folder / "Song.wav", "not really audio");
    const ProjectSource first = sound_mind::core::storeSourceFile(folder / "sources", folder / "Song.wav", "audio");
    REQUIRE(first.kind == "audio");
    REQUIRE(first.originalName == "Song.wav");
    REQUIRE(first.fileName == first.sha256.substr(0, 16) + ".wav");
    REQUIRE(std::filesystem::exists(folder / "sources" / first.fileName));

    const ProjectSource again = sound_mind::core::storeSourceFile(folder / "sources", folder / "Song.wav", "audio");
    REQUIRE(again.fileName == first.fileName);
    std::filesystem::remove_all(folder);
}

TEST_CASE("storeSourceAudio writes a lossless FLAC that reads back", "[core][sources]") {
    const auto folder = freshFolder("sound-mind-test-sources-flac");
    sound_mind::codec::AudioBuffer audio;
    audio.sampleRateHz = 44100;
    for (int i = 0; i < 4410; ++i) {
        const float sample = (i % 100) / 200.0f - 0.25f;
        audio.left.push_back(sample);
        audio.right.push_back(-sample);
    }
    const ProjectSource source = sound_mind::core::storeSourceAudio(folder, audio, "recording", "Take");
    REQUIRE(source.kind == "recording");
    REQUIRE(source.fileName.ends_with(".flac"));
    const auto readBack = sound_mind::codec::readAudioFile(folder / source.fileName);
    REQUIRE(readBack.frameCount() == audio.frameCount());
    REQUIRE(std::abs(readBack.left[150] - audio.left[150]) < 1e-3f);
    std::filesystem::remove_all(folder);
}

TEST_CASE("A project registers each source once and finds its file", "[core][sources]") {
    const auto folder = freshFolder("sound-mind-test-sources-registry");
    Project project = Project::createNew(smallSettings());
    REQUIRE_FALSE(project.sourcesFolder().has_value());
    project.setFolderForProjectFile(folder / "Piece.smproj");
    REQUIRE(project.sourcesFolder() == folder / "Piece" / "sources");

    writeText(folder / "a.png", "image bytes");
    const auto stored = sound_mind::core::storeSourceFile(*project.sourcesFolder(), folder / "a.png", "image");
    const auto id = project.addSource(stored);
    REQUIRE(project.addSource(stored) == id);  // the same file again: reused.
    REQUIRE(project.sources().size() == 1);
    REQUIRE(project.sourceById(id)->originalName == "a.png");
    REQUIRE(project.sourcePath(id) == *project.sourcesFolder() / stored.fileName);
    REQUIRE_FALSE(project.sourcePath(id + 1).has_value());
    std::filesystem::remove_all(folder);
}

TEST_CASE("Sources are saved with the project and follow it on Save As", "[core][sources]") {
    const auto folder = freshFolder("sound-mind-test-sources-save-as");
    Project project = Project::createNew(smallSettings());
    project.save(folder / "First.smproj");
    writeText(folder / "clip.wav", "audio bytes");
    const auto id = project.addSource(
        sound_mind::core::storeSourceFile(*project.sourcesFolder(), folder / "clip.wav", "audio"));
    project.save(folder / "First.smproj");

    project.save(folder / "Second.smproj");  // Save As.
    REQUIRE(project.folder() == folder / "Second");
    REQUIRE(std::filesystem::exists(*project.sourcePath(id)));

    const Project loaded = Project::load(folder / "Second.smproj");
    REQUIRE(loaded.sources().size() == 1);
    REQUIRE(loaded.sources().front().kind == "audio");
    REQUIRE(std::filesystem::exists(*loaded.sourcePath(id)));
    std::filesystem::remove_all(folder);
}
