#include "sound_mind/core/mind_wave.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

namespace {

/// @brief The smallest period `evaluate()` will actually divide by -
/// defensive only (avoids a literal divide-by-zero/NaN for a period at or
/// below zero), not a musically meaningful minimum cycle length.
constexpr double kMinimumPeriod = 1e-6;

// --- Hand-rolled deterministic value noise ---------------------------
//
// Confirmed with the user (v0.Y.31.1 Installment B): a small, seedable,
// from-scratch noise primitive rather than a new third-party dependency,
// satisfying docs/sound-mind-architecture.md's own "anything seeded must
// replay bit-for-bit on the same Studio version and architecture" rule.
// Pure integer/double arithmetic throughout - no platform-specific
// intrinsics, so this is identical on Arm64 and x64.

/// @brief A fast integer bit-mixer (xorshift-multiply family) - the same
/// kind of hash commonly used to seed value-noise lattices.
std::uint32_t hashUint32(std::uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

/// @brief Hashes a single integer lattice coordinate against a seed.
std::uint32_t hashCoord(int coordinate, std::uint32_t seed) {
    return hashUint32(static_cast<std::uint32_t>(coordinate) * 0x9e3779b1U ^ hashUint32(seed));
}

/// @brief Hashes a 2D integer lattice coordinate against a seed.
std::uint32_t hashCoord2D(int x, int y, std::uint32_t seed) {
    return hashUint32(hashCoord(x, seed) ^ hashUint32(static_cast<std::uint32_t>(y) * 0x85ebca6bU));
}

/// @brief Maps a 32-bit hash uniformly onto `[-1, 1]`.
double hashToUnitSigned(std::uint32_t h) {
    return (static_cast<double>(h) / static_cast<double>(0xFFFFFFFFU)) * 2.0 - 1.0;
}

/// @brief Hermite smoothing (`3t^2 - 2t^3`) - the standard interpolant for
/// value noise, avoiding visible lattice-aligned creases a plain linear
/// interpolation would leave.
double smoothstep(double t) { return t * t * (3.0 - 2.0 * t); }

/// @brief 1D value noise: smoothly interpolates hashed lattice values,
/// in `[-1, 1]`.
double valueNoise1D(double x, std::uint32_t seed) {
    const double floorX = std::floor(x);
    const int i0 = static_cast<int>(floorX);
    const double t = smoothstep(x - floorX);
    const double v0 = hashToUnitSigned(hashCoord(i0, seed));
    const double v1 = hashToUnitSigned(hashCoord(i0 + 1, seed));
    return v0 + (v1 - v0) * t;
}

/// @brief 2D value noise: bilinearly interpolates hashed lattice values,
/// in `[-1, 1]`.
double valueNoise2D(double x, double y, std::uint32_t seed) {
    const double floorX = std::floor(x);
    const double floorY = std::floor(y);
    const int ix0 = static_cast<int>(floorX);
    const int iy0 = static_cast<int>(floorY);
    const double tx = smoothstep(x - floorX);
    const double ty = smoothstep(y - floorY);
    const double v00 = hashToUnitSigned(hashCoord2D(ix0, iy0, seed));
    const double v10 = hashToUnitSigned(hashCoord2D(ix0 + 1, iy0, seed));
    const double v01 = hashToUnitSigned(hashCoord2D(ix0, iy0 + 1, seed));
    const double v11 = hashToUnitSigned(hashCoord2D(ix0 + 1, iy0 + 1, seed));
    const double vx0 = v00 + (v10 - v00) * tx;
    const double vx1 = v01 + (v11 - v01) * tx;
    return vx0 + (vx1 - vx0) * ty;
}

/// @brief 1D fractal Brownian motion: sums `octaves` of `valueNoise1D` at
/// doubling frequency and shrinking (`persistence`) amplitude, normalized
/// back to `[-1, 1]`.
double fractalBrownianMotion1D(double x, std::uint32_t seed, int octaves, double persistence) {
    const int safeOctaves = std::max(1, octaves);
    double total = 0.0;
    double amplitude = 1.0;
    double frequency = 1.0;
    double maxAmplitude = 0.0;
    for (int octave = 0; octave < safeOctaves; ++octave) {
        total += valueNoise1D(x * frequency, seed + static_cast<std::uint32_t>(octave)) * amplitude;
        maxAmplitude += amplitude;
        amplitude *= persistence;
        frequency *= 2.0;
    }
    return maxAmplitude > 0.0 ? total / maxAmplitude : 0.0;
}

/// @brief 2D fractal Brownian motion - see `fractalBrownianMotion1D()`.
double fractalBrownianMotion2D(double x, double y, std::uint32_t seed, int octaves, double persistence) {
    const int safeOctaves = std::max(1, octaves);
    double total = 0.0;
    double amplitude = 1.0;
    double frequency = 1.0;
    double maxAmplitude = 0.0;
    for (int octave = 0; octave < safeOctaves; ++octave) {
        total += valueNoise2D(x * frequency, y * frequency, seed + static_cast<std::uint32_t>(octave)) * amplitude;
        maxAmplitude += amplitude;
        amplitude *= persistence;
        frequency *= 2.0;
    }
    return maxAmplitude > 0.0 ? total / maxAmplitude : 0.0;
}

/// @brief Worley/Voronoi "F1" noise: the distance from `(x, y)` (in cell
/// units, i.e. already divided by cell size) to the nearest of a hashed
/// per-cell feature point, searched across the 3x3 neighborhood of cells.
double worleyF1Distance(double cellX, double cellY, std::uint32_t seed) {
    const int baseX = static_cast<int>(std::floor(cellX));
    const int baseY = static_cast<int>(std::floor(cellY));
    double minDistance = std::numeric_limits<double>::max();
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int cellXIndex = baseX + dx;
            const int cellYIndex = baseY + dy;
            const std::uint32_t h = hashCoord2D(cellXIndex, cellYIndex, seed);
            const double offsetX = static_cast<double>(h & 0xFFFFu) / 65535.0;
            const double offsetY = static_cast<double>((h >> 16) & 0xFFFFu) / 65535.0;
            const double featureX = static_cast<double>(cellXIndex) + offsetX;
            const double featureY = static_cast<double>(cellYIndex) + offsetY;
            const double deltaX = cellX - featureX;
            const double deltaY = cellY - featureY;
            minDistance = std::min(minDistance, std::sqrt(deltaX * deltaX + deltaY * deltaY));
        }
    }
    return minDistance;
}

