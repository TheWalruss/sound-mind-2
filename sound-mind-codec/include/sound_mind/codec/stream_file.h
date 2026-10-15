#pragma once

#include <filesystem>

#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::codec {

/**
 * @brief Writes a StreamImage to a Stream file on disk.
 *
 * Per `docs/sound-mind-architecture.md`'s Stream File Format decision: a
 * lightweight custom binary container (not TIFF-like), self-describing -
 * everything needed to decode is embedded in the file itself, matching
 * Pool's self-describing property without needing a full TIFF
 * implementation for a format whose entire reason to exist is
 * encode/decode speed rather than generic-viewer compatibility.
 *
 * @param path Destination path.
 * @param image The Stream image to write.
 * @throws std::ios_base::failure if the file can't be written.
 *
 * @note Native byte order, no endian-swapping: this project targets Arm64
 *       and x64 only (see `docs/tech-stack-decisions.md`), both
 *       little-endian. Revisit if a big-endian target is ever added.
 */
void writeStreamFile(const std::filesystem::path& path, const StreamImage& image);

/**
 * @brief Reads a StreamImage from a Stream file on disk.
 * @param path Path to the Stream file.
 * @return The decoded StreamImage - still the time-frequency
 *         representation; call decode() on the result to get audio back.
 * @throws std::ios_base::failure if the file can't be read.
 * @throws std::invalid_argument if the file isn't a valid Stream file (bad
 *         magic bytes or an unsupported format version).
 */
[[nodiscard]] StreamImage readStreamFile(const std::filesystem::path& path);

}  // namespace sound_mind::codec
