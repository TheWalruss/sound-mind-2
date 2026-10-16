#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/blend_mode.h"
#include "sound_mind/core/paste_operation.h"

namespace sound_mind::core {

/**
 * @brief Rendered text glyphs as plain coverage values - what the Studio's
 *        font rasterizer produces for the Text tool (Pre-Beta Plan
 *        `v0.Y.78.1`), kept free of any font or GUI library so the step
 *        from glyphs to canvas content can live (and be tested) here.
 */
struct GlyphRaster {
    /// @brief Columns - one per canvas frame the text covers.
    std::uint32_t width = 0;
    /// @brief Rows - one per canvas bin the text covers.
    std::uint32_t height = 0;
    /// @brief Coverage in `[0, 1]`, row-major, row `0` at the **top** (the
    ///        highest frequency), as an image is laid out - exactly
    ///        `width * height` values.
    std::vector<float> coverage;
};

/// @brief Coverage (times opacity) below this writes nothing - the
///        transparent background around and between the glyphs.
inline constexpr float kTextCoverageThreshold = 1.0f / 64.0f;

/// @brief The level a glyph cell is written at where nothing is to be
///        written (outside the glyphs, excluded by the paste's mask anyway).
inline constexpr float kTextSilenceDb = -96.0f;

/**
 * @brief Turns rendered text into a paste on the canvas: a `PasteOperation`
 *        whose clip is the glyphs and whose mask covers only the glyph
 *        cells, so everything around and between the letters is left
 *        untouched whatever the blend mode.
 *
 * Each covered cell is written at its channel's intensity, made quieter by
 * its coverage times `opacity` (`intensity + 20*log10(coverage*opacity)`),
 * so antialiased glyph edges fade out instead of stepping. Phase is zero.
 * The raster's bottom-left cell lands at (`frameOrigin`, `binOrigin`);
 * cells past the canvas are clipped when the paste is applied.
 *
 * @param id The new operation's id.
 * @param layer The layer the text is written onto.
 * @param raster The rendered glyphs.
 * @param frameOrigin The frame (column) of the raster's left edge.
 * @param binOrigin The bin (row) of the raster's bottom edge.
 * @param leftIntensityDb The left channel's level where coverage is full.
 * @param rightIntensityDb The right channel's level where coverage is full.
 * @param opacity Scales every cell's amplitude, in `[0, 1]`.
 * @param blendMode How the text combines with what's already there.
 * @param config The layer content's codec configuration (Hz/time mapping).
 * @return The paste, or `nullptr` if no cell reaches the coverage
 *         threshold (empty or fully transparent text).
 */
[[nodiscard]] std::unique_ptr<PasteOperation> makeTextPaste(OperationId id, LayerId layer, const GlyphRaster& raster,
                                                            int frameOrigin, int binOrigin, float leftIntensityDb,
                                                            float rightIntensityDb, float opacity,
                                                            BlendMode blendMode,
                                                            const sound_mind::codec::StreamCodecConfig& config);

}  // namespace sound_mind::core