/// @brief Recursive midpoint displacement (confirmed with the user for
/// `GeneratorType::Fractal`): starts from fixed endpoints and recursively
/// displaces each segment's own midpoint by a hashed random offset that
/// shrinks by `roughness` at each successive level, producing a
/// self-similar, jagged curve. `nodeIndex` uniquely identifies each
/// midpoint node in the recursion tree (root's left child is `0`, right
/// child `1`, and so on), so the same `(level, nodeIndex, seed)` always
/// hashes to the same displacement - the recursion needs no shared state
/// or precomputed array, only integer bookkeeping.
double midpointDisplacement(double t, double left, double right, double leftValue, double rightValue,
                             int remainingDepth, int level, std::uint32_t nodeIndex, double roughness,
                             std::uint32_t seed) {
    if (remainingDepth <= 0) {
        const double span = right - left;
        const double localT = span > 0.0 ? (t - left) / span : 0.0;
        return leftValue + (rightValue - leftValue) * localT;
    }
    const double mid = (left + right) / 2.0;
    const double amplitude = std::pow(roughness, static_cast<double>(level + 1));
    const std::uint32_t midHash = hashCoord2D(static_cast<int>(nodeIndex), level, seed);
    const double midValue = (leftValue + rightValue) / 2.0 + hashToUnitSigned(midHash) * amplitude;
    if (t < mid) {
        return midpointDisplacement(t, left, mid, leftValue, midValue, remainingDepth - 1, level + 1, nodeIndex * 2,
                                     roughness, seed);
    }
    return midpointDisplacement(t, mid, right, midValue, rightValue, remainingDepth - 1, level + 1,
                                 nodeIndex * 2 + 1, roughness, seed);
}

