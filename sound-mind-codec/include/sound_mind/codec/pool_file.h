#pragma once

#include <filesystem>

#include "sound_mind/codec/pool_codec.h"

namespace sound_mind::codec {

/**
 * @brief Writes a PoolImage to a Pool file (`.smpool`) on disk.
 *
 * Per `docs/sound-mind-architecture.md`'s Pool File Format: a standard
 * TIFF 6.0 container, four 16-bit grayscale pages (left amplitude, right
 * amplitude, left phase, right phase, in that order), LZW-compressed, one
 * strip per page - "carries forward the legacy Sound Mind TIFF's proven
 * shape," informed by `docs/legacy/SOUND_MIND_TIFF_SPEC.md` but not bound
 * to it: this codec's own metadata schema (a `SoundMindPool:key=value`
 * block in the first page's `ImageDescription` tag) is designed fresh, and
 * A-weighting (a cosmetic, decode-reversible dB offset the legacy format
 * applies) isn't implemented in this first pass - amplitude is stored as
 * plain (non-A-weighted) dB, matching how StreamImage's dB values are
 * already unweighted.
 *
 * Amplitude and phase are quantized to 16-bit unsigned integers on write
 * (dB clamped to [-96, 0], phase to [-pi, +pi]) - a real, if very fine
 * (~1.5 microdB per level), precision loss on top of poolEncode()'s own
 * near-lossless transform; `readPoolFile()` reverses it exactly.
 *
 * @param path Destination path.
 * @param image The Pool image to write.
 * @throws std::ios_base::failure if the file can't be written.
 */
void writePoolFile(const std::filesystem::path& path, const PoolImage& image);

/**
 * @brief Reads a PoolImage from a Pool file on disk.
 * @param path Path to the Pool file.
 * @return The decoded PoolImage - still the time-frequency representation;
 *         call poolDecode() on the result to get audio back.
 * @throws std::ios_base::failure if the file can't be read or isn't a
 *         valid Pool file.
 */
[[nodiscard]] PoolImage readPoolFile(const std::filesystem::path& path);

}  // namespace sound_mind::codec
