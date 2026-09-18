#include "sound_mind/core/filter_application.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <numbers>
#include <random>
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

/// @brief `mindWave`'s own value at `(bin, frame)`'s own canvas position,
/// lerped from `baseline` (where the MindWave evaluates to `0`) to
/// `ceiling` (where it evaluates to `1`) - `v0.Y.31.1` Installment D's own
/// per-cell parameter-binding entry point, matching `docs/sound-mind-
/// design.md`'s own "the bound value becomes the ceiling... falls toward
/// its own no-effect baseline" contract exactly. Returns `ceiling`
/// unconditionally when `mindWave` is `nullptr` - the unbound case, where
/// the parameter is simply its own fixed, configured value everywhere
/// (this function is never even called on that path in practice, since
/// every caller below branches around it, but this keeps the contract
/// well-defined regardless).
double perCellParameterValue(const MindWave* mindWave, double baseline, double ceiling, std::uint32_t bin,
                              std::uint32_t frame, const sound_mind::codec::StreamCodecConfig& config) {
    if (mindWave == nullptr) {
        return ceiling;
    }
    const TimeFrequencyPoint point{frameIndexToTime(frame, config), binIndexToFrequency(static_cast<float>(bin), config)};
    const double field = mindWave->evaluate(point, config);
    return baseline + (ceiling - baseline) * field;
}

/// @brief `perCellParameterValue()` above, evaluated once for every cell
/// in a `binCount x frameCount` grid and returned as a flat, row-major
/// array - `v0.Y.31.1` Installment D2's own building block, needed once a
/// GPU dispatch requires a real uploadable buffer rather than a lazily-
/// evaluated closure. Building the array unconditionally (even on the
/// CPU-only path) also fixes a real Installment D1 inefficiency: the
/// lambda-based `sigmaAt()`/`sizeAt()`/etc. closures `applyFilter()`
/// previously built were each evaluated **twice** per cell -
/// `applyPerChannelGridFilter()` calls its own filter function once per
/// channel (left, then right), and the per-cell parameter value doesn't
/// depend on which channel is being processed, so the second evaluation
/// was pure waste. Returns an all-`ceiling` array immediately, without
/// evaluating anything, when `mindWave` is `nullptr` - the common,
/// unbound case skips every `MindWave::evaluate()` call entirely, the
/// same short-circuit `compositor.cpp`'s own `buildMindWaveField()`
/// already establishes.
std::vector<float> buildParameterField(const MindWave* mindWave, double baseline, double ceiling,
                                        std::uint32_t binCount, std::uint32_t frameCount,
                                        const sound_mind::codec::StreamCodecConfig& config) {
    const std::size_t cellCount = std::size_t{binCount} * frameCount;
    if (mindWave == nullptr) {
        return std::vector<float>(cellCount, static_cast<float>(ceiling));
    }
    std::vector<float> field(cellCount);
    for (std::uint32_t bin = 0; bin < binCount; ++bin) {
        for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
            field[static_cast<std::size_t>(bin) * frameCount + frame] =
                static_cast<float>(perCellParameterValue(mindWave, baseline, ceiling, bin, frame, config));
        }
    }
    return field;
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

/// @brief `gaussianBlur2D()` above, but with `sigma` evaluated fresh per
/// cell via `sigmaPerCell` (`v0.Y.31.1` Installment D's own `blurSigma`
/// binding - `buildParameterField()`'s own output, one entry per cell,
/// row-major) - a genuine, non-separable 2D Gaussian: each output cell
/// computes its own weighted sum directly over its own (sigma-dependent)
/// neighborhood, rather than two 1D passes (which would only be
/// mathematically correct for a single, shared sigma). This is the CPU
/// fallback for `gaussianBlur2DVaryingGpuOrCpu()` below (Installment D2) -
/// the unbound path above keeps its own separate, existing GPU dispatch
/// (`gaussianBlur2DGpuOrCpu()`) unaffected either way.
std::vector<float> gaussianBlur2DVarying(const std::vector<float>& grid, std::uint32_t binCount,
                                          std::uint32_t frameCount, const std::vector<float>& sigmaPerCell) {
    const int rows = static_cast<int>(binCount);
    const int cols = static_cast<int>(frameCount);
    std::vector<float> result(grid.size());
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const std::size_t cell =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(col);
            const float sigma = std::max(0.1f, sigmaPerCell[cell]);
            const int radius = std::max(1, static_cast<int>(std::ceil(4.0f * sigma)));
            const float twoSigmaSquared = 2.0f * sigma * sigma;
            float weightedSum = 0.0f;
            float weightTotal = 0.0f;
            for (int dr = -radius; dr <= radius; ++dr) {
                const int sourceRow = clampIndex(row + dr, rows);
                for (int dc = -radius; dc <= radius; ++dc) {
                    const int sourceCol = clampIndex(col + dc, cols);
                    const auto distanceSquared = static_cast<float>(dr * dr + dc * dc);
                    const float weight = std::exp(-distanceSquared / twoSigmaSquared);
                    weightedSum += grid[static_cast<std::size_t>(sourceRow) * static_cast<std::size_t>(cols) +
                                        static_cast<std::size_t>(sourceCol)] *
                                   weight;
                    weightTotal += weight;
                }
            }
            result[cell] = weightedSum / weightTotal;
        }
    }
    return result;
}

