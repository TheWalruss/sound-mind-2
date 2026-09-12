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
    const bool anyContent = std::any_of(
        layers.begin(), layers.end(), [](const Layer& layer) { return layer.visible() && layer.content().has_value(); });
    if (!anyContent) {
        return std::nullopt;
    }

    const auto& settings = project.settings();
    const auto config = streamCodecConfigFor(settings);
    const std::uint32_t canvasWidth = settings.canvasWidth;

    StreamImage result;
    result.config = config;
    result.frameCount = canvasWidth;
    result.sampleCount = static_cast<std::uint64_t>(canvasWidth) * config.hopLength;
    const std::size_t cellCount = std::size_t{config.binCount} * canvasWidth;
    result.leftMagnitudeDb.resize(cellCount);
    result.rightMagnitudeDb.resize(cellCount);
    result.sharedPhaseRadians.resize(cellCount);

    for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
        for (std::uint32_t x = 0; x < canvasWidth; ++x) {
            std::complex<float> leftSum(0.0f, 0.0f);
            std::complex<float> rightSum(0.0f, 0.0f);

            for (const Layer& layer : layers) {
                if (!layer.visible() || !layer.content().has_value()) {
                    continue;
                }
                const StreamImage& content = *layer.content();
                const auto sourceColumn =
                    sourceColumnFor(x, content.frameCount, layer.rescaleFactor(), layer.translationColumns());
                if (!sourceColumn.has_value()) {
                    continue;
                }

                const std::size_t sourceCell = std::size_t{bin} * content.frameCount + *sourceColumn;
                const float leftLinear = dbToLinearAmplitude(content.leftMagnitudeDb[sourceCell]) * layer.opacity();
                const float rightLinear = dbToLinearAmplitude(content.rightMagnitudeDb[sourceCell]) * layer.opacity();
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
