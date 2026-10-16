#include "sound_mind/core/generator.h"
#include "sound_mind/codec/parallel.h"

#include <algorithm>
#include <limits>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>
#include <vector>

#include "sound_mind/core/gradient.h"
#include "sound_mind/core/mind_wave.h"
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

/// @brief One tone line for the Fractal/Streaming families: from
/// `startFrame` for `frames` frames, centred on (fractional) bin `startBin`
/// and drifting `driftBinsPerFrame` per frame, at `levelDb`.
struct Partial {
    double startFrame = 0.0;
    double frames = 0.0;
    double startBin = 0.0;
    double driftBinsPerFrame = 0.0;
    double levelDb = -12.0;
};

/// @brief Accumulates tone lines into linear amplitudes, keeping each cell's
/// phase from its loudest contribution, then writes them out as dB.
class ToneCanvas {
public:
    ToneCanvas(const sound_mind::codec::StreamCodecConfig& config, std::uint32_t frameCount)
        : config_(config),
          frameCount_(frameCount),
          amplitude_(std::size_t{config.binCount} * frameCount, 0.0f),
          loudest_(amplitude_.size(), 0.0f),
          phase_(amplitude_.size(), 0.0f) {}

    /// @brief Draws `partial` as a narrow Gaussian ridge with a short fade
    /// (10% of its length) in and out - see drawTrajectory().
    void draw(const Partial& partial, double initialPhase) {
        const auto first = static_cast<std::int64_t>(std::ceil(partial.startFrame));
        std::vector<double> bins;
        std::vector<double> gains;
        const double fade = std::max(1.0, 0.1 * partial.frames);
        const double gain = std::pow(10.0, partial.levelDb / 20.0);
        for (std::int64_t frame = first; static_cast<double>(frame) < partial.startFrame + partial.frames; ++frame) {
            const double t = static_cast<double>(frame) - partial.startFrame;
            bins.push_back(partial.startBin + partial.driftBinsPerFrame * t);
            gains.push_back(gain * std::min({1.0, (t + 1.0) / fade, (partial.frames - t) / fade}));
        }
        drawTrajectory(first, bins, gains, initialPhase);
    }

    /**
     * @brief Draws one tone following `bins[i]` (fractional centre bin) at
     * linear amplitude `gains[i]` in frame `firstFrame + i`: a narrow
     * Gaussian ridge (about 1.4 bins wide) whose phase advances each frame
     * by `2*pi*f*hop/sampleRate` at its current frequency, so it decodes as
     * one clean, continuous tone even as it glides. Frames off the canvas
     * are skipped (their phase still advances).
     */
    void drawTrajectory(std::int64_t firstFrame, const std::vector<double>& bins, const std::vector<double>& gains,
                        double initialPhase) {
        const double phasePerHz = 2.0 * std::numbers::pi * config_.hopLength / config_.sampleRateHz;
        double phase = initialPhase;
        for (std::size_t i = 0; i < bins.size(); ++i) {
            const std::int64_t frame = firstFrame + static_cast<std::int64_t>(i);
            const double centre = bins[i];
            if (i > 0) {
                phase += phasePerHz * binIndexToFrequency(static_cast<float>(centre), config_);
            }
            if (frame < 0 || frame >= static_cast<std::int64_t>(frameCount_)) {
                continue;
            }
            const auto lowBin = static_cast<std::int64_t>(std::floor(centre)) - 2;
            for (std::int64_t bin = lowBin; bin <= lowBin + 5; ++bin) {
                if (bin < 0 || bin >= static_cast<std::int64_t>(config_.binCount)) {
                    continue;
                }
                const double distance = (static_cast<double>(bin) - centre) / 0.6;
                const auto value = static_cast<float>(gains[i] * std::exp(-0.5 * distance * distance));
                if (value < 1e-7f) {
                    continue;
                }
                const std::size_t cell =
                    cellIndex(static_cast<std::uint32_t>(bin), static_cast<std::uint32_t>(frame), frameCount_);
                amplitude_[cell] += value;
                if (value > loudest_[cell]) {
                    loudest_[cell] = value;
                    phase_[cell] = static_cast<float>(std::remainder(phase, 2.0 * std::numbers::pi));
                }
            }
        }
    }