/// @brief `gaussianBlur2DVarying()` above, preferring the GPU when
/// available - `v0.Y.31.1` Installment D2's own dispatch point, matching
/// `gaussianBlur2DGpuOrCpu()`'s own fallback contract (a device lost
/// mid-session degrades to the CPU path, not a fatal error).
std::vector<float> gaussianBlur2DVaryingGpuOrCpu(const std::vector<float>& grid, std::uint32_t binCount,
                                                  std::uint32_t frameCount, const std::vector<float>& sigmaPerCell) {
    if (auto* device = detail::gpuComputeDeviceOrNull()) {
        try {
            return device->gaussianBlur2DVarying(grid, frameCount, binCount, sigmaPerCell);
        } catch (const std::exception&) {
            // Fall through to the CPU path below.
        }
    }
    return gaussianBlur2DVarying(grid, binCount, frameCount, sigmaPerCell);
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

/// @brief `medianBlur2D()` above, but with the window size evaluated
/// fresh per cell via `sizePerCell` (`v0.Y.31.1` Installment D's own
/// `medianSize` binding - `buildParameterField()`'s own output). A
/// per-cell size of `1` or less (the baseline this parameter's own
/// MindWave binding falls toward - see `applyFilter()`'s own docs) skips
/// windowing entirely for that cell - the true identity a `1`-cell
/// "window" already is, not a `3`-cell one (the unbound path's own floor,
/// which is *not* identity). Clamped to at most `31` (`961` samples) -
/// added alongside Installment D2's own GPU kernel, which needs a
/// compile-time-sized local array to gather a window into (no
/// `std::nth_element` equivalent exists in HLSL) and would otherwise risk
/// a real out-of-bounds write for an unrealistically large bound value;
/// matched here so the CPU and GPU paths agree even in that edge case,
/// not just in the realistic range `medianSize()`'s own Studio UI already
/// limits to. This is the CPU fallback for `medianBlur2DVaryingGpuOrCpu()`
/// below.
std::vector<float> medianBlur2DVarying(const std::vector<float>& grid, std::uint32_t binCount,
                                        std::uint32_t frameCount, const std::vector<float>& sizePerCell) {
    const int rows = static_cast<int>(binCount);
    const int cols = static_cast<int>(frameCount);
    std::vector<float> result(grid.size());
    std::vector<float> window;
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const std::size_t cell = static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) +
                                      static_cast<std::size_t>(col);
            const float rawSize = sizePerCell[cell];
            if (rawSize <= 1.0f) {
                result[cell] = grid[cell];  // Baseline: a 1-cell window is the identity.
                continue;
            }
            int windowSize = std::max(3, static_cast<int>(std::lround(rawSize)) | 1);
            windowSize = std::min(windowSize, 31);
            const int half = windowSize / 2;
            window.clear();
            window.reserve(static_cast<std::size_t>(windowSize) * static_cast<std::size_t>(windowSize));
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
            result[cell] = *middle;
        }
    }
    return result;
}

/// @brief `medianBlur2DVarying()` above, preferring the GPU when
/// available - Installment D2's own dispatch point.
std::vector<float> medianBlur2DVaryingGpuOrCpu(const std::vector<float>& grid, std::uint32_t binCount,
                                                std::uint32_t frameCount, const std::vector<float>& sizePerCell) {
    if (auto* device = detail::gpuComputeDeviceOrNull()) {
        try {
            return device->medianBlur2DVarying(grid, frameCount, binCount, sizePerCell);
        } catch (const std::exception&) {
            // Fall through to the CPU path below.
        }
    }
    return medianBlur2DVarying(grid, binCount, frameCount, sizePerCell);
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

/// @brief `directionalBlur2D()` above, but with `length`/`angleDegrees`
/// both evaluated fresh per cell via `lengthPerCell`/`anglePerCell`
/// (`v0.Y.31.1` Installment D's own `directionalBlurLength`/
/// `directionalBlurAngleDegrees` bindings - `buildParameterField()`'s own
/// output each, independent of each other, so either, both, or neither
/// may actually be bound; an unbound one still has a real array here,
/// just filled with its own fixed configured value throughout - see
/// `applyFilter()`'s own dispatch). A per-cell length of `0` or less (the
/// baseline `directionalBlurLength` falls toward) skips convolution
/// entirely for that cell - the true identity a zero-length kernel
/// already is. `directionalBlurOffsets()` is rebuilt fresh per cell (it
/// depends on both length and angle, both now potentially cell-specific)
/// rather than reused globally - real additional cost, same as the other
/// two varying kernel-shape filters. This is the CPU fallback for
/// `directionalBlur2DVaryingGpuOrCpu()` below.
std::vector<float> directionalBlur2DVarying(const std::vector<float>& grid, std::uint32_t binCount,
                                             std::uint32_t frameCount, const std::vector<float>& lengthPerCell,
                                             const std::vector<float>& anglePerCell) {
    const int rows = static_cast<int>(binCount);
    const int cols = static_cast<int>(frameCount);
    std::vector<float> result(grid.size());
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const std::size_t cell = static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) +
                                      static_cast<std::size_t>(col);
            const float rawLength = lengthPerCell[cell];
            if (rawLength <= 0.0f) {
                result[cell] = grid[cell];  // Baseline: a zero-length kernel is the identity.
                continue;
            }
            const auto length = static_cast<int>(std::lround(rawLength));
            const float angleDegrees = anglePerCell[cell];
            const auto offsets = directionalBlurOffsets(length, angleDegrees);
            float sum = 0.0f;
            for (const auto& [rowOffset, colOffset, weight] : offsets) {
                const int sourceRow = clampIndex(row + rowOffset, rows);
                const int sourceCol = clampIndex(col + colOffset, cols);
                sum += grid[static_cast<std::size_t>(sourceRow) * static_cast<std::size_t>(cols) +
                            static_cast<std::size_t>(sourceCol)] *
                       weight;
            }
            result[cell] = sum;
        }
    }
    return result;
}

