#include "sound_mind/core/compositor.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::core {

namespace {

using sound_mind::codec::RgbImage;
using sound_mind::codec::StreamImage;

/// @brief Nearest-neighbor horizontal resample of `source` to `newWidth`
/// columns, keeping the same height - used to apply a layer's
/// rescaleFactor() to its own timeline before placeOnCanvas() below. A
/// no-op (returns `source` unchanged) when nothing would actually change,
/// to skip the allocation on the (common) unrescaled path.
RgbImage resampleHorizontally(const RgbImage& source, std::uint32_t newWidth) {
    if (newWidth == source.width || source.width == 0) {
        return source;
    }
    RgbImage result;
    result.width = newWidth;
    result.height = source.height;
    result.pixels.resize(std::size_t{newWidth} * source.height * 3);
    for (std::uint32_t x = 0; x < newWidth; ++x) {
        const auto sourceX = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(source.width - 1, (std::uint64_t{x} * source.width) / newWidth));
        for (std::uint32_t y = 0; y < source.height; ++y) {
            const std::size_t srcIndex = (std::size_t{y} * source.width + sourceX) * 3;
            const std::size_t dstIndex = (std::size_t{y} * newWidth + x) * 3;
            result.pixels[dstIndex] = source.pixels[srcIndex];
            result.pixels[dstIndex + 1] = source.pixels[srcIndex + 1];
            result.pixels[dstIndex + 2] = source.pixels[srcIndex + 2];
        }
    }
    return result;
}

/// @brief Places `source` onto a `canvasWidth`-wide black canvas, shifted
/// by `translationColumns` (positive = later/right) - see renderLayer()'s
/// own docs for the padding/cropping this produces.
RgbImage placeOnCanvas(const RgbImage& source, std::int64_t translationColumns, std::uint32_t canvasWidth) {
    RgbImage result;
    result.width = canvasWidth;
    result.height = source.height;
    result.pixels.assign(std::size_t{canvasWidth} * source.height * 3, 0);
    for (std::uint32_t x = 0; x < canvasWidth; ++x) {
        const std::int64_t sourceX = static_cast<std::int64_t>(x) - translationColumns;
        if (sourceX < 0 || sourceX >= static_cast<std::int64_t>(source.width)) {
            continue;  // stays black - outside the layer's (shifted) content.
        }
        for (std::uint32_t y = 0; y < source.height; ++y) {
            const std::size_t srcIndex = (std::size_t{y} * source.width + static_cast<std::uint32_t>(sourceX)) * 3;
            const std::size_t dstIndex = (std::size_t{y} * canvasWidth + x) * 3;
            result.pixels[dstIndex] = source.pixels[srcIndex];
            result.pixels[dstIndex + 1] = source.pixels[srcIndex + 1];
            result.pixels[dstIndex + 2] = source.pixels[srcIndex + 2];
        }
    }
    return result;
}

/// @brief The smallest linear amplitude linearAmplitudeToDb() below will
/// ever report as anything but this floor's own dB value - avoids
/// `log10(0)` for genuine silence, the same reasoning (and the same
/// value) `sound-mind-codec`'s own private per-frame STFT helpers already
/// use for exactly this.
constexpr float kMinLinearAmplitude = 1e-7f;

/// @brief Converts a stored dB amplitude to its linear equivalent -
/// compositeProject()'s own copy of the same one-line formula
/// `sound-mind-codec`'s private `stream_frame_codec.h` already has,
/// duplicated rather than shared across the module boundary: Core is
/// allowed to depend on Codec's public `include/` headers, but not on an
/// internal, unexported `src/` implementation file, and a two-line
/// formula isn't worth promoting to a shared public header over.
[[nodiscard]] float dbToLinearAmplitude(float db) noexcept { return std::pow(10.0f, db / 20.0f); }

/// @brief The inverse of dbToLinearAmplitude() - see its own docs.
[[nodiscard]] float linearAmplitudeToDb(float amplitude) noexcept {
    return 20.0f * std::log10(std::max(amplitude, kMinLinearAmplitude));
}