/// @brief Folds `member` into `running`, per `SuperpositionBlendMode`'s
/// own docs.
double foldSuperposition(SuperpositionBlendMode mode, double running, double member) {
    switch (mode) {
        case SuperpositionBlendMode::Multiply:
            return running * member;
        case SuperpositionBlendMode::Add:
            return running + member;
        case SuperpositionBlendMode::Min:
            return std::min(running, member);
        case SuperpositionBlendMode::Max:
            return std::max(running, member);
        case SuperpositionBlendMode::Average:
            return (running + member) / 2.0;
    }
    return running;  // Unreachable - defensive only.
}

}  // namespace

float MindWave::evaluate(TimeFrequencyPoint point, const sound_mind::codec::StreamCodecConfig& config) const {
    const double safePeriod = std::max(kMinimumPeriod, period_);
    const double binIndex = static_cast<double>(frequencyToBinIndex(static_cast<float>(point.frequencyHz), config));
    const double axisPosition = (axis_ == MindWaveAxis::Time) ? point.timeSeconds : binIndex;

    double result = 0.5;  // Overwritten by every real branch below.

    switch (type_) {
        case GeneratorType::Periodic: {
            const double phase = 2.0 * std::numbers::pi_v<double> * (axisPosition / safePeriod) + phaseRadians_;
            switch (periodicWaveform_) {
                case PeriodicWaveform::Sine:
                    // sin() ranges [-1, 1] - rescaled to this class's own
                    // [0, 1] field convention.
                    result = (std::sin(phase) + 1.0) / 2.0;
                    break;
                case PeriodicWaveform::Triangle: {
                    // Aligned to the same phase convention as Sine: 0.5 at
                    // p=0, peaking at p=0.25, 0.5 at p=0.5, troughing at
                    // p=0.75 - rather than the more common (but less
                    // consistent with this class's other waveforms)
                    // convention of peaking at p=0.
                    double p = phase / (2.0 * std::numbers::pi_v<double>);
                    p -= std::floor(p);
                    if (p < 0.25) {
                        result = 0.5 + 2.0 * p;
                    } else if (p < 0.5) {
                        result = 1.0 - 2.0 * (p - 0.25);
                    } else if (p < 0.75) {
                        result = 0.5 - 2.0 * (p - 0.5);
                    } else {
                        result = 2.0 * (p - 0.75);
                    }
                    break;
                }
                case PeriodicWaveform::Square: {
                    double p = phase / (2.0 * std::numbers::pi_v<double>);
                    p -= std::floor(p);
                    result = (p < 0.5) ? 1.0 : 0.0;
                    break;
                }
                case PeriodicWaveform::Sawtooth: {
                    double p = phase / (2.0 * std::numbers::pi_v<double>);
                    p -= std::floor(p);
                    result = p;
                    break;
                }
                case PeriodicWaveform::Pulse: {
                    double p = phase / (2.0 * std::numbers::pi_v<double>);
                    p -= std::floor(p);
                    result = (p < dutyCycle_) ? 1.0 : 0.0;
                    break;
                }
            }
            break;
        }
        case GeneratorType::Envelope: {
            const double relativePosition = axisPosition - envelopeCenter_;
            switch (envelopeShape_) {
                case EnvelopeShape::ExponentialDecay:
                    // Plateaus at 1.0 before envelopeCenter_, decays after it.
                    result = std::exp(-decayRate_ * std::max(0.0, relativePosition));
                    break;
                case EnvelopeShape::DecayingOscillation: {
                    const double decayFactor = std::exp(-decayRate_ * std::max(0.0, relativePosition));
                    const double oscillation =
                        std::cos(2.0 * std::numbers::pi_v<double> * (relativePosition / safePeriod) + phaseRadians_);
                    result = 0.5 + 0.5 * decayFactor * oscillation;
                    break;
                }
                case EnvelopeShape::SCurve:
                    // A logistic sigmoid centered on envelopeCenter_.
                    result = 1.0 / (1.0 + std::exp(-envelopeSteepness_ * relativePosition));
                    break;
            }
            break;
        }
        case GeneratorType::SteppedNoise: {
            switch (steppedNoiseShape_) {
                case SteppedNoiseShape::Stepped: {
                    double p = (axisPosition / safePeriod) + phaseRadians_ / (2.0 * std::numbers::pi_v<double>);
                    p -= std::floor(p);
                    const int safeStepCount = std::max(1, stepCount_);
                    const int level = std::clamp(static_cast<int>(p * safeStepCount), 0, safeStepCount - 1);
                    result = static_cast<double>(level) / static_cast<double>(std::max(1, safeStepCount - 1));
                    break;
                }
                case SteppedNoiseShape::GaussianNoise:
                    result = (valueNoise1D(axisPosition / std::max(kMinimumPeriod, noiseScale_), seed_) + 1.0) / 2.0;
                    break;
                case SteppedNoiseShape::FractalNoise:
                    result = (fractalBrownianMotion1D(axisPosition / std::max(kMinimumPeriod, noiseScale_), seed_,
                                                       noiseOctaves_, noisePersistence_) +
                              1.0) /
                             2.0;
                    break;
            }
            break;
        }
        case GeneratorType::Spatial: {
            const double safeCellSize = std::max(kMinimumPeriod, noiseScale_);
            switch (spatialPattern_) {
                case SpatialPattern::Ripples: {
                    const double deltaX = point.timeSeconds - spatialCenterX_;
                    const double deltaY = binIndex - spatialCenterY_;
                    const double distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
                    result = (std::sin(2.0 * std::numbers::pi_v<double> * (distance / safePeriod) + phaseRadians_) +
                              1.0) /
                             2.0;
                    break;
                }
                case SpatialPattern::Checkerboard: {
                    const auto cellX = static_cast<long long>(std::floor(point.timeSeconds / safePeriod));
                    const auto cellY = static_cast<long long>(std::floor(binIndex / safePeriod));
                    result = ((cellX + cellY) % 2 == 0) ? 1.0 : 0.0;
                    break;
                }
                case SpatialPattern::Cellular: {
                    const double distance =
                        worleyF1Distance(point.timeSeconds / safeCellSize, binIndex / safeCellSize, seed_);
                    // Brighter near a feature point, fading out toward the
                    // cell boundary - normalized so a typical F1 distance
                    // (up to ~0.7 cell-widths) maps into [0, 1].
                    result = 1.0 - std::clamp(distance / 0.7, 0.0, 1.0);
                    break;
                }
                case SpatialPattern::DomainWarpedNoise: {
                    const double warpX =
                        point.timeSeconds +
                        domainWarpStrength_ * valueNoise2D(point.timeSeconds / safeCellSize,
                                                            binIndex / safeCellSize, seed_);
                    const double warpY =
                        binIndex + domainWarpStrength_ * valueNoise2D(point.timeSeconds / safeCellSize,
                                                                       binIndex / safeCellSize, seed_ + 1);
                    result = (fractalBrownianMotion2D(warpX / safeCellSize, warpY / safeCellSize, seed_ + 2,
                                                       noiseOctaves_, noisePersistence_) +
                              1.0) /
                             2.0;
                    break;
                }
            }
            break;
        }
        case GeneratorType::Fractal: {
            double t = axisPosition / safePeriod;
            t -= std::floor(t);
            const int safeIterations = std::max(0, fractalIterations_);
            result = midpointDisplacement(t, 0.0, 1.0, 0.5, 0.5, safeIterations, 0, 0, fractalRoughness_, seed_);
            break;
        }
    }

    for (const MindWave& member : superpositionStack_) {
        result = foldSuperposition(superpositionBlendMode_, result, static_cast<double>(member.evaluate(point, config)));
    }

    return static_cast<float>(std::clamp(result, 0.0, 1.0));
}