/// @brief `directionalBlur2DVarying()` above, preferring the GPU when
/// available - Installment D2's own dispatch point. The GPU kernel itself
/// doesn't deduplicate offsets landing on the same integer cell the way
/// `directionalBlurOffsets()` does (no associative container exists in
/// HLSL) - see `sound_mind::gpu::ComputeDevice::
/// directionalBlur2DVarying()`'s own docs for why summing each step
/// individually is mathematically equivalent regardless, so this still
/// agrees with the CPU path within ordinary floating-point tolerance.
std::vector<float> directionalBlur2DVaryingGpuOrCpu(const std::vector<float>& grid, std::uint32_t binCount,
                                                     std::uint32_t frameCount,
                                                     const std::vector<float>& lengthPerCell,
                                                     const std::vector<float>& anglePerCell) {
    if (auto* device = detail::gpuComputeDeviceOrNull()) {
        try {
            return device->directionalBlur2DVarying(grid, frameCount, binCount, lengthPerCell, anglePerCell);
        } catch (const std::exception&) {
            // Fall through to the CPU path below.
        }
    }
    return directionalBlur2DVarying(grid, binCount, frameCount, lengthPerCell, anglePerCell);
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

/// @brief `sharpen2D()` above, but with `amount` evaluated fresh per cell
/// via `amountAt(bin, frame)` (`v0.Y.31.1` Installment D's own
/// `sharpenAmount` binding). Unlike the other three bindable kernel-shape
/// parameters, this one varies for free: the internal blur (fixed
/// `sigma=1.0`, unaffected by `amount`) still runs exactly once, and
/// `amount` only scales an already-computed per-cell difference term - no
/// new per-cell kernel work at all, so this needs no GPU follow-up the way
/// the other three do.
std::vector<float> sharpen2DVarying(const std::vector<float>& grid, std::uint32_t binCount, std::uint32_t frameCount,
                                     const std::function<double(std::uint32_t, std::uint32_t)>& amountAt) {
    const auto blurred = gaussianBlur2D(grid, binCount, frameCount, 1.0f);
    std::vector<float> result(grid.size());
    const auto cols = static_cast<std::size_t>(frameCount);
    for (std::uint32_t row = 0; row < binCount; ++row) {
        for (std::uint32_t col = 0; col < frameCount; ++col) {
            const std::size_t cell = static_cast<std::size_t>(row) * cols + static_cast<std::size_t>(col);
            const auto amount = static_cast<float>(amountAt(row, col));
            result[cell] = grid[cell] + amount * (grid[cell] - blurred[cell]);
        }
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

// `compositor.cpp`'s own private `dbToLinearAmplitude()`/
// `linearAmplitudeToDb()`, duplicated here rather than shared - the same
// "duplicated, not shared" reasoning Decision #59 already gives (an
// internal, unexported `src/`-only helper isn't a header this file can
// depend on). `ChannelBalance`'s own energy-redistribution formula needs
// genuine linear-amplitude arithmetic - dB values can't be meaningfully
// summed directly the way `total = left + right` requires.
constexpr float kMinLinearAmplitude = 1e-7f;

float dbToLinearAmplitude(float db) { return std::pow(10.0f, db / 20.0f); }

float linearAmplitudeToDb(float amplitude) { return 20.0f * std::log10(std::max(amplitude, kMinLinearAmplitude)); }

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

// ---------------------------------------------------------------------------
// v0.Y.36.1 Installment A: Noise & distortion.
// ---------------------------------------------------------------------------
//
// All eight operate directly in dB space, leave phase untouched, and use no
// MindWave binding at all - see FilterConfiguration's own docs for why the
// last point is a deliberate, matching-precedent deferral, not an oversight.

/// @brief A fast, well-distributed, fully deterministic hash of
/// `(seed, a, b)` - `SpeckleAdd`/`GranularNoise`'s own noise source, so the
/// same `FilterConfiguration` always produces the same noise pattern
/// (stable across every recomposite and reload) without needing to persist
/// an entire noise buffer. A standard iterative mix (in the spirit of
/// MurmurHash3's own finalizer), not cryptographic - only speed and a
/// visually/aurally even distribution matter here.
std::uint32_t hashCell(std::uint32_t seed, std::uint32_t a, std::uint32_t b) {
    std::uint32_t h = seed;
    h ^= a + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= b + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

/// @brief `hashCell()`'s own output, rescaled to `[0, 1)`.
float hashToUnitFloat(std::uint32_t hash) {
    return static_cast<float>(hash) / (static_cast<float>(std::numeric_limits<std::uint32_t>::max()) + 1.0f);
}

/// @brief `SpeckleAdd`'s own implementation - see `applyFilter()`'s docs.
/// A no-op whenever `density` or `intensity` is non-positive, without
/// touching the hash at all.
std::vector<float> applySpeckleAdd(const std::vector<float>& grid, std::uint32_t binCount, std::uint32_t frameCount,
                                    std::uint32_t seed, float density, float intensity) {
    if (density <= 0.0f || intensity <= 0.0f) {
        return grid;
    }
    std::vector<float> result = grid;
    const auto cols = static_cast<std::size_t>(frameCount);
    for (std::uint32_t bin = 0; bin < binCount; ++bin) {
        for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
            const std::size_t cell = static_cast<std::size_t>(bin) * cols + frame;
            if (hashToUnitFloat(hashCell(seed, bin, frame)) < density) {
                result[cell] = grid[cell] + intensity * (kToneCurveMaxDb - grid[cell]);
            }
        }
    }
    return result;
}

/// @brief `SpeckleRemove`'s own implementation - see `applyFilter()`'s docs.
/// Reuses `medianBlur2D()`'s own fixed 3x3 window rather than a bespoke
/// median routine.
std::vector<float> applySpeckleRemove(const std::vector<float>& grid, std::uint32_t binCount,
                                       std::uint32_t frameCount, float thresholdDb) {
    const auto median = medianBlur2D(grid, binCount, frameCount, 3);
    std::vector<float> result(grid.size());
    for (std::size_t i = 0; i < grid.size(); ++i) {
        result[i] = (std::abs(grid[i] - median[i]) > thresholdDb) ? median[i] : grid[i];
    }
    return result;
}

/// @brief `Denoise`'s own implementation - see `applyFilter()`'s docs. A
/// per-cell downward expander/spectral gate with a `kKneeWidthDb`-wide soft
/// knee straddling `noiseFloorDb`, avoiding a hard on/off click right at
/// the threshold.
std::vector<float> applyDenoise(const std::vector<float>& grid, float noiseFloorDb, float reductionDb) {
    constexpr float kKneeWidthDb = 6.0f;
    std::vector<float> result(grid.size());
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const float db = grid[i];
        float attenuationFraction;
        if (db >= noiseFloorDb + kKneeWidthDb) {
            attenuationFraction = 0.0f;
        } else if (db <= noiseFloorDb - kKneeWidthDb) {
            attenuationFraction = 1.0f;
        } else {
            attenuationFraction = (noiseFloorDb + kKneeWidthDb - db) / (2.0f * kKneeWidthDb);
        }
        result[i] = db - attenuationFraction * reductionDb;
    }
    return result;
}

/// @brief `BitDepthCrush`'s own implementation - see `applyFilter()`'s
/// docs. A true no-op at `crushAmount <= 0`; otherwise quantizes the
/// `dbToUnit()`-normalized loudness into `lerp(256, 2, crushAmount)`
/// discrete steps.
std::vector<float> applyBitDepthCrush(const std::vector<float>& grid, float crushAmount) {
    const float amount = std::clamp(crushAmount, 0.0f, 1.0f);
    if (amount <= 0.0f) {
        return grid;
    }
    const float levels = std::max(2.0f, 256.0f - amount * 254.0f);
    std::vector<float> result(grid.size());
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const float unit = dbToUnit(grid[i]);
        const float quantized = std::clamp(std::floor(unit * levels) / levels, 0.0f, 1.0f);
        result[i] = unitToDb(quantized);
    }
    return result;
}

/// @brief `GranularNoise`'s own implementation - see `applyFilter()`'s
/// docs. Deterministic per `seed` and block position (`hashCell()`, keyed
/// by the block's own top-left corner, not each individual cell) - every
/// cell within a `size` x `size` block gets the exact same offset. A
/// no-op whenever `amountDb` is non-positive.
std::vector<float> applyGranularNoise(const std::vector<float>& grid, std::uint32_t binCount,
                                       std::uint32_t frameCount, std::uint32_t seed, int size, float amountDb) {
    if (amountDb <= 0.0f) {
        return grid;
    }
    const int blockSize = std::max(1, size);
    std::vector<float> result = grid;
    const int rows = static_cast<int>(binCount);
    const int cols = static_cast<int>(frameCount);
    for (int blockRow = 0; blockRow < rows; blockRow += blockSize) {
        for (int blockCol = 0; blockCol < cols; blockCol += blockSize) {
            const float unit = hashToUnitFloat(hashCell(seed, static_cast<std::uint32_t>(blockRow),
                                                          static_cast<std::uint32_t>(blockCol)));
            const float offset = (unit * 2.0f - 1.0f) * amountDb;
            const int rowEnd = std::min(rows, blockRow + blockSize);
            const int colEnd = std::min(cols, blockCol + blockSize);
            for (int row = blockRow; row < rowEnd; ++row) {
                for (int col = blockCol; col < colEnd; ++col) {
                    result[static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) +
                           static_cast<std::size_t>(col)] += offset;
                }
            }
        }
    }
    return result;
}

