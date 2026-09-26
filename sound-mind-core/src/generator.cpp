#include "sound_mind/core/generator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>
#include <vector>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

namespace {

/// @brief A silent, correctly-dimensioned `StreamImage` for whichever
/// `GeneratorFamily` isn't implemented yet - see `GeneratorFamily`'s own
/// docs.
sound_mind::codec::StreamImage silentResult(const sound_mind::codec::StreamCodecConfig& config,
                                             std::uint32_t frameCount) {
    sound_mind::codec::StreamImage result;
    result.config = config;
    result.frameCount = frameCount;
    result.sampleCount = std::uint64_t{frameCount} * config.hopLength;
    const std::size_t cellCount = std::size_t{config.binCount} * frameCount;
    result.leftMagnitudeDb.assign(cellCount, -96.0f);
    result.rightMagnitudeDb.assign(cellCount, -96.0f);
    result.sharedPhaseRadians.assign(cellCount, 0.0f);
    return result;
}

/// @brief `grid`'s own value at `(row, col)`, clamping `row` at the
/// frequency-axis edges (a lattice generator's own texture doesn't wrap
/// vertically - there's no meaningful "wraparound" between the lowest and
/// highest frequency) and wrapping `col` around the time axis (so the
/// result loops cleanly if ever used as a seamless, repeating source).
float gridValueAt(const std::vector<float>& grid, std::uint32_t rows, std::uint32_t cols, int row, int col) {
    const int clampedRow = std::clamp(row, 0, static_cast<int>(rows) - 1);
    const int wrappedCol = ((col % static_cast<int>(cols)) + static_cast<int>(cols)) % static_cast<int>(cols);
    return grid[static_cast<std::size_t>(clampedRow) * cols + static_cast<std::size_t>(wrappedCol)];
}

/// @brief One coupled-map-lattice smoothing pass: every cell moves
/// halfway toward its own 4-neighbor average - see
/// `generateLatticeContent()`'s own docs for why repeating this pushes
/// the result toward `order`, and skipping it entirely leaves it at pure
/// `chaos`.
std::vector<float> smoothLatticeOnce(const std::vector<float>& grid, std::uint32_t rows, std::uint32_t cols) {
    std::vector<float> next(grid.size());
    for (std::uint32_t row = 0; row < rows; ++row) {
        for (std::uint32_t col = 0; col < cols; ++col) {
            const float neighborAverage = (gridValueAt(grid, rows, cols, static_cast<int>(row) - 1,
                                                        static_cast<int>(col)) +
                                            gridValueAt(grid, rows, cols, static_cast<int>(row) + 1,
                                                        static_cast<int>(col)) +
                                            gridValueAt(grid, rows, cols, static_cast<int>(row),
                                                        static_cast<int>(col) - 1) +
                                            gridValueAt(grid, rows, cols, static_cast<int>(row),
                                                        static_cast<int>(col) + 1)) /
                                           4.0f;
            const float current = grid[std::size_t{row} * cols + col];
            next[std::size_t{row} * cols + col] = 0.5f * current + 0.5f * neighborAverage;
        }
    }
    return next;
}

/// @brief Bilinearly samples `grid` (`rows` x `cols`) at the fractional
/// position corresponding to `(bin, frame)` within a `binCount` x
/// `frameCount` output canvas.
float bilinearSample(const std::vector<float>& grid, std::uint32_t rows, std::uint32_t cols, std::uint32_t binCount,
                      std::uint32_t frameCount, std::uint32_t bin, std::uint32_t frame) {
    const double u = frameCount > 1 ? static_cast<double>(frame) / (frameCount - 1) * (cols - 1) : 0.0;
    const double v = binCount > 1 ? static_cast<double>(bin) / (binCount - 1) * (rows - 1) : 0.0;
    const auto col0 = static_cast<int>(std::floor(u));
    const auto row0 = static_cast<int>(std::floor(v));
    const double colFrac = u - col0;
    const double rowFrac = v - row0;

    const float topLeft = gridValueAt(grid, rows, cols, row0, col0);
    const float topRight = gridValueAt(grid, rows, cols, row0, col0 + 1);
    const float bottomLeft = gridValueAt(grid, rows, cols, row0 + 1, col0);
    const float bottomRight = gridValueAt(grid, rows, cols, row0 + 1, col0 + 1);
    const double top = topLeft + (topRight - topLeft) * colFrac;
    const double bottom = bottomLeft + (bottomRight - bottomLeft) * colFrac;
    return static_cast<float>(top + (bottom - top) * rowFrac);
}

/**
 * @brief `GeneratorFamily::Lattice`'s own algorithm: a coarse grid of
 * cells, seeded with independent random noise from `config.seed`, then
 * smoothed toward its own local neighbor-average for a number of
 * iterations set by `config.orderChaos` - `0` iterations at full chaos
 * (pure noise, broadband and unstructured), progressively more at
 * increasing order (each pass blurs the texture toward simpler, smoother,
 * more self-similar structure - the classic coupled-map-lattice
 * behavior), then bilinearly upsampled onto the full canvas so the result
 * reads as smooth, organic blobs rather than independent per-pixel
 * static. The design doc's own "near the middle, the rich, complex-but-
 * coherent texture most generators are actually used for" is exactly
 * this lattice after a handful (not zero, not many) smoothing passes.
 */
sound_mind::codec::StreamImage generateLatticeContent(const GeneratorConfiguration& config,
                                                       const sound_mind::codec::StreamCodecConfig& codecConfig,
                                                       std::uint32_t frameCount) {
    sound_mind::codec::StreamImage result;
    result.config = codecConfig;
    result.frameCount = frameCount;
    result.sampleCount = std::uint64_t{frameCount} * codecConfig.hopLength;
    const std::size_t cellCount = std::size_t{codecConfig.binCount} * frameCount;
    result.leftMagnitudeDb.resize(cellCount);
    result.rightMagnitudeDb.resize(cellCount);
    result.sharedPhaseRadians.resize(cellCount);

    std::mt19937_64 rng(config.seed);
    // A moderately loud, still-clearly-textured dB range - full silence
    // to full scale would make most of the texture inaudible/invisible
    // once averaged toward order.
    std::uniform_real_distribution<float> magnitudeDist(-60.0f, -6.0f);
    std::uniform_real_distribution<float> phaseDist(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);

    // A coarser lattice than the full pixel grid - see this function's
    // own docs on why the result is upsampled afterward, not evaluated
    // cell-for-pixel.
    const std::uint32_t latticeBinCount = std::max(std::uint32_t{4}, codecConfig.binCount / 8);
    const std::uint32_t latticeFrameCount = std::max(std::uint32_t{4}, frameCount / 8);

    std::vector<float> grid(std::size_t{latticeBinCount} * latticeFrameCount);
    for (float& cell : grid) {
        cell = magnitudeDist(rng);
    }

    // orderChaos in [-1, 1]: -1 (pure chaos) -> 0 smoothing passes; +1
    // (pure order) -> kMaxIterations passes. 12 passes already converges
    // a small lattice to a near-flat, rigid-looking result - more would
    // only waste time re-smoothing an already-settled texture.
    constexpr int kMaxIterations = 12;
    const double orderFraction = std::clamp((config.orderChaos + 1.0) / 2.0, 0.0, 1.0);
    const int iterations = static_cast<int>(std::lround(orderFraction * kMaxIterations));
    for (int i = 0; i < iterations; ++i) {
        grid = smoothLatticeOnce(grid, latticeBinCount, latticeFrameCount);
    }

    for (std::uint32_t bin = 0; bin < codecConfig.binCount; ++bin) {
        for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
            const float value =
                bilinearSample(grid, latticeBinCount, latticeFrameCount, codecConfig.binCount, frameCount, bin, frame);
            const std::size_t index = cellIndex(bin, frame, frameCount);
            result.leftMagnitudeDb[index] = value;
            result.rightMagnitudeDb[index] = value;
            // Drawn from the same seeded stream the magnitude noise
            // itself came from, so the whole result - magnitude and
            // phase alike - stays deterministic from config.seed.
            result.sharedPhaseRadians[index] = phaseDist(rng);
        }
    }

    return result;
}

}  // namespace

sound_mind::codec::StreamImage generateContent(const GeneratorConfiguration& config, const ProjectSettings& settings) {
    const sound_mind::codec::StreamCodecConfig codecConfig = streamCodecConfigFor(settings);
    const std::uint32_t frameCount = settings.canvasWidth;

    switch (config.family) {
        case GeneratorFamily::Lattice:
            return generateLatticeContent(config, codecConfig, frameCount);
        case GeneratorFamily::Fractal:
        case GeneratorFamily::Streaming:
        default:
            return silentResult(codecConfig, frameCount);
    }
}

}  // namespace sound_mind::core