    /// @brief The accumulated tones as a layer's content, silent elsewhere.
    [[nodiscard]] sound_mind::codec::StreamImage finish() const {
        sound_mind::codec::StreamImage result = silentResult(config_, frameCount_);
        for (std::size_t cell = 0; cell < amplitude_.size(); ++cell) {
            if (amplitude_[cell] > 0.0f) {
                result.leftMagnitudeDb[cell] = std::max(kNoiseSilenceDb, 20.0f * std::log10(amplitude_[cell]));
                result.rightMagnitudeDb[cell] = result.leftMagnitudeDb[cell];
                result.sharedPhaseRadians[cell] = phase_[cell];
            }
        }
        return result;
    }

private:
    sound_mind::codec::StreamCodecConfig config_;
    std::uint32_t frameCount_;
    std::vector<float> amplitude_;
    std::vector<float> loudest_;
    std::vector<float> phase_;
};

/**
 * @brief `GeneratorFamily::Fractal` (Pre-Beta Plan `v0.Y.79.1`): a seeded
 * branching grammar of tones - see the design doc's "Generators".
 *
 * A sustained trunk tone spans the canvas; every branch spawns children
 * part-way along it, each starting at a pitch interval from its parent and
 * lasting part of what's left, up to `kMaxDepth` levels deep, each level
 * about 6 dB quieter. `config.orderChaos` blends every choice:
 * - toward **order**: two children at evenly spaced points, harmonic
 *   intervals (octave, fifth, third, octave below, twelfth), steady pitch,
 *   on exact bins - rigid, repetitive structure;
 * - toward **chaos**: one to four children at random points, arbitrary
 *   intervals up to two octaves either way, random lengths and pitch drift;
 * - in between, branching melodic and harmonic shapes that vary rather
 *   than repeat.
 */