/// @brief `DynamicSpeckle`'s own implementation - see `applyFilter()`'s
/// docs. Deliberately *not* `hashCell()`-deterministic (unlike
/// `SpeckleAdd`/`GranularNoise` above) - a genuinely fresh
/// `thread_local` RNG roll every call, the whole point being visible
/// flicker across recomposites. Computed over fixed 2x2 blocks (one RNG
/// roll per block, not per cell) specifically to stay cheap on a large
/// canvas, confirmed with the user over a per-cell version.
std::vector<float> applyDynamicSpeckle(const std::vector<float>& grid, std::uint32_t binCount,
                                        std::uint32_t frameCount, float density, float intensity) {
    if (density <= 0.0f || intensity <= 0.0f) {
        return grid;
    }
    constexpr int kBlockSize = 2;
    std::vector<float> result = grid;
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> uniform(0.0f, 1.0f);
    const int rows = static_cast<int>(binCount);
    const int cols = static_cast<int>(frameCount);
    for (int blockRow = 0; blockRow < rows; blockRow += kBlockSize) {
        for (int blockCol = 0; blockCol < cols; blockCol += kBlockSize) {
            if (uniform(rng) >= density) {
                continue;
            }
            const int rowEnd = std::min(rows, blockRow + kBlockSize);
            const int colEnd = std::min(cols, blockCol + kBlockSize);
            for (int row = blockRow; row < rowEnd; ++row) {
                for (int col = blockCol; col < colEnd; ++col) {
                    const std::size_t cell =
                        static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(col);
                    result[cell] = grid[cell] + intensity * (kToneCurveMaxDb - grid[cell]);
                }
            }
        }
    }
    return result;
}

