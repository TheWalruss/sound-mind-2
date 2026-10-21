#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::core {

/// @brief Identifies one source in a project's registry; never reused.
using SourceId = std::uint64_t;

/**
 * @brief One piece of original media a project was built from
 *        (`v0.1.43.2`): an imported file, a recording, a Loop take, or a
 *        layer brought in from another project - kept in the project's
 *        `sources/` folder so replaying its History can rebuild it from the
 *        originals (`docs/sound-mind-architecture.md`'s *History, Sources
 *        and Macros*).
 */
struct ProjectSource {
    /// Its id in the project's registry; `0` until registered.
    SourceId id = 0;
    /// What it is: `"audio"`, `"image"`, `"midi"`, `"recording"`,
    /// `"loopTake"` or `"layerImage"`.
    std::string kind;
    /// The original file name (or a descriptive name for a recording),
    /// kept for provenance - UTF-8.
    std::string originalName;
    /// SHA-256 of the file's bytes, lowercase hex - also how duplicates are
    /// recognized.
    std::string sha256;
    /// The file's name inside the project's `sources/` folder - UTF-8.
    std::string fileName;

    /// @brief Field-by-field equality.
    /// @return Whether every field matches.
    bool operator==(const ProjectSource&) const = default;
};

/// @brief Serializes a source registry entry.
void to_json(nlohmann::json& json, const ProjectSource& source);

/// @brief Parses a source registry entry.
/// @throws nlohmann::json::exception on missing or malformed fields.
void from_json(const nlohmann::json& json, ProjectSource& source);

/**
 * @brief SHA-256 of a byte range.
 * @param data The bytes.
 * @param size How many.
 * @return The digest as 64 lowercase hex digits.
 */
[[nodiscard]] std::string sha256Hex(const void* data, std::size_t size);

/**
 * @brief SHA-256 of a file's contents, read in chunks.
 * @param path The file.
 * @return The digest as 64 lowercase hex digits.
 * @throws std::ios_base::failure if the file can't be read.
 */
[[nodiscard]] std::string sha256OfFile(const std::filesystem::path& path);

/**
 * @brief Copies `file` into `sourcesFolder` under a name derived from its
 *        hash, unless a file of that name is already there.
 *
 * Thread-safe (touches no project) - an import can run it off the UI
 * thread, then register the result with `Project::addSource()`.
 *
 * @param sourcesFolder The project's `sources/` folder; created if needed.
 * @param file The original file.
 * @param kind See ProjectSource::kind.
 * @return The (unregistered) source.
 * @throws std::filesystem::filesystem_error / std::ios_base::failure on
 *         I/O failure.
 */
[[nodiscard]] ProjectSource storeSourceFile(const std::filesystem::path& sourcesFolder,
                                            const std::filesystem::path& file, std::string kind);

/**
 * @brief Writes `audio` into `sourcesFolder` as a lossless FLAC file, named
 *        by its hash - how recordings and Loop takes become sources.
 *        Thread-safe, as storeSourceFile().
 * @param sourcesFolder The project's `sources/` folder; created if needed.
 * @param audio The audio.
 * @param kind See ProjectSource::kind (`"recording"` or `"loopTake"`).
 * @param name A descriptive name for provenance.
 * @return The (unregistered) source.
 * @throws std::runtime_error / std::filesystem::filesystem_error on failure.
 */
[[nodiscard]] ProjectSource storeSourceAudio(const std::filesystem::path& sourcesFolder,
                                             const sound_mind::codec::AudioBuffer& audio, std::string kind,
                                             std::string name);

/**
 * @brief Writes `image` into `sourcesFolder` as a Stream file, named by
 *        its hash - how a layer brought in from another project becomes a
 *        source. Thread-safe, as storeSourceFile().
 * @param sourcesFolder The project's `sources/` folder; created if needed.
 * @param image The layer content.
 * @param name A descriptive name for provenance.
 * @return The (unregistered) source.
 * @throws std::runtime_error / std::filesystem::filesystem_error on failure.
 */
[[nodiscard]] ProjectSource storeSourceStreamImage(const std::filesystem::path& sourcesFolder,
                                                   const sound_mind::codec::StreamImage& image, std::string name);

}  // namespace sound_mind::core