sound_mind::codec::StreamImage generateFractalContent(const GeneratorConfiguration& config,
                                                       const sound_mind::codec::StreamCodecConfig& codecConfig,
                                                       std::uint32_t frameCount) {
    constexpr int kMaxDepth = 5;
    constexpr std::array<double, 5> kHarmonicSemitones{12.0, 7.0, 4.0, -12.0, 19.0};
    const double chaos = 1.0 - std::clamp((config.orderChaos + 1.0) / 2.0, 0.0, 1.0);
    const double binsPerOctave =
        static_cast<double>(codecConfig.binCount > 1 ? codecConfig.binCount - 1 : 1) /
        std::log2(std::max(1.0001, static_cast<double>(codecConfig.maxFrequencyHz) / codecConfig.minFrequencyHz));
    std::mt19937_64 rng(config.seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    const auto mix = [chaos](double ordered, double chaotic) { return ordered + chaos * (chaotic - ordered); };

    std::vector<Partial> partials;
    Partial trunk;
    trunk.frames = frameCount;
    trunk.startBin = std::round(codecConfig.binCount * (0.25 + 0.25 * unit(rng)));
    partials.push_back(trunk);

    // Breadth-first growth: the seed's random sequence is consumed in one
    // fixed order.
    std::vector<std::pair<std::size_t, int>> frontier{{0, 0}};
    for (std::size_t next = 0; next < frontier.size(); ++next) {
        const auto [parentIndex, depth] = frontier[next];
        const Partial parent = partials[parentIndex];
        if (depth >= kMaxDepth || parent.frames < 8.0) {
            continue;
        }
        const int children = std::clamp(static_cast<int>(std::lround(mix(2.0, 1.0 + 3.0 * unit(rng)))), 1, 4);
        for (int child = 0; child < children; ++child) {
            const double position = mix((child + 1.0) / (children + 1.0), unit(rng));
            Partial branch;
            branch.startFrame = parent.startFrame + position * parent.frames;
            const double remaining = parent.startFrame + parent.frames - branch.startFrame;
            branch.frames = remaining * mix(0.6, 0.3 + 0.6 * unit(rng));
            const std::size_t pick =
                static_cast<std::size_t>(unit(rng) * static_cast<double>(kHarmonicSemitones.size())) %
                kHarmonicSemitones.size();
            const double semitones = mix(kHarmonicSemitones[pick], (unit(rng) * 2.0 - 1.0) * 24.0);
            const double parentBinThere = parent.startBin + parent.driftBinsPerFrame * position * parent.frames;
            branch.startBin = parentBinThere + semitones / 12.0 * binsPerOctave;
            branch.driftBinsPerFrame = chaos * (unit(rng) * 2.0 - 1.0) * binsPerOctave / std::max(1.0, branch.frames);
            if (branch.driftBinsPerFrame == 0.0) {
                branch.startBin = std::round(branch.startBin);  // a rigid tone sits exactly on a bin.
            }
            branch.levelDb = parent.levelDb - 6.0 - chaos * 6.0 * unit(rng);
            if (branch.startBin < 0.0 || branch.startBin > codecConfig.binCount - 1.0 || branch.frames < 2.0) {
                continue;
            }
            partials.push_back(branch);
            frontier.emplace_back(partials.size() - 1, depth + 1);
        }
    }

    ToneCanvas canvas(codecConfig, frameCount);
    for (const Partial& partial : partials) {
        canvas.draw(partial, (unit(rng) * 2.0 - 1.0) * std::numbers::pi);
    }
    return canvas.finish();
}

/**
 * @brief `GeneratorFamily::Streaming` (Pre-Beta Plan `v0.Y.79.1`): a
 * genuinely time-varying signal - see the design doc's "Generators".
 *
 * `kVoices` voices, spread across the frequency range, are driven by a
 * coupled logistic-map lattice, `x_k <- (1-eps) f(x_k) + eps/2 (f(x_{k-1}) +
 * f(x_{k+1}))` with `f(x) = r x (1-x)`, advanced every `kStepFrames` frames
 * and interpolated between steps so each voice glides. A voice's state sets
 * its pitch (up to half an octave either side of its own register) and its
 * neighbor's state its level. `config.orderChaos` sets `r`: about 2.9
 * toward order (the lattice settles to a fixed point - a steady drone),
 * the edge of chaos (about 3.57) at the middle (evolving, but coherent),
 * and 4.0 toward chaos (wandering without settling).
 */
sound_mind::codec::StreamImage generateStreamingContent(const GeneratorConfiguration& config,
                                                         const sound_mind::codec::StreamCodecConfig& codecConfig,
                                                         std::uint32_t frameCount) {
    constexpr int kVoices = 8;
    constexpr int kStepFrames = 4;
    constexpr double kCoupling = 0.3;
    constexpr double kFadeFrames = 10.0;
    const double chaos = 1.0 - std::clamp((config.orderChaos + 1.0) / 2.0, 0.0, 1.0);
    const double r = chaos <= 0.5 ? 2.9 + (3.57 - 2.9) * 2.0 * chaos : 3.57 + (4.0 - 3.57) * 2.0 * (chaos - 0.5);
    const double binsPerOctave =
        static_cast<double>(codecConfig.binCount > 1 ? codecConfig.binCount - 1 : 1) /
        std::log2(std::max(1.0001, static_cast<double>(codecConfig.maxFrequencyHz) / codecConfig.minFrequencyHz));
    std::mt19937_64 rng(config.seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    std::vector<double> state(kVoices);
    for (double& x : state) {
        x = 0.1 + 0.8 * unit(rng);
    }
    const auto logistic = [r](double x) { return r * x * (1.0 - x); };
    const int steps = static_cast<int>(frameCount) / kStepFrames + 2;
    std::vector<std::vector<double>> history(static_cast<std::size_t>(steps), std::vector<double>(kVoices));
    for (int step = 0; step < steps; ++step) {
        history[static_cast<std::size_t>(step)] = state;
        std::vector<double> next(kVoices);
        for (int k = 0; k < kVoices; ++k) {
            const double left = logistic(state[static_cast<std::size_t>((k + kVoices - 1) % kVoices)]);
            const double right = logistic(state[static_cast<std::size_t>((k + 1) % kVoices)]);
            next[static_cast<std::size_t>(k)] =
                std::clamp((1.0 - kCoupling) * logistic(state[static_cast<std::size_t>(k)]) +
                               kCoupling / 2.0 * (left + right),
                           1e-6, 1.0 - 1e-6);
        }
        state = std::move(next);
    }

    ToneCanvas canvas(codecConfig, frameCount);
    for (int k = 0; k < kVoices; ++k) {
        const double home = (0.2 + 0.6 * (k + 0.5) / kVoices) * (codecConfig.binCount - 1);
        std::vector<double> bins(frameCount);
        std::vector<double> gains(frameCount);
        for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
            const auto step = static_cast<std::size_t>(frame / kStepFrames);
            const double along = static_cast<double>(frame % kStepFrames) / kStepFrames;
            const auto at = [&](std::size_t s, int voice) { return history[s][static_cast<std::size_t>(voice)]; };
            const double x = at(step, k) + along * (at(step + 1, k) - at(step, k));
            const int neighbor = (k + 1) % kVoices;
            const double y = at(step, neighbor) + along * (at(step + 1, neighbor) - at(step, neighbor));
            bins[frame] = home + (x - 0.5) * binsPerOctave;  // +-half an octave.
            const double fade = std::min({1.0, (frame + 1.0) / kFadeFrames, (frameCount - frame) / kFadeFrames});
            gains[frame] = std::pow(10.0, (-18.0 - 18.0 * (1.0 - y)) / 20.0) * fade;
        }
        canvas.drawTrajectory(0, bins, gains, (unit(rng) * 2.0 - 1.0) * std::numbers::pi);
    }
    return canvas.finish();
}

/// @brief SplitMix64's finalizer - a fast, well-mixed 64-bit hash.
std::uint64_t mix64(std::uint64_t value) noexcept {
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31);
}