/// @brief `FeedbackDistortion`'s own implementation - see `applyFilter()`'s
/// docs. A one-pole recursive filter along the time axis (frames),
/// independently per bin - `amount` is clamped to `[0, 0.99]` internally
/// regardless of what's stored, guaranteeing stability (a value at or past
/// `1.0` would never decay). Each bin's own first frame is seeded to its
/// own original value, so it's always exactly unchanged - there is no
/// prior frame to feed back from yet.
std::vector<float> applyFeedbackDistortion(const std::vector<float>& grid, std::uint32_t binCount,
                                            std::uint32_t frameCount, float feedbackAmount) {
    if (frameCount == 0) {
        return grid;
    }
    const float amount = std::clamp(feedbackAmount, 0.0f, 0.99f);
    std::vector<float> result(grid.size());
    const auto cols = static_cast<std::size_t>(frameCount);
    for (std::uint32_t bin = 0; bin < binCount; ++bin) {
        const std::size_t rowStart = static_cast<std::size_t>(bin) * cols;
        float previous = grid[rowStart];
        for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
            const std::size_t cell = rowStart + frame;
            previous = (1.0f - amount) * grid[cell] + amount * previous;
            result[cell] = previous;
        }
    }
    return result;
}

/// @brief A period-2 triangle wave folding `x` into `[0, 1]` - the classic
/// wavefolder mechanic `SpectralWavefold` uses.
float triangleFold(float x) {
    float m = std::fmod(x, 2.0f);
    if (m < 0.0f) {
        m += 2.0f;
    }
    return (m <= 1.0f) ? m : (2.0f - m);
}

/// @brief `SpectralWavefold`'s own implementation - see `applyFilter()`'s
/// docs. A true no-op at `foldGain <= 1.0` (any in-range value stays
/// within the triangle wave's own unfolded `[0, 1]` segment).
std::vector<float> applySpectralWavefold(const std::vector<float>& grid, float foldGain) {
    const float gain = std::max(1.0f, foldGain);
    std::vector<float> result(grid.size());
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const float unit = dbToUnit(grid[i]);
        result[i] = unitToDb(triangleFold(unit * gain));
    }
    return result;
}

// ---------------------------------------------------------------------------
// v0.Y.36.1 Installment B: the rest of Tonal, plus the rest of Spectral
// shaping.
// ---------------------------------------------------------------------------