/// @brief The column of a `sourceWidth`-wide layer that `outputColumn`
/// (on the project's own `canvasWidth`-wide timeline) maps to, after
/// applying `rescaleFactor` (stretches/compresses the layer's own
/// timeline, applied first) then `translationColumns` (shifts the result
/// earlier/later, applied second) - the same geometry renderLayer()'s own
/// (RGB-pixel-space) resampleHorizontally()/placeOnCanvas() apply,
/// expressed here as a single index lookup rather than two intermediate
/// image copies, since compositeProject() needs this same lookup for
/// every layer at every output cell rather than once per whole image.
///
/// @return The source column, or `std::nullopt` if `outputColumn` falls
///         outside the layer's own (rescaled, translated) content -
///         nothing to place there, matching renderLayer()'s own
///         black-padding for the exact same case.
[[nodiscard]] std::optional<std::uint32_t> sourceColumnFor(std::uint32_t outputColumn, std::uint32_t sourceWidth,
                                                              double rescaleFactor,
                                                              std::int64_t translationColumns) noexcept {
    if (sourceWidth == 0) {
        return std::nullopt;
    }
    std::uint32_t rescaledWidth = sourceWidth;
    if (rescaleFactor != 1.0) {
        const double scaled = static_cast<double>(sourceWidth) * rescaleFactor;
        rescaledWidth = static_cast<std::uint32_t>(std::max(1.0, std::round(scaled)));
    }
    const std::int64_t rescaledIndex = static_cast<std::int64_t>(outputColumn) - translationColumns;
    if (rescaledIndex < 0 || rescaledIndex >= static_cast<std::int64_t>(rescaledWidth)) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(
        sourceWidth - 1, (static_cast<std::uint64_t>(rescaledIndex) * sourceWidth) / rescaledWidth));
}

}  // namespace

std::optional<sound_mind::codec::RgbImage> renderLayer(const Layer& layer, std::uint32_t canvasWidth) {
    if (!layer.content().has_value()) {
        return std::nullopt;
    }
    const RgbImage base = sound_mind::codec::toRgbImage(*layer.content());

    std::uint32_t rescaledWidth = base.width;
    if (layer.rescaleFactor() != 1.0) {
        const double scaled = static_cast<double>(base.width) * layer.rescaleFactor();
        rescaledWidth = static_cast<std::uint32_t>(std::max(1.0, std::round(scaled)));
    }
    const RgbImage rescaled = resampleHorizontally(base, rescaledWidth);

    return placeOnCanvas(rescaled, layer.translationColumns(), canvasWidth);
}