/// @brief The per-cell parameter of a bound noise setting: `baseline` where
/// the MindWave reads `0`, `value` where it reads `1` - see
/// `NoiseConfiguration`'s docs. Unbound (an empty field), always `value`.
double boundValue(const std::vector<float>& field, std::size_t cell, double baseline, double value) {
    if (field.empty()) {
        return value;
    }
    return baseline + static_cast<double>(field[cell]) * (value - baseline);
}

/// @brief `id`'s MindWave evaluated across the whole canvas, or an empty
/// field when unbound or unresolvable.
std::vector<float> fieldFor(const std::optional<MindWaveId>& id, const MindWaveResolver& resolveMindWave,
                            const sound_mind::codec::StreamCodecConfig& config, std::uint32_t frameCount) {
    if (!id.has_value() || !resolveMindWave) {
        return {};
    }
    const MindWave* wave = resolveMindWave(*id);
    return wave != nullptr ? evaluateMindWaveField(*wave, config, frameCount) : std::vector<float>{};
}

/**
 * @brief `GeneratorFamily::Noise` - see `NoiseConfiguration`'s docs.
 *
 * Each canvas cell belongs to the noise cell `(frame / horizontal, bin /
 * vertical)` (resolutions per cell when bound), whose amplitude and phase
 * come from `hashedUnitNoise()` - so cells are independent of evaluation
 * order and the result replays exactly. A named color's level at a bin is
 * `levelDb + 10*alpha*log10(f / 1 kHz)` plus the codec's A-weighting at
 * `f` (where an encoded recording of that noise would read), except
 * `Gray`, which is flat by construction. The random amplitude is
 * Rayleigh-distributed with unit mean power, as an STFT of real noise is.
 */