/// @brief `ChannelBalance`'s own implementation - see `applyFilter()`'s
/// docs for the exact energy-redistribution pan law, confirmed with the
/// user against the legacy Python Studio's own `channel_balance()`. Unlike
/// every other filter type, this one genuinely mixes the two channels
/// together rather than processing each independently, so it doesn't go
/// through `applyPerChannelGridFilter()`.
StreamImage applyChannelBalance(const StreamImage& composite, float balance) {
    StreamImage result = composite;
    const float b = std::clamp(balance, 0.0f, 1.0f);
    for (std::size_t i = 0; i < composite.leftMagnitudeDb.size(); ++i) {
        const float total = dbToLinearAmplitude(composite.leftMagnitudeDb[i]) +
                             dbToLinearAmplitude(composite.rightMagnitudeDb[i]);
        result.leftMagnitudeDb[i] = linearAmplitudeToDb(total * (1.0f - b));
        result.rightMagnitudeDb[i] = linearAmplitudeToDb(total * b);
    }
    // Phase is left untouched, matching every other filter's own precedent.
    return result;
}

/// @brief `Invert`'s own implementation - see `applyFilter()`'s docs.
std::vector<float> applyInvert(const std::vector<float>& grid) {
    std::vector<float> result(grid.size());
    for (std::size_t i = 0; i < grid.size(); ++i) {
        result[i] = unitToDb(1.0f - dbToUnit(grid[i]));
    }
    return result;
}

/// @brief A plain 2D spatial convolution over a `binCount` x `frameCount`
/// grid, clamp-to-edge boundary handling (matching every other spatial
/// filter in this file) - `Convolve`'s own core algorithm, confirmed with
/// the user against the legacy Python Studio's own `custom_convolve_filter()`
/// (`scipy.ndimage.convolve(..., mode="nearest")`). `kernelSize` is forced
/// odd (`| 1`, matching `medianBlur2D()`'s own precedent) and at least `1`;
/// a `kernel` whose own length doesn't match `kernelSize * kernelSize`
/// (a corrupted/hand-edited project file) is treated as a no-op rather
/// than indexed out of bounds. `normalize`, when set, first divides the
/// kernel by the sum of its own positive coefficients - otherwise a
/// pure-positive kernel (a blur) would brighten or darken the whole image
/// by that sum, matching legacy's own `custom_convolve_filter()` exactly.
std::vector<float> convolve2D(const std::vector<float>& grid, std::uint32_t binCount, std::uint32_t frameCount,
                               const std::vector<float>& kernel, int kernelSize, bool normalize) {
    const int size = std::max(1, kernelSize | 1);
    if (kernel.size() != static_cast<std::size_t>(size) * static_cast<std::size_t>(size)) {
        return grid;
    }
    std::vector<float> effectiveKernel = kernel;
    if (normalize) {
        float positiveSum = 0.0f;
        for (const float coefficient : effectiveKernel) {
            if (coefficient > 0.0f) {
                positiveSum += coefficient;
            }
        }
        if (positiveSum > 1e-9f) {
            for (float& coefficient : effectiveKernel) {
                coefficient /= positiveSum;
            }
        }
    }

    const int half = size / 2;
    const int rows = static_cast<int>(binCount);
    const int cols = static_cast<int>(frameCount);
    std::vector<float> result(grid.size());
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            float sum = 0.0f;
            for (int kr = -half; kr <= half; ++kr) {
                const int sourceRow = clampIndex(row + kr, rows);
                for (int kc = -half; kc <= half; ++kc) {
                    const int sourceCol = clampIndex(col + kc, cols);
                    const float weight = effectiveKernel[static_cast<std::size_t>(kr + half) *
                                                              static_cast<std::size_t>(size) +
                                                          static_cast<std::size_t>(kc + half)];
                    sum += grid[static_cast<std::size_t>(sourceRow) * static_cast<std::size_t>(cols) +
                                static_cast<std::size_t>(sourceCol)] *
                           weight;
                }
            }
            result[static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(col)] =
                sum;
        }
    }
    return result;
}

/// @brief `convolve2D()` above, blended against the original by `amount`
/// (`0` = fully dry/no-op, `1` = the fully convolved result) - `Convolve`'s
/// own dispatch entry point.
std::vector<float> applyConvolve(const std::vector<float>& grid, std::uint32_t binCount, std::uint32_t frameCount,
                                  const std::vector<float>& kernel, int kernelSize, bool normalize, float amount) {
    const auto convolved = convolve2D(grid, binCount, frameCount, kernel, kernelSize, normalize);
    const float wet = std::clamp(amount, 0.0f, 1.0f);
    std::vector<float> result(grid.size());
    for (std::size_t i = 0; i < grid.size(); ++i) {
        result[i] = grid[i] + wet * (convolved[i] - grid[i]);
    }
    return result;
}

// ---------------------------------------------------------------------------
// v0.Y.36.1 Installment C: Geometric.
// ---------------------------------------------------------------------------

/// @brief Bilinearly samples `grid` at fractional position `(rowF, colF)`,
/// clamp-to-edge at the boundary - `Displace`'s own sampling primitive.
float sampleBilinear(const std::vector<float>& grid, std::uint32_t binCount, std::uint32_t frameCount, float rowF,
                      float colF) {
    const int rows = static_cast<int>(binCount);
    const int cols = static_cast<int>(frameCount);
    const int row0 = static_cast<int>(std::floor(rowF));
    const int col0 = static_cast<int>(std::floor(colF));
    const float fr = rowF - static_cast<float>(row0);
    const float fc = colF - static_cast<float>(col0);
    const int row1 = row0 + 1;
    const int col1 = col0 + 1;

    const auto at = [&](int row, int col) {
        return grid[static_cast<std::size_t>(clampIndex(row, rows)) * static_cast<std::size_t>(cols) +
                    static_cast<std::size_t>(clampIndex(col, cols))];
    };
    const float top = at(row0, col0) + fc * (at(row0, col1) - at(row0, col0));
    const float bottom = at(row1, col0) + fc * (at(row1, col1) - at(row1, col0));
    return top + fr * (bottom - top);
}

