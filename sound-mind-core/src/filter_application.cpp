#include "sound_mind/core/filter_application.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <map>
#include <numbers>
#include <tuple>
#include <utility>
#include <vector>

#include "gpu_compute_access.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/tone_curve.h"

namespace sound_mind::core {

namespace {

using sound_mind::codec::StreamImage;

/// @brief Clamp-to-edge index lookup - the boundary convention every
/// Installment B filter shares (see `applyFilter()`'s own docs).
int clampIndex(int index, int size) {
    if (index < 0) {
        return 0;
    }
    if (index >= size) {
        return size - 1;
    }
    return index;
}

/// @brief A 1D Gaussian kernel for `sigma`, truncated at 4 standard
/// deviations (matching `scipy.ndimage.gaussian_filter`'s own default
/// `truncate`) and normalized to sum to 1.
std::vector<float> gaussianKernel1D(float sigma) {
    const float s = std::max(0.1f, sigma);
    const int radius = std::max(1, static_cast<int>(std::ceil(4.0f * s)));
    std::vector<float> kernel(static_cast<std::size_t>(radius) * 2 + 1);
    float sum = 0.0f;
    for (int i = -radius; i <= radius; ++i) {
        const float weight = std::exp(-(static_cast<float>(i) * static_cast<float>(i)) / (2.0f * s * s));
        kernel[static_cast<std::size_t>(i + radius)] = weight;
        sum += weight;
    }
    for (float& weight : kernel) {
        weight /= sum;
    }
    return kernel;
}

/// @brief Separable 2D Gaussian blur over a `binCount`x`frameCount` grid
/// (bins are rows, frames are columns) - a horizontal pass along frames
/// then a vertical pass along bins, each using `gaussianKernel1D()` with
/// clamp-to-edge boundary handling.
std::vector<float> gaussianBlur2D(const std::vector<float>& grid, std::uint32_t binCount, std::uint32_t frameCount,
                                   float sigma) {
    const auto kernel = gaussianKernel1D(sigma);
    const int radius = static_cast<int>(kernel.size() / 2);
    const int rows = static_cast<int>(binCount);
    const int cols = static_cast<int>(frameCount);

    std::vector<float> horizontal(grid.size());
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            float sum = 0.0f;
            for (int k = -radius; k <= radius; ++k) {
                const int sourceCol = clampIndex(col + k, cols);
                sum += grid[static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) +
                            static_cast<std::size_t>(sourceCol)] *
                       kernel[static_cast<std::size_t>(k + radius)];
            }
            horizontal[static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) +
                       static_cast<std::size_t>(col)] = sum;
        }
    }

    std::vector<float> result(grid.size());
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            float sum = 0.0f;
            for (int k = -radius; k <= radius; ++k) {
                const int sourceRow = clampIndex(row + k, rows);
                sum += horizontal[static_cast<std::size_t>(sourceRow) * static_cast<std::size_t>(cols) +
                                   static_cast<std::size_t>(col)] *
                       kernel[static_cast<std::size_t>(k + radius)];
            }
            result[static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(col)] =
                sum;
        }
    }
    return result;
}

/// @brief `gaussianBlur2D()` above, preferring the GPU when available -
/// `UniformBlur`'s own dispatch point, confirmed with the user ahead of
/// implementation (`v0.Y.30.1`'s roadmap entry names Uniform Blur as "the
/// cleanest textbook GPU fit"). Falls back to the CPU implementation
/// above whenever `sound_mind::core::detail::gpuComputeDeviceOrNull()`
/// returns `nullptr` (no device at all, or
/// `setGpuComputeForcedOffForTesting(true)` is in effect), or if the GPU
/// call itself throws - a device lost mid-session (driver reset/removal)
/// is treated as a transient failure to degrade past, not a fatal error,
/// also confirmed with the user.
///
/// Deliberately not used by `sharpen2D()`'s own internal blur call below:
/// that one always runs at a small, fixed `sigma` (1.0, kernel radius 4),
/// a much smaller workload than this GPU kernel has ever been measured
/// against (`sound-mind-gpu`'s own tests use a 512x512 grid at `sigma`
/// 8.0) - routing it through the GPU risked a real regression from fixed
/// per-call dispatch overhead outweighing such a small kernel's own CPU
/// cost, unmeasured, so left CPU-only rather than assumed faster.
std::vector<float> gaussianBlur2DGpuOrCpu(const std::vector<float>& grid, std::uint32_t binCount,
                                           std::uint32_t frameCount, float sigma) {
    if (auto* device = detail::gpuComputeDeviceOrNull()) {
        try {
            // gaussianBlur2D()'s own grid is binCount (rows) x frameCount
            // (columns); ComputeDevice::gaussianBlur2D() names the same
            // shape width (columns) x height (rows) - frameCount is the
            // width, binCount the height.
            return device->gaussianBlur2D(grid, frameCount, binCount, sigma);
        } catch (const std::exception&) {
            // Fall through to the CPU path below.
        }
    }
    return gaussianBlur2D(grid, binCount, frameCount, sigma);
}