sound_mind::codec::StreamImage generateNoiseContent(const GeneratorConfiguration& config,
                                                     const sound_mind::codec::StreamCodecConfig& codecConfig,
                                                     std::uint32_t frameCount,
                                                     const MindWaveResolver& resolveMindWave) {
    const NoiseConfiguration& noise = config.noise;
    sound_mind::codec::StreamImage result = silentResult(codecConfig, frameCount);
    const std::vector<float> horizontalField =
        fieldFor(noise.horizontalResolutionMindWave, resolveMindWave, codecConfig, frameCount);
    const std::vector<float> verticalField =
        fieldFor(noise.verticalResolutionMindWave, resolveMindWave, codecConfig, frameCount);
    const std::vector<float> spectrumField =
        noise.color == NoiseColor::Custom
            ? fieldFor(noise.customSpectrumMindWave, resolveMindWave, codecConfig, frameCount)
            : std::vector<float>{};
    const double alpha = noiseColorExponent(noise.color);
    const double binSpan = codecConfig.binCount > 1 ? static_cast<double>(codecConfig.binCount - 1) : 1.0;

    // Every cell is independent (its randomness is hashed from its own
    // position), so frequency rows run in parallel (v0.1.41.5), each
    // writing only its own cells.
    sound_mind::codec::parallelFor(codecConfig.binCount, 8, [&](std::size_t rowBegin, std::size_t rowEnd) {
    for (auto bin = static_cast<std::uint32_t>(rowBegin); bin < rowEnd; ++bin) {
        const float frequencyHz = binIndexToFrequency(static_cast<float>(bin), codecConfig);
        float leftShapeDb = 0.0f;
        float rightShapeDb = 0.0f;
        if (noise.color == NoiseColor::Custom) {
            const GradientStop stop = noise.customSpectrum.evaluate(static_cast<float>(bin / binSpan));
            leftShapeDb = stop.leftIntensity;
            rightShapeDb = stop.rightIntensity;
        } else {
            float shape = noise.levelDb;
            if (noise.color != NoiseColor::Gray) {
                shape += static_cast<float>(10.0 * alpha * std::log10(frequencyHz / 1000.0)) +
                         sound_mind::codec::aWeightingOffsetDb(frequencyHz);
            }
            leftShapeDb = shape;
            rightShapeDb = shape;
        }
        for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
            const std::size_t cell = cellIndex(bin, frame, frameCount);
            const double horizontal = std::max(1.0, boundValue(horizontalField, cell, 1.0, noise.horizontalResolution));
            const double vertical = std::max(1.0, boundValue(verticalField, cell, 1.0, noise.verticalResolution));
            const auto cellX = static_cast<std::int64_t>(std::floor(frame / horizontal));
            const auto cellY = static_cast<std::int64_t>(std::floor(bin / vertical));
            const auto channelDb = [&](std::uint64_t stream, float shapeDb) {
                const double level =
                    boundValue(spectrumField, cell, kNoiseSilenceDb, static_cast<double>(shapeDb));
                if (level <= kNoiseSilenceDb) {
                    return kNoiseSilenceDb;  // silence stays silent - no random spread above the floor.
                }
                // Rayleigh with E[r^2] = 1: r = sqrt(-ln(1 - u)).
                const double u = hashedUnitNoise(config.seed, stream, cellX, cellY);
                const double rayleigh = std::sqrt(-std::log(std::max(1e-12, 1.0 - u)));
                return static_cast<float>(
                    std::max<double>(kNoiseSilenceDb, level + 20.0 * std::log10(std::max(1e-6, rayleigh))));
            };
            result.leftMagnitudeDb[cell] = channelDb(1, leftShapeDb);
            result.rightMagnitudeDb[cell] = channelDb(2, rightShapeDb);
            result.sharedPhaseRadians[cell] = static_cast<float>(
                (hashedUnitNoise(config.seed, 3, cellX, cellY) * 2.0 - 1.0) * std::numbers::pi);
        }
    }
    });
    return result;
}

}  // namespace