/// @brief `Displace`'s own implementation - see `applyFilter()`'s docs.
/// Confirmed with the user against the legacy Python Studio's own
/// `offset_filter()`: same `output(row, col) = input(row - dRow, col -
/// dCol)` source-shift formula and the same bilinear sampling, but
/// clamp-to-edge at the boundary (not legacy's own silence-fill) for
/// consistency with every other spatial filter in this file.
std::vector<float> applyDisplace(const std::vector<float>& grid, std::uint32_t binCount, std::uint32_t frameCount,
                                  float distance, float angleDegrees) {
    const float angleRadians = angleDegrees * std::numbers::pi_v<float> / 180.0f;
    const float dCol = distance * std::cos(angleRadians);
    const float dRow = distance * std::sin(angleRadians);
    std::vector<float> result(grid.size());
    for (std::uint32_t row = 0; row < binCount; ++row) {
        for (std::uint32_t col = 0; col < frameCount; ++col) {
            const float sourceRow = static_cast<float>(row) - dRow;
            const float sourceCol = static_cast<float>(col) - dCol;
            result[static_cast<std::size_t>(row) * frameCount + col] =
                sampleBilinear(grid, binCount, frameCount, sourceRow, sourceCol);
        }
    }
    return result;
}

/// @brief `ChannelCycle`'s own implementation - see `applyFilter()`'s
/// docs. The direct 3-channel analog of the legacy Python Studio's own
/// `color_rotate()`: left loudness, right loudness, and phase are
/// normalized to a shared `[0, 1]` domain (matching legacy's own pages,
/// which are *already* stored that way - amplitude via `dbToUnit()`,
/// phase via wrapping into a `[0, 1]` turn fraction), continuously
/// rotated among each other by `angleDegrees`, then converted back.
/// Unlike every other filter type, this one genuinely mixes all three
/// channels together, so it doesn't go through `applyPerChannelGridFilter()`.
StreamImage applyChannelCycle(const StreamImage& composite, float angleDegrees) {
    StreamImage result = composite;
    float wrappedDegrees = std::fmod(angleDegrees, 360.0f);
    if (wrappedDegrees < 0.0f) {
        wrappedDegrees += 360.0f;
    }
    const float t = wrappedDegrees / 360.0f * 3.0f;
    const int step = static_cast<int>(std::floor(t)) % 3;
    const float fraction = t - std::floor(t);

    for (std::size_t i = 0; i < composite.leftMagnitudeDb.size(); ++i) {
        const float twoPi = 2.0f * std::numbers::pi_v<float>;
        float phaseTurns = std::fmod(composite.sharedPhaseRadians[i], twoPi) / twoPi;
        if (phaseTurns < 0.0f) {
            phaseTurns += 1.0f;
        }
        const std::array<float, 3> channels{dbToUnit(composite.leftMagnitudeDb[i]),
                                              dbToUnit(composite.rightMagnitudeDb[i]), phaseTurns};
        std::array<float, 3> rotated{};
        for (int destination = 0; destination < 3; ++destination) {
            const int source0 = ((destination - step) % 3 + 3) % 3;
            const int source1 = ((destination - step - 1) % 3 + 3) % 3;
            rotated[static_cast<std::size_t>(destination)] =
                (1.0f - fraction) * channels[static_cast<std::size_t>(source0)] +
                fraction * channels[static_cast<std::size_t>(source1)];
        }
        result.leftMagnitudeDb[i] = unitToDb(rotated[0]);
        result.rightMagnitudeDb[i] = unitToDb(rotated[1]);
        result.sharedPhaseRadians[i] = rotated[2] * twoPi;
    }
    return result;
}

}  // namespace