void to_json(nlohmann::json& json, const MindWave& mindWave) {
    json = nlohmann::json{{"type", mindWave.type()},
                          {"periodicWaveform", mindWave.periodicWaveform()},
                          {"axis", mindWave.axis()},
                          {"period", mindWave.period()},
                          {"phaseRadians", mindWave.phaseRadians()},
                          {"dutyCycle", mindWave.dutyCycle()},
                          {"envelopeShape", mindWave.envelopeShape()},
                          {"envelopeCenter", mindWave.envelopeCenter()},
                          {"envelopeSteepness", mindWave.envelopeSteepness()},
                          {"decayRate", mindWave.decayRate()},
                          {"steppedNoiseShape", mindWave.steppedNoiseShape()},
                          {"stepCount", mindWave.stepCount()},
                          {"seed", mindWave.seed()},
                          {"noiseScale", mindWave.noiseScale()},
                          {"noiseOctaves", mindWave.noiseOctaves()},
                          {"noisePersistence", mindWave.noisePersistence()},
                          {"spatialPattern", mindWave.spatialPattern()},
                          {"spatialCenterX", mindWave.spatialCenterX()},
                          {"spatialCenterY", mindWave.spatialCenterY()},
                          {"domainWarpStrength", mindWave.domainWarpStrength()},
                          {"fractalRoughness", mindWave.fractalRoughness()},
                          {"fractalIterations", mindWave.fractalIterations()},
                          {"superpositionStack", mindWave.superpositionStack()},
                          {"superpositionBlendMode", mindWave.superpositionBlendMode()}};
}

