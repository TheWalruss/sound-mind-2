#pragma once

namespace sound_mind::studio {

/**
 * @brief A gradient stop intensity (dB), as the 0-255 byte a `QColor`
 *        picker's own red/green channel shows for it.
 *
 * `color_mapping.cpp`'s own dB<->display-byte range (`kDisplayMinDb`/
 * `kDisplayMaxDb`, `-96`/`0`) - not exported from there (an anonymous-
 * namespace implementation detail of `sound-mind-codec`, which
 * `sound-mind-studio` doesn't depend on for this), so re-derived here -
 * the same "Codec's own formula is in a private header" precedent
 * `sound-mind-core`'s own `paint_application.cpp` already set (see
 * `docs/sound-mind-architecture.md`'s Decision #36). Shared by every Studio
 * color picker that needs this exact conversion (`ToolConfigurationPanel`'s
 * own brush Color swatch, `MainWindow`'s own Fill Selection action) rather
 * than each re-deriving it a second, easily-divergent time.
 *
 * @param db The intensity to convert, in dB.
 * @return The corresponding 0-255 byte.
 */
[[nodiscard]] int dbToDisplayByte(float db) noexcept;

/// @brief The exact inverse of dbToDisplayByte().
/// @param value The byte to convert, in `[0, 255]`.
/// @return The corresponding intensity, in dB.
[[nodiscard]] float displayByteToDb(int value) noexcept;

}  // namespace sound_mind::studio