StreamImage applyFilter(const StreamImage& composite, const FilterConfiguration& config,
                          const ProjectSettings& /*settings*/, const FilterParameterMindWaves& mindWaves) {
    const auto& streamConfig = composite.config;
    const std::uint32_t binCount = streamConfig.binCount;
    const std::uint32_t frameCount = composite.frameCount;
    switch (config.type()) {
        case FilterType::FrequencyAxisGradient:
            return applyFrequencyAxisGradient(composite, config.frequencyGradient());
        case FilterType::UniformBlur:
            if (mindWaves.blurSigma != nullptr) {
                // Built once here, not inside the per-channel lambda below -
                // applyPerChannelGridFilter() calls its own filter function
                // once per channel (left, then right), and the per-cell
                // sigma doesn't depend on which channel is being processed,
                // so building it twice would waste half the evaluations
                // (a real Installment D1 inefficiency, fixed alongside this
                // installment's own GPU dispatch, which needs the array
                // built regardless).
                const auto sigmaField =
                    buildParameterField(mindWaves.blurSigma, 0.0, config.blurSigma(), binCount, frameCount, streamConfig);
                return applyPerChannelGridFilter(
                    composite, [&sigmaField](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                        return gaussianBlur2DVaryingGpuOrCpu(grid, bins, frames, sigmaField);
                    });
            }
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return gaussianBlur2DGpuOrCpu(grid, bins, frames, config.blurSigma());
                });
        case FilterType::EdgePreservingBlur:
            if (mindWaves.medianSize != nullptr) {
                const auto sizeField =
                    buildParameterField(mindWaves.medianSize, 1.0, config.medianSize(), binCount, frameCount, streamConfig);
                return applyPerChannelGridFilter(
                    composite, [&sizeField](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                        return medianBlur2DVaryingGpuOrCpu(grid, bins, frames, sizeField);
                    });
            }
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return medianBlur2D(grid, bins, frames, config.medianSize());
                });
        case FilterType::DirectionalBlur:
            if (mindWaves.directionalBlurLength != nullptr || mindWaves.directionalBlurAngle != nullptr) {
                const auto lengthField = buildParameterField(mindWaves.directionalBlurLength, 0.0,
                                                              config.directionalBlurLength(), binCount, frameCount,
                                                              streamConfig);
                const auto angleField = buildParameterField(mindWaves.directionalBlurAngle, 0.0,
                                                             config.directionalBlurAngleDegrees(), binCount,
                                                             frameCount, streamConfig);
                return applyPerChannelGridFilter(composite, [&lengthField, &angleField](const std::vector<float>& grid,
                                                                                         std::uint32_t bins,
                                                                                         std::uint32_t frames) {
                    return directionalBlur2DVaryingGpuOrCpu(grid, bins, frames, lengthField, angleField);
                });
            }
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return directionalBlur2D(grid, bins, frames, config.directionalBlurLength(),
                                              config.directionalBlurAngleDegrees());
                });
        case FilterType::Sharpen:
            if (mindWaves.sharpenAmount != nullptr) {
                // sharpenAmount varies for free (it only scales an already-
                // fixed difference term - see sharpen2DVarying()'s own
                // docs), so this alone stays lambda-based rather than
                // building a field array - there's no GPU dispatch here to
                // feed one to.
                return applyPerChannelGridFilter(
                    composite, [&](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                        return sharpen2DVarying(grid, bins, frames, [&](std::uint32_t bin, std::uint32_t frame) {
                            return perCellParameterValue(mindWaves.sharpenAmount, 0.0, config.sharpenAmount(), bin,
                                                          frame, streamConfig);
                        });
                    });
            }
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return sharpen2D(grid, bins, frames, config.sharpenAmount());
                });
        case FilterType::ToneCurve:
            return applyToneCurve(composite, config.toneCurvePoints());
        case FilterType::SpeckleAdd:
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return applySpeckleAdd(grid, bins, frames, config.noiseSeed(), config.speckleDensity(),
                                            config.speckleIntensity());
                });
        case FilterType::SpeckleRemove:
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return applySpeckleRemove(grid, bins, frames, config.speckleThresholdDb());
                });
        case FilterType::Denoise:
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t, std::uint32_t) {
                    return applyDenoise(grid, config.noiseFloorDb(), config.reductionDb());
                });
        case FilterType::BitDepthCrush:
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t, std::uint32_t) {
                    return applyBitDepthCrush(grid, config.crushAmount());
                });
        case FilterType::GranularNoise:
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return applyGranularNoise(grid, bins, frames, config.noiseSeed(), config.grainSize(),
                                               config.grainAmountDb());
                });
        case FilterType::DynamicSpeckle:
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return applyDynamicSpeckle(grid, bins, frames, config.speckleDensity(), config.speckleIntensity());
                });
        case FilterType::FeedbackDistortion:
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return applyFeedbackDistortion(grid, bins, frames, config.feedbackAmount());
                });
        case FilterType::SpectralWavefold:
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t, std::uint32_t) {
                    return applySpectralWavefold(grid, config.foldGain());
                });
        case FilterType::ChannelBalance:
            return applyChannelBalance(composite, config.channelBalance());
        case FilterType::Invert:
            return applyPerChannelGridFilter(
                composite, [](const std::vector<float>& grid, std::uint32_t, std::uint32_t) {
                    return applyInvert(grid);
                });
        case FilterType::Convolve:
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return applyConvolve(grid, bins, frames, config.convolveKernel(), config.convolveKernelSize(),
                                          config.convolveNormalize(), config.convolveAmount());
                });
        case FilterType::Displace:
            return applyPerChannelGridFilter(
                composite, [&config](const std::vector<float>& grid, std::uint32_t bins, std::uint32_t frames) {
                    return applyDisplace(grid, bins, frames, config.displaceDistance(), config.displaceAngleDegrees());
                });
        case FilterType::ChannelCycle:
            return applyChannelCycle(composite, config.channelCycleAngleDegrees());
    }
    return composite;
}

}  // namespace sound_mind::core
