#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

#include <QImage>

#include "sound_mind/codec/audio_export.h"
#include "sound_mind/studio/image_scale_picker_dialog.h"

namespace sound_mind::studio {

/**
 * @brief `path`'s extension, lowercased.
 *
 * Extracted out of `MainWindow` as part of the Phase 2.5 Refactor & Clean
 * Up milestone (`v0.Y.23.1`) - shared by `dropEvent()`/`handleDroppedFiles()`
 * to recognize a dropped file's type case-insensitively (`.PNG` and
 * `.png` are the same file type).
 *
 * @param path The path to extract an extension from.
 * @return The extension, including its leading `.`, lowercased.
 */
[[nodiscard]] std::string lowercasedExtension(const std::filesystem::path& path);

/**
 * @brief Whether `lowercaseExtension` is one of the image extensions
 *        `ImageScalePickerDialog`/`MainWindow::importImageFiles()` accept.
 * @param lowercaseExtension An extension as lowercasedExtension() returns
 *        it - including its leading `.`.
 * @return `true` for `.png`/`.jpg`/`.jpeg`/`.bmp`/`.tga`/`.webp`.
 */
[[nodiscard]] bool isImageExtension(const std::string& lowercaseExtension);

/**
 * @brief Maps a destination path's extension to a compressed audio format.
 * @param path The destination path to inspect - its extension is
 *        recognized case-insensitively, via lowercasedExtension().
 * @return The matching format, or `std::nullopt` for an unrecognized
 *         extension.
 */
[[nodiscard]] std::optional<sound_mind::codec::CompressedAudioFormat> audioFormatFromExtension(
    const std::filesystem::path& path);

/**
 * @brief Zero-pads a snippet index to four digits ("0000", "0001", ...) -
 *        matching the legacy Studio's own `name_0000`/`name_0001`/...
 *        naming convention for a multi-snippet audio import.
 * @param index The snippet index to format.
 * @return The zero-padded index; wider than four digits if `index` itself
 *         is (never truncated).
 */
[[nodiscard]] std::string formatSnippetIndex(std::size_t index);

/**
 * @brief Resizes `source` to the project's canvas dimensions according to
 *        `mode` - see `ImageScalePickerDialog::Mode`'s own docs for
 *        exactly what each value means.
 *
 * `Qt::IgnoreAspectRatio` is used throughout, including for
 * `ScaleVerticalProportional`, since that mode's own proportional width is
 * already computed by hand internally - asking Qt to *also* fit an aspect
 * ratio on top would risk a slightly different rounding than the one this
 * function's own docs (via `ImageScalePickerDialog::Mode`'s) promise.
 *
 * @param source The image to resize.
 * @param mode Which of the five resize behaviors to apply.
 * @param canvasWidth The project's canvas width, in pixels.
 * @param canvasHeight The project's canvas height, in pixels.
 * @return The resized image.
 */
[[nodiscard]] QImage scaleImageForImport(const QImage& source, ImageScalePickerDialog::Mode mode, int canvasWidth,
                                          int canvasHeight);

}  // namespace sound_mind::studio