void from_json(const nlohmann::json& json, MindWave& mindWave) {
    mindWave.setType(json.at("type").get<GeneratorType>());
    mindWave.setPeriodicWaveform(json.at("periodicWaveform").get<PeriodicWaveform>());
    mindWave.setAxis(json.at("axis").get<MindWaveAxis>());
    mindWave.setPeriod(json.at("period").get<double>());
    mindWave.setPhaseRadians(json.at("phaseRadians").get<double>());
    mindWave.setDutyCycle(json.at("dutyCycle").get<double>());
    mindWave.setEnvelopeShape(json.at("envelopeShape").get<EnvelopeShape>());
    mindWave.setEnvelopeCenter(json.at("envelopeCenter").get<double>());
    mindWave.setEnvelopeSteepness(json.at("envelopeSteepness").get<double>());
    mindWave.setDecayRate(json.at("decayRate").get<double>());
    mindWave.setSteppedNoiseShape(json.at("steppedNoiseShape").get<SteppedNoiseShape>());
    mindWave.setStepCount(json.at("stepCount").get<int>());
    mindWave.setSeed(json.at("seed").get<std::uint32_t>());
    mindWave.setNoiseScale(json.at("noiseScale").get<double>());
    mindWave.setNoiseOctaves(json.at("noiseOctaves").get<int>());
    mindWave.setNoisePersistence(json.at("noisePersistence").get<double>());
    mindWave.setSpatialPattern(json.at("spatialPattern").get<SpatialPattern>());
    mindWave.setSpatialCenterX(json.at("spatialCenterX").get<double>());
    mindWave.setSpatialCenterY(json.at("spatialCenterY").get<double>());
    mindWave.setDomainWarpStrength(json.at("domainWarpStrength").get<double>());
    mindWave.setFractalRoughness(json.at("fractalRoughness").get<double>());
    mindWave.setFractalIterations(json.at("fractalIterations").get<int>());
    mindWave.setSuperpositionStack(json.at("superpositionStack").get<std::vector<MindWave>>());
    mindWave.setSuperpositionBlendMode(json.at("superpositionBlendMode").get<SuperpositionBlendMode>());
}

}  // namespace sound_mind::core