/// @brief A 2D median filter over a square, clamp-to-edge window (matching
/// legacy's own `size | 1`, floored at `3`, odd-window forcing).
std::vector<float> medianBlur2D(const std::vector<float>& grid, std::uint32_t binCount, std::uint32_t frameCount,
                                 int size) {
    const int windowSize = std::max(3, size | 1);
    const int half = windowSize / 2;
    const int rows = static_cast<int>(binCount);
    const int cols = static_cast<int>(frameCount);

    std::vector<float> result(grid.size());
    std::vector<float> window;
    window.reserve(static_cast<std::size_t>(windowSize) * static_cast<std::size_t>(windowSize));
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            window.clear();
            for (int dr = -half; dr <= half; ++dr) {
                const int sourceRow = clampIndex(row + dr, rows);
                for (int dc = -half; dc <= half; ++dc) {
                    const int sourceCol = clampIndex(col + dc, cols);
                    window.push_back(grid[static_cast<std::size_t>(sourceRow) * static_cast<std::size_t>(cols) +
                                           static_cast<std::size_t>(sourceCol)]);
                }
            }
            const auto middle = window.begin() + static_cast<std::ptrdiff_t>(window.size() / 2);
            std::nth_element(window.begin(), middle, window.end());
            result[static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(col)] =
                *middle;
        }
    }
    return result;
}

/// @brief Builds `DirectionalBlur`'s own kernel as a sparse list of
/// `(rowOffset, colOffset, weight)` samples rather than a dense
/// `(2*length+1)^2` grid - ported directly from legacy's own
/// `motion_blur_filter`: `length` unit steps are taken each side of
/// center along `(cos(angle), -sin(angle))`, each snapped to its nearest
/// integer cell (collisions at coarse angles accumulate rather than
/// overwrite), then the whole set is normalized to sum to 1.
std::vector<std::tuple<int, int, float>> directionalBlurOffsets(int length, float angleDegrees) {
    const int n = std::max(1, length);
    const float angleRadians = angleDegrees * std::numbers::pi_v<float> / 180.0f;
    const float cosA = std::cos(angleRadians);
    const float sinA = std::sin(angleRadians);

    std::map<std::pair<int, int>, float> weightsByOffset;
    for (int step = -n; step <= n; ++step) {
        const int colOffset = static_cast<int>(std::lround(static_cast<float>(step) * cosA));
        const int rowOffset = static_cast<int>(std::lround(-static_cast<float>(step) * sinA));
        weightsByOffset[{rowOffset, colOffset}] += 1.0f;
    }

    float sum = 0.0f;
    for (const auto& [offset, weight] : weightsByOffset) {
        sum += weight;
    }

    std::vector<std::tuple<int, int, float>> offsets;
    offsets.reserve(weightsByOffset.size());
    for (const auto& [offset, weight] : weightsByOffset) {
        offsets.emplace_back(offset.first, offset.second, weight / sum);
    }
    return offsets;
}

/// @brief Convolves a `binCount`x`frameCount` grid with
/// `directionalBlurOffsets()`'s own sparse kernel, clamp-to-edge.
std::vector<float> directionalBlur2D(const std::vector<float>& grid, std::uint32_t binCount,
                                      std::uint32_t frameCount, int length, float angleDegrees) {
    const auto offsets = directionalBlurOffsets(length, angleDegrees);
    const int rows = static_cast<int>(binCount);
    const int cols = static_cast<int>(frameCount);

    std::vector<float> result(grid.size());
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            float sum = 0.0f;
            for (const auto& [rowOffset, colOffset, weight] : offsets) {
                const int sourceRow = clampIndex(row + rowOffset, rows);
                const int sourceCol = clampIndex(col + colOffset, cols);
                sum += grid[static_cast<std::size_t>(sourceRow) * static_cast<std::size_t>(cols) +
                            static_cast<std::size_t>(sourceCol)] *
                       weight;
            }
            result[static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(col)] =
                sum;
        }
    }
    return result;
}

/// @brief Unsharp mask: `original + amount * (original - gaussianBlur2D(original, sigma=1.0))`,
/// matching legacy's own `sharpen_filter`'s fixed internal blur sigma.
std::vector<float> sharpen2D(const std::vector<float>& grid, std::uint32_t binCount, std::uint32_t frameCount,
                              float amount) {
    const auto blurred = gaussianBlur2D(grid, binCount, frameCount, 1.0f);
    std::vector<float> result(grid.size());
    for (std::size_t i = 0; i < grid.size(); ++i) {
        result[i] = grid[i] + amount * (grid[i] - blurred[i]);
    }
    return result;
}