double hashedUnitNoise(std::uint64_t seed, std::uint64_t stream, std::int64_t x, std::int64_t y) noexcept {
    std::uint64_t hash = mix64(seed ^ mix64(stream));
    hash = mix64(hash ^ static_cast<std::uint64_t>(x));
    hash = mix64(hash ^ static_cast<std::uint64_t>(y));
    return static_cast<double>(hash >> 11) * (1.0 / 9007199254740992.0);  // 53 bits -> [0, 1).
}

double coloredNoiseSample(std::uint64_t seed, std::int64_t row, std::int64_t n, double alpha) noexcept {
    const auto white = [&](std::uint64_t stream, std::int64_t step) {
        return hashedUnitNoise(seed, stream, step, row) - 0.5;  // in [-0.5, 0.5).
    };
    if (alpha >= 0.0) {
        // White blended toward its own first difference (anti-correlated).
        const double toward = std::clamp(alpha / 2.0, 0.0, 1.0);
        const double value = (1.0 - toward) * white(0, n) + toward * 0.5 * (white(0, n) - white(0, n - 1));
        return std::clamp(0.5 + value, 0.0, 1.0);
    }
    // Octave generators held over 2^k steps, weighted so the summed spectrum
    // falls as f^alpha (beta = -alpha).
    constexpr int kOctaves = 10;
    const double beta = -alpha;
    // The weights depend only on beta, which a whole field shares: cache the
    // last one's per thread (v0.1.41.5) - the same pow() calls, made once per
    // field instead of ten per cell, so the values are bit-identical.
    struct OctaveWeights {
        double beta = std::numeric_limits<double>::quiet_NaN();
        std::array<double, kOctaves> weight{};
        double spread = 1.0;
    };
    thread_local OctaveWeights cached;
    if (!(cached.beta == beta)) {
        double weightSquares = 0.0;
        for (int k = 0; k < kOctaves; ++k) {
            cached.weight[static_cast<std::size_t>(k)] = std::pow(2.0, k * (beta - 1.0) / 2.0);
            weightSquares += cached.weight[static_cast<std::size_t>(k)] * cached.weight[static_cast<std::size_t>(k)];
        }
        // A uniform on [-0.5, 0.5) has standard deviation 1/sqrt(12); three
        // standard deviations of the sum span [0, 1].
        cached.spread = 3.0 * std::sqrt(weightSquares / 12.0);
        cached.beta = beta;
    }
    double sum = 0.0;
    for (int k = 0; k < kOctaves; ++k) {
        const auto held = static_cast<std::int64_t>(std::floor(static_cast<double>(n) / std::ldexp(1.0, k)));
        sum += cached.weight[static_cast<std::size_t>(k)] * white(static_cast<std::uint64_t>(1 + k), held);
    }
    return std::clamp(0.5 + 0.5 * sum / cached.spread, 0.0, 1.0);
}

sound_mind::codec::StreamImage generateContent(const GeneratorConfiguration& config, const ProjectSettings& settings,
                                               const MindWaveResolver& resolveMindWave) {
    const sound_mind::codec::StreamCodecConfig codecConfig = streamCodecConfigFor(settings);
    const std::uint32_t frameCount = settings.canvasWidth;

    switch (config.family) {
        case GeneratorFamily::Lattice:
            return generateLatticeContent(config, codecConfig, frameCount);
        case GeneratorFamily::Noise:
            return generateNoiseContent(config, codecConfig, frameCount, resolveMindWave);
        case GeneratorFamily::Fractal:
            return generateFractalContent(config, codecConfig, frameCount);
        case GeneratorFamily::Streaming:
            return generateStreamingContent(config, codecConfig, frameCount);
        default:
            return silentResult(codecConfig, frameCount);
    }
}

}  // namespace sound_mind::core