std::optional<StreamImage> compositeProject(const Project& project) {
    const auto& layers = project.layers();
    std::vector<const Layer*> contributingLayers;
    for (const Layer& layer : layers) {
        if (layer.visible() && layer.content().has_value()) {
            contributingLayers.push_back(&layer);
        }
    }
    if (contributingLayers.empty()) {
        return std::nullopt;
    }

    const auto& settings = project.settings();
    auto config = streamCodecConfigFor(settings);
    // The project's own settings.binCount is authoritative for a project
    // whose layers were actually encoded from it (the normal case - every
    // real layer's own content already comes from streamCodecConfigFor()
    // against these same settings, so the two numbers are identical in
    // practice). But nothing enforces that in general, and it's routine
    // for a hand-built StreamImage (an in-memory test fixture, in
    // particular) to declare its own, different binCount - using every
    // contributing layer's own tallest bin range instead keeps the
    // composite matching whatever its own real content actually is,
    // rather than silently padding it to (or truncating it against) a
    // project-level number nothing here actually guarantees matches.
    std::uint32_t binCount = 0;
    for (const Layer* layer : contributingLayers) {
        binCount = std::max(binCount, layer->content()->config.binCount);
    }
    if (binCount > 0) {
        config.binCount = binCount;
    }
    const std::uint32_t canvasWidth = settings.canvasWidth;

    StreamImage result;
    result.config = config;
    result.frameCount = canvasWidth;
    result.sampleCount = static_cast<std::uint64_t>(canvasWidth) * config.hopLength;
    const std::size_t cellCount = std::size_t{config.binCount} * canvasWidth;
    result.leftMagnitudeDb.resize(cellCount);
    result.rightMagnitudeDb.resize(cellCount);
    result.sharedPhaseRadians.resize(cellCount);
    const float silenceFloorDb = linearAmplitudeToDb(0.0f);

    if (contributingLayers.size() == 1) {
        // Fast path: summing a single term is the identity, so this
        // produces exactly the same result the general path below would
        // - but skips every transcendental call (sin/cos/abs/log10) it
        // needs to actually sum *multiple* complex values, replacing
        // them with one gain shift (computed once, not per cell) plus a
        // placement copy. Worth a dedicated path specifically because
        // it's the overwhelmingly common case (most projects, and most
        // moments even within a genuinely multi-layer one, have exactly
        // one contributing layer at any given cell) and this function
        // runs on every canvas repaint - see its own docs.
        const Layer& layer = *contributingLayers.front();
        const StreamImage& content = *layer.content();
        const float gainDb = 20.0f * std::log10(std::max(layer.opacity(), kMinLinearAmplitude));
        for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
            for (std::uint32_t x = 0; x < canvasWidth; ++x) {
                const std::size_t outputCell = std::size_t{bin} * canvasWidth + x;
                // A layer's own content may have fewer bins than the
                // project's own binCount (e.g. a project reconfigured
                // since this layer was last encoded) - out-of-range bins
                // contribute nothing, the same as a column outside the
                // layer's own placed range does, rather than indexing
                // past that layer's own arrays.
                if (bin >= content.config.binCount) {
                    result.leftMagnitudeDb[outputCell] = silenceFloorDb;
                    result.rightMagnitudeDb[outputCell] = silenceFloorDb;
                    result.sharedPhaseRadians[outputCell] = 0.0f;
                    continue;
                }
                const auto sourceColumn =
                    sourceColumnFor(x, content.frameCount, layer.rescaleFactor(), layer.translationColumns());
                if (!sourceColumn.has_value()) {
                    result.leftMagnitudeDb[outputCell] = silenceFloorDb;
                    result.rightMagnitudeDb[outputCell] = silenceFloorDb;
                    result.sharedPhaseRadians[outputCell] = 0.0f;
                    continue;
                }
                const std::size_t sourceCell = std::size_t{bin} * content.frameCount + *sourceColumn;
                // dB(linear * gain) == dB(linear) + dB(gain) - the same
                // identity dbToLinearAmplitude()/linearAmplitudeToDb()
                // round-trip exactly for any value clear of the silence
                // floor, letting opacity apply as a plain addition
                // instead of a full linear round-trip.
                result.leftMagnitudeDb[outputCell] = content.leftMagnitudeDb[sourceCell] + gainDb;
                result.rightMagnitudeDb[outputCell] = content.rightMagnitudeDb[sourceCell] + gainDb;
                result.sharedPhaseRadians[outputCell] = content.sharedPhaseRadians[sourceCell];
            }
        }
        return result;
    }

    for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
        for (std::uint32_t x = 0; x < canvasWidth; ++x) {
            std::complex<float> leftSum(0.0f, 0.0f);
            std::complex<float> rightSum(0.0f, 0.0f);

            for (const Layer* layer : contributingLayers) {
                const StreamImage& content = *layer->content();
                // See the fast path's own comment above on why a
                // layer's own content may have fewer bins than the
                // project's own binCount.
                if (bin >= content.config.binCount) {
                    continue;
                }
                const auto sourceColumn =
                    sourceColumnFor(x, content.frameCount, layer->rescaleFactor(), layer->translationColumns());
                if (!sourceColumn.has_value()) {
                    continue;
                }

                const std::size_t sourceCell = std::size_t{bin} * content.frameCount + *sourceColumn;
                const float leftLinear = dbToLinearAmplitude(content.leftMagnitudeDb[sourceCell]) * layer->opacity();
                const float rightLinear =
                    dbToLinearAmplitude(content.rightMagnitudeDb[sourceCell]) * layer->opacity();
                const float phase = content.sharedPhaseRadians[sourceCell];
                const std::complex<float> direction(std::cos(phase), std::sin(phase));

                leftSum += leftLinear * direction;
                rightSum += rightLinear * direction;
            }

            const std::size_t outputCell = std::size_t{bin} * canvasWidth + x;
            result.leftMagnitudeDb[outputCell] = linearAmplitudeToDb(std::abs(leftSum));
            result.rightMagnitudeDb[outputCell] = linearAmplitudeToDb(std::abs(rightSum));
            const std::complex<float> mid = (leftSum + rightSum) / 2.0f;
            result.sharedPhaseRadians[outputCell] = (std::abs(mid) > 0.0f) ? std::arg(mid) : 0.0f;
        }
    }

    return result;
}

}  // namespace sound_mind::core