/// @brief Applies a per-channel grid filter (one of the four helpers above)
/// to both `leftMagnitudeDb` and `rightMagnitudeDb` independently, leaving
/// phase untouched - the shared plumbing every Installment B filter type
/// dispatches through.
StreamImage applyPerChannelGridFilter(
    const StreamImage& composite,
    const std::function<std::vector<float>(const std::vector<float>&, std::uint32_t, std::uint32_t)>& filterFn) {
    StreamImage result = composite;
    if (composite.config.binCount == 0 || composite.frameCount == 0) {
        return result;
    }
    result.leftMagnitudeDb = filterFn(composite.leftMagnitudeDb, composite.config.binCount, composite.frameCount);
    result.rightMagnitudeDb = filterFn(composite.rightMagnitudeDb, composite.config.binCount, composite.frameCount);
    return result;
}

/// @brief `applyFilter()`'s own `FrequencyAxisGradient` implementation -
/// see its docs for the exact per-bin blend.
StreamImage applyFrequencyAxisGradient(const StreamImage& composite, const Gradient& gradient) {
    StreamImage result = composite;
    const std::uint32_t binCount = composite.config.binCount;
    const std::uint32_t frameCount = composite.frameCount;
    if (binCount == 0 || frameCount == 0) {
        return result;
    }

    for (std::uint32_t bin = 0; bin < binCount; ++bin) {
        // binIndexToFrequency()'s own log-scaled bin-to-Hz mapping is
        // already linear in bin index, so a bin-index fraction *is*
        // "normalized position across the encoded frequency range" - no
        // separate Hz conversion needed (see applyFilter()'s own docs).
        const float t = (binCount > 1) ? static_cast<float>(bin) / static_cast<float>(binCount - 1) : 0.0f;
        const GradientStop stop = gradient.evaluate(t);

        for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
            const std::size_t cell = cellIndex(bin, frame, frameCount);
            blendTowardStop(result.leftMagnitudeDb[cell], result.rightMagnitudeDb[cell], stop);
            // Phase is left untouched - the gradient only ever targets
            // amplitude, the same as Fill/Paint's own use of a Gradient.
        }
    }
    return result;
}

// The same `-96..0` dB display range `color_mapping.cpp`'s own (private,
// Codec-internal) `dbToByte()`/`byteToDb()` already establish - Core's
// own copy, not a shared header, since Core can't depend on Codec's
// private `src/`-only files (the same "duplicated, not shared" reasoning
// `docs/sound-mind-architecture.md`'s Decision #59 already gives for
// `dbToLinearAmplitude()`/`linearAmplitudeToDb()`). A plain float
// `[0, 1]` normalization, not an 8-bit round trip - full precision, since
// nothing here needs to display or store a byte.
constexpr float kToneCurveMinDb = -96.0f;
constexpr float kToneCurveMaxDb = 0.0f;

float dbToUnit(float db) {
    const float clamped = std::clamp(db, kToneCurveMinDb, kToneCurveMaxDb);
    return (clamped - kToneCurveMinDb) / (kToneCurveMaxDb - kToneCurveMinDb);
}

float unitToDb(float unit) { return kToneCurveMinDb + unit * (kToneCurveMaxDb - kToneCurveMinDb); }

/// @brief `applyFilter()`'s own `ToneCurve` implementation - see its docs
/// for the exact remap.
StreamImage applyToneCurve(const StreamImage& composite, const std::vector<std::array<float, 2>>& points) {
    StreamImage result = composite;
    if (composite.config.binCount == 0 || composite.frameCount == 0 || points.empty()) {
        return result;
    }

    const auto tangents = monotoneCubicTangents(points);
    const auto remap = [&points, &tangents](float db) {
        const float curved = evaluateToneCurve(points, tangents, dbToUnit(db));
        return unitToDb(std::clamp(curved, 0.0f, 1.0f));
    };
    for (float& db : result.leftMagnitudeDb) {
        db = remap(db);
    }
    for (float& db : result.rightMagnitudeDb) {
        db = remap(db);
    }
    // Phase is left untouched - the same amplitude-only contract every
    // other filter this milestone has built so far already keeps.
    return result;
}

}  // namespace

StreamImage applyFilter(const StreamImage& composite, const FilterConfiguration& config,
                          const ProjectSettings& /*settings*/) {
    switch (config.type()) {
        case FilterType::FrequencyAxisGradient:
            return applyFrequencyAxisGradient(composite, config.frequencyGradient());
        case FilterType::UniformBlur:
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return gaussianBlur2DGpuOrCpu(grid, bins, frames, config.blurSigma());
                });
        case FilterType::EdgePreservingBlur:
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return medianBlur2D(grid, bins, frames, config.medianSize());
                });
        case FilterType::DirectionalBlur:
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return directionalBlur2D(grid, bins, frames, config.directionalBlurLength(),
                                              config.directionalBlurAngleDegrees());
                });
        case FilterType::Sharpen:
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return sharpen2D(grid, bins, frames, config.sharpenAmount());
                });
        case FilterType::ToneCurve:
            return applyToneCurve(composite, config.toneCurvePoints());
    }
    return composite;
}

}  // namespace sound_mind::core
