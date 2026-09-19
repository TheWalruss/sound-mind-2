#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/path.h"

#include <nlohmann/json.hpp>

namespace sound_mind::core {

/// @brief Opaque identifier for a `NamedMindWave` within a Project - see
/// `LayerId`'s own docs for the same pattern applied to `Layer`.
using MindWaveId = std::uint64_t;

/**
 * @brief Which family of waveform a `MindWave` generates - see `docs/
 *        sound-mind-design.md`'s "MindWave Functions".
 *
 * `v0.Y.31.1` Installment B fills out the full v1 catalogue named there:
 * periodic, envelope, stepped/noise, spatial, and a first fractal field.
 * `Spatial` deliberately ignores `MindWaveAxis` (see that enum's own docs) -
 * it varies across both canvas axes at once. `v0.Y.39.1` Installment B adds
 * `Drawn` (a hand-drawn `Path`, sampled as a waveform - see `drawnPath()`'s
 * own docs). Installment C adds `StepGrid` (explicit, hand-authored discrete
 * values on a grid of steps - see `stepGridValues()`'s own docs; a distinct
 * concept from `SteppedNoise`'s own quantized-noise-*staircase*, despite the
 * similar name - `StepGrid`'s own values are directly authored, never
 * derived from noise). Installment D adds `Continuous` (`docs/sound-mind-
 * design.md`'s "Continuous Controls" - `continuousShape()`/
 * `continuousSkew()`/`continuousCharacter()`'s own docs) - a genuinely new
 * generator, not just a front door onto the others: its own `evaluate()`
 * case blends a clean periodic curve with fractal noise and layers fine
 * turbulence on top, all via three dedicated continuous dials rather than
 * picking among existing discrete generator types/waveforms. Explicitly a
 * best-effort placeholder (confirmed with the user) - `docs/sound-mind-
 * design.md`'s own Deferred Decision #7 disclaims a real interaction-design
 * pass for MindWaves generally, and this generator's own exact formula is
 * subject to revision once that pass happens.
 */
enum class GeneratorType {
    Periodic,
    Envelope,
    SteppedNoise,
    Spatial,
    Fractal,
    Drawn,
    StepGrid,
    Continuous,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(GeneratorType, {
    {GeneratorType::Periodic, "periodic"},
    {GeneratorType::Envelope, "envelope"},
    {GeneratorType::SteppedNoise, "steppedNoise"},
    {GeneratorType::Spatial, "spatial"},
    {GeneratorType::Fractal, "fractal"},
    {GeneratorType::Drawn, "drawn"},
    {GeneratorType::StepGrid, "stepGrid"},
    {GeneratorType::Continuous, "continuous"},
})
// clang-format on

/**
 * @brief Which specific periodic shape a `GeneratorType::Periodic`
 *        `MindWave` cycles through - see `docs/sound-mind-design.md`'s
 *        "MindWave Functions": "sine, triangle, square, sawtooth, and
 *        pulse".
 *
 * `Square` is exactly `Pulse` at a fixed 50% `dutyCycle()` - kept as its
 * own named value because the design doc names it separately, even though
 * `Pulse` alone could express it.
 */
enum class PeriodicWaveform {
    Sine,
    Triangle,
    Square,
    Sawtooth,
    Pulse,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(PeriodicWaveform, {
    {PeriodicWaveform::Sine, "sine"},
    {PeriodicWaveform::Triangle, "triangle"},
    {PeriodicWaveform::Square, "square"},
    {PeriodicWaveform::Sawtooth, "sawtooth"},
    {PeriodicWaveform::Pulse, "pulse"},
})
// clang-format on

/**
 * @brief Which canvas axis a `MindWave` cycles along - see `docs/
 *        sound-mind-design.md`'s "a spatial 'LFO' that varies over time
 *        (horizontal axis), frequency (vertical axis), or both".
 *
 * Meaningless for `GeneratorType::Spatial`, which varies across both axes
 * at once and has no single axis to name.
 */
enum class MindWaveAxis {
    Time,
    Frequency,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(MindWaveAxis, {
    {MindWaveAxis::Time, "time"},
    {MindWaveAxis::Frequency, "frequency"},
})
// clang-format on

/**
 * @brief Which one-shot shape a `GeneratorType::Envelope` `MindWave`
 *        follows - see `docs/sound-mind-design.md`'s "Envelope shapes":
 *        "exponential decay, a decaying oscillation, and an S-curve
 *        transition, for one-shot fades and thresholds rather than
 *        repeating cycles."
 */
enum class EnvelopeShape {
    ExponentialDecay,
    DecayingOscillation,
    SCurve,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(EnvelopeShape, {
    {EnvelopeShape::ExponentialDecay, "exponentialDecay"},
    {EnvelopeShape::DecayingOscillation, "decayingOscillation"},
    {EnvelopeShape::SCurve, "sCurve"},
})
// clang-format on

/**
 * @brief Which shape a `GeneratorType::SteppedNoise` `MindWave` produces -
 *        see `docs/sound-mind-design.md`'s "Stepped and noise fields": "a
 *        quantised staircase, and smooth (Gaussian-filtered) or fractal
 *        noise."
 */
enum class SteppedNoiseShape {
    Stepped,
    GaussianNoise,
    FractalNoise,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(SteppedNoiseShape, {
    {SteppedNoiseShape::Stepped, "stepped"},
    {SteppedNoiseShape::GaussianNoise, "gaussianNoise"},
    {SteppedNoiseShape::FractalNoise, "fractalNoise"},
})
// clang-format on

/**
 * @brief Which two-axis pattern a `GeneratorType::Spatial` `MindWave`
 *        produces - see `docs/sound-mind-design.md`'s "Spatial patterns":
 *        "ripples, checkerboards, cellular (Voronoi-like) blobs, and
 *        domain-warped noise, evaluated across both axes at once."
 */
enum class SpatialPattern {
    Ripples,
    Checkerboard,
    Cellular,
    DomainWarpedNoise,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(SpatialPattern, {
    {SpatialPattern::Ripples, "ripples"},
    {SpatialPattern::Checkerboard, "checkerboard"},
    {SpatialPattern::Cellular, "cellular"},
    {SpatialPattern::DomainWarpedNoise, "domainWarpedNoise"},
})
// clang-format on

/**
 * @brief How a superposed `MindWave` folds into the running result - see
 *        `docs/sound-mind-design.md`'s "Field Operators": "Superposition
 *        ... layering them together the same way layers themselves
 *        composite, with a chosen blend (multiply, add, min, max, average)
 *        determining how each one folds into the running result."
 *
 * @note `Average` folds pairwise, in stack order (`running = (running +
 *       member) / 2`, one member at a time) - the same "folds into the
 *       running result" mechanism the other four modes use, not a single
 *       statistical mean across every member at once. With more than one
 *       stack member, later members therefore weigh more heavily than
 *       earlier ones; documented here as a known v1 simplification rather
 *       than a claimed true average.
 */
enum class SuperpositionBlendMode {
    Multiply,
    Add,
    Min,
    Max,
    Average,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(SuperpositionBlendMode, {
    {SuperpositionBlendMode::Multiply, "multiply"},
    {SuperpositionBlendMode::Add, "add"},
    {SuperpositionBlendMode::Min, "min"},
    {SuperpositionBlendMode::Max, "max"},
    {SuperpositionBlendMode::Average, "average"},
})
// clang-format on

/**
 * @brief A parametric waveform producing a per-cell scalar field in
 *        `[0, 1]` across the canvas - see `docs/sound-mind-design.md`'s
 *        "Low Frequency Oscillations (MindWaves)".
 *
 * **`v0.Y.31.1`, Installment A**: deliberately a bare value type, not yet
 * a named, `Project`-scoped, independently-referenceable resource the way
 * the design doc's own "MindWave" ultimately is - the same "not built
 * until a real consumer needs it" gap `ToolConfiguration` currently has
 * too. Identity/storage arrives once something actually binds to a
 * `MindWave` by reference (the layer-opacity-binding installment).
 *
 * **`v0.Y.31.1`, Installment B**: fills out the rest of the v1 generator
 * catalogue (`Envelope`, `SteppedNoise`, `Spatial`, `Fractal`, and
 * `Periodic`'s remaining four waveforms) plus superposition
 * (`superpositionStack()`/`superpositionBlendMode()`). Deliberately flat
 * fields for every generator type, not a tagged union/`std::variant` - the
 * same reasoning `FilterConfiguration`'s own docs (and `docs/sound-mind-
 * architecture.md`'s Decision #34) already give: plain typed fields stay
 * simpler and safer than an opaque bag or a discriminated union while the
 * per-type field count stays small, even with five real generator types
 * now coexisting.
 *
 * A hand-rolled, seeded value-noise primitive (not a third-party library -
 * confirmed with the user) backs `GaussianNoise`/`FractalNoise`/`Cellular`/
 * `DomainWarpedNoise`, satisfying `docs/sound-mind-architecture.md`'s own
 * "anything seeded must replay bit-for-bit on the same Studio version and
 * architecture" rule - see `mind_wave.cpp`'s anonymous namespace.
 *
 * `GeneratorType::Fractal` is built from recursive midpoint displacement
 * (confirmed with the user) rather than the branching amplitude/phase
 * grammar `docs/sound-mind-design.md` cross-references from `v0.Y.39.1`
 * (Generators, Phase 5, which comes *after* this milestone) - a simpler,
 * self-contained 1D-field primitive suited to this narrower need, per
 * `docs/sound-mind-roadmap.md`'s own note on why this milestone can't lean
 * on that later one.
 *
 * **`v0.Y.39.1` Installment B adds `GeneratorType::Drawn`** (`drawnPath()`)
 * - a hand-drawn `Path`, sampled as a waveform along whichever axis this
 * MindWave is bound to and looped via `period()`, the same shared field
 * every other generator type already reuses for its own cycle length
 * (confirmed with the user over locking the loop to the path's own
 * recorded duration). The curve's own *other* coordinate at each sample is
 * normalized into `[0, 1]` against the path's own recorded bounding box
 * (`Path::bounds()`, confirmed with the user over the project's global
 * frequency/time range) - "what you drew is what you get," independent of
 * canvas geometry. A drawn curve isn't guaranteed single-valued as a
 * function of the queried axis (a hand-drawn stroke can loop back on
 * itself), so `evaluate()` resolves the ambiguity with a **first-crossing
 * rule**: walking the curve from its own start (`t=0`) toward its end
 * (`t=1`), the first point where it crosses the queried position wins.
 *
 * **`v0.Y.39.1` Installment C adds `GeneratorType::StepGrid`**
 * (`stepGridValues()`) - a bare-bones, hand-authored discrete step
 * sequence rather than a formula-driven shape, looped via `period()` the
 * same way every other generator type already is (one period divided
 * evenly into `stepGridValues().size()` steps).
 */
class MindWave {
public:
    /// @brief Constructs a real, audible-if-bound default - a 1-second
    ///        (or 1-bin, on the frequency axis) sine cycle at zero phase,
    ///        not a degenerate placeholder - matching `FilterConfiguration`'s
    ///        own "nothing happens by accident" default philosophy applied
    ///        to a MindWave instead: a freshly created one should visibly
    ///        do something the moment it's bound, not silently no-op.
    MindWave() noexcept = default;

    /// @brief Which generator family this MindWave uses.
    /// @return The current generator type.
    [[nodiscard]] GeneratorType type() const noexcept { return type_; }

    /// @brief Sets which generator family this MindWave uses.
    /// @param type The new generator type.
    void setType(GeneratorType type) noexcept { type_ = type; }

    /// @brief Which periodic shape this MindWave cycles through - only
    ///        meaningful while `type()` is `GeneratorType::Periodic`.
    /// @return The current periodic waveform.
    [[nodiscard]] PeriodicWaveform periodicWaveform() const noexcept { return periodicWaveform_; }

    /// @brief Sets which periodic shape this MindWave cycles through.
    /// @param waveform The new periodic waveform.
    void setPeriodicWaveform(PeriodicWaveform waveform) noexcept { periodicWaveform_ = waveform; }

    /// @brief Which canvas axis this MindWave cycles along - meaningless
    ///        for `GeneratorType::Spatial` (see `MindWaveAxis`'s own docs).
    /// @return The current axis.
    [[nodiscard]] MindWaveAxis axis() const noexcept { return axis_; }

    /// @brief Sets which canvas axis this MindWave cycles along.
    /// @param axis The new axis.
    void setAxis(MindWaveAxis axis) noexcept { axis_ = axis; }

    /**
     * @brief How long one full cycle takes, in the current axis's own
     *        natural unit: seconds for `MindWaveAxis::Time`, bins (not
     *        Hz) for `MindWaveAxis::Frequency` - see `evaluate()`'s own
     *        docs for why bins, not Hz. Also used as `Envelope`'s own
     *        oscillation period (`DecayingOscillation`), `SteppedNoise`'s
     *        own cycle length (`Stepped`), `Spatial`'s own ring spacing
     *        (`Ripples`) or cell size (`Checkerboard`, in both axes' own
     *        mixed units - a deliberate v1 simplification), and
     *        `Fractal`'s own tile length.
     * @return The current period.
     */
    [[nodiscard]] double period() const noexcept { return period_; }

    /// @brief Sets the cycle length - see `period()`'s own docs for the
    ///        unit. Values at or below zero are treated as a small
    ///        positive floor by `evaluate()` (defensive only - not a
    ///        musically meaningful minimum), rather than dividing by zero.
    /// @param period The new period.
    void setPeriod(double period) noexcept { period_ = period; }

    /// @brief The cycle's own phase offset, in radians - `0` starts the
    ///        cycle at its own defined origin. Also used as `Envelope`'s
    ///        own `DecayingOscillation` phase and `Spatial`'s own
    ///        `Ripples` phase.
    /// @return The current phase offset, in radians.
    [[nodiscard]] double phaseRadians() const noexcept { return phaseRadians_; }

    /// @brief Sets the cycle's own phase offset - see `phaseRadians()`'s
    ///        own docs for the unit.
    /// @param phaseRadians The new phase offset, in radians.
    void setPhaseRadians(double phaseRadians) noexcept { phaseRadians_ = phaseRadians; }

    /**
     * @brief `PeriodicWaveform::Pulse`'s own fraction of each cycle spent
     *        at `1.0` before dropping to `0.0`, in `[0, 1]` - `0.5`
     *        (the default) makes `Pulse` identical to `Square`.
     * @return The current duty cycle; meaningless unless `periodicWaveform()`
     *         is `Pulse`. Not clamped or validated here.
     */
    [[nodiscard]] double dutyCycle() const noexcept { return dutyCycle_; }

    /// @brief Sets `Pulse`'s own duty cycle.
    /// @param dutyCycle The new duty cycle, intended to be within `[0, 1]`.
    void setDutyCycle(double dutyCycle) noexcept { dutyCycle_ = dutyCycle; }

    /// @brief Which one-shot shape this MindWave follows - only
    ///        meaningful while `type()` is `GeneratorType::Envelope`.
    /// @return The current envelope shape.
    [[nodiscard]] EnvelopeShape envelopeShape() const noexcept { return envelopeShape_; }

    /// @brief Sets which one-shot shape this MindWave follows.
    /// @param shape The new envelope shape.
    void setEnvelopeShape(EnvelopeShape shape) noexcept { envelopeShape_ = shape; }

    /**
     * @brief The position (in the current axis's own unit - see
     *        `period()`'s own docs) where `ExponentialDecay` begins
     *        decaying, or where `SCurve` sits at exactly `0.5`. Before this
     *        position, `ExponentialDecay` plateaus at `1.0`.
     * @return The current center; meaningless unless `type()` is
     *         `GeneratorType::Envelope`.
     */
    [[nodiscard]] double envelopeCenter() const noexcept { return envelopeCenter_; }

    /// @brief Sets the envelope's own center - see `envelopeCenter()`'s
    ///        own docs.
    /// @param center The new center.
    void setEnvelopeCenter(double center) noexcept { envelopeCenter_ = center; }

    /**
     * @brief `SCurve`'s own transition steepness - higher values produce a
     *        sharper threshold, lower values a gentler ramp.
     * @return The current steepness; meaningless unless `envelopeShape()`
     *         is `SCurve`. Not clamped or validated here.
     */
    [[nodiscard]] double envelopeSteepness() const noexcept { return envelopeSteepness_; }

    /// @brief Sets `SCurve`'s own transition steepness.
    /// @param steepness The new steepness.
    void setEnvelopeSteepness(double steepness) noexcept { envelopeSteepness_ = steepness; }

    /**
     * @brief How quickly `ExponentialDecay`/`DecayingOscillation` decay
     *        past `envelopeCenter()` - higher values decay faster.
     * @return The current decay rate; meaningless unless `envelopeShape()`
     *         is `ExponentialDecay` or `DecayingOscillation`. Not clamped
     *         or validated here.
     */
    [[nodiscard]] double decayRate() const noexcept { return decayRate_; }

    /// @brief Sets the envelope's own decay rate.
    /// @param decayRate The new decay rate.
    void setDecayRate(double decayRate) noexcept { decayRate_ = decayRate; }

    /// @brief Which shape this MindWave produces - only meaningful while
    ///        `type()` is `GeneratorType::SteppedNoise`.
    /// @return The current stepped/noise shape.
    [[nodiscard]] SteppedNoiseShape steppedNoiseShape() const noexcept { return steppedNoiseShape_; }

    /// @brief Sets which shape this MindWave produces.
    /// @param shape The new stepped/noise shape.
    void setSteppedNoiseShape(SteppedNoiseShape shape) noexcept { steppedNoiseShape_ = shape; }

    /**
     * @brief `Stepped`'s own number of discrete levels per cycle.
     * @return The current step count; meaningless unless
     *         `steppedNoiseShape()` is `Stepped`. Not clamped or validated
     *         here (a non-positive value is defensively floored at `1` by
     *         `evaluate()`, not rejected here).
     */
    [[nodiscard]] int stepCount() const noexcept { return stepCount_; }

    /// @brief Sets `Stepped`'s own number of discrete levels.
    /// @param stepCount The new step count; intended to be a positive integer.
    void setStepCount(int stepCount) noexcept { stepCount_ = stepCount; }

    /**
     * @brief The seed driving every noise-based generator
     *        (`GaussianNoise`, `FractalNoise`, `Cellular`,
     *        `DomainWarpedNoise`) - the same seed always reproduces the
     *        same field, per `docs/sound-mind-architecture.md`'s own
     *        seeded-reproducibility rule.
     * @return The current seed; meaningless for every non-noise-based
     *         generator shape.
     */
    [[nodiscard]] std::uint32_t seed() const noexcept { return seed_; }

    /// @brief Sets the noise seed - see `seed()`'s own docs.
    /// @param seed The new seed.
    void setSeed(std::uint32_t seed) noexcept { seed_ = seed; }

    /**
     * @brief The lattice/cell size noise-based generators sample at, in
     *        the current axis's own unit (or the same mixed unit
     *        `period()` uses for `Spatial`) - smaller values produce
     *        finer, more rapidly-varying noise.
     * @return The current noise scale; meaningless for every non-noise-
     *         based generator shape. Not clamped or validated here.
     */
    [[nodiscard]] double noiseScale() const noexcept { return noiseScale_; }

    /// @brief Sets the noise lattice/cell size.
    /// @param scale The new scale; intended to be positive.
    void setNoiseScale(double scale) noexcept { noiseScale_ = scale; }

    /**
     * @brief How many octaves `FractalNoise`/`DomainWarpedNoise` sum
     *        (fractal Brownian motion) - more octaves add finer detail.
     * @return The current octave count; meaningless unless
     *         `steppedNoiseShape()` is `FractalNoise` or `spatialPattern()`
     *         is `DomainWarpedNoise`. Not clamped or validated here (a
     *         non-positive value is defensively floored at `1` by
     *         `evaluate()`, not rejected here).
     */
    [[nodiscard]] int noiseOctaves() const noexcept { return noiseOctaves_; }

    /// @brief Sets the fractal-noise octave count.
    /// @param octaves The new octave count; intended to be a positive integer.
    void setNoiseOctaves(int octaves) noexcept { noiseOctaves_ = octaves; }

    /**
     * @brief How much each successive fractal-noise octave's amplitude
     *        shrinks by, in `(0, 1)` - lower values weight coarse detail
     *        more heavily, higher values weight fine detail more heavily.
     * @return The current persistence; meaningless unless
     *         `steppedNoiseShape()` is `FractalNoise` or `spatialPattern()`
     *         is `DomainWarpedNoise`. Not clamped or validated here.
     */
    [[nodiscard]] double noisePersistence() const noexcept { return noisePersistence_; }

    /// @brief Sets the fractal-noise persistence.
    /// @param persistence The new persistence.
    void setNoisePersistence(double persistence) noexcept { noisePersistence_ = persistence; }

    /// @brief Which two-axis pattern this MindWave produces - only
    ///        meaningful while `type()` is `GeneratorType::Spatial`.
    /// @return The current spatial pattern.
    [[nodiscard]] SpatialPattern spatialPattern() const noexcept { return spatialPattern_; }

    /// @brief Sets which two-axis pattern this MindWave produces.
    /// @param pattern The new spatial pattern.
    void setSpatialPattern(SpatialPattern pattern) noexcept { spatialPattern_ = pattern; }

    /**
     * @brief `Ripples`'s own ring center, along the time axis (seconds).
     * @return The current center X; meaningless unless `spatialPattern()`
     *         is `Ripples`.
     */
    [[nodiscard]] double spatialCenterX() const noexcept { return spatialCenterX_; }

    /// @brief Sets `Ripples`'s own ring center's time-axis component.
    /// @param centerX The new center X, in seconds.
    void setSpatialCenterX(double centerX) noexcept { spatialCenterX_ = centerX; }

    /**
     * @brief `Ripples`'s own ring center, along the frequency axis (bins,
     *        not Hz - see `evaluate()`'s own docs).
     * @return The current center Y; meaningless unless `spatialPattern()`
     *         is `Ripples`.
     */
    [[nodiscard]] double spatialCenterY() const noexcept { return spatialCenterY_; }

    /// @brief Sets `Ripples`'s own ring center's frequency-axis component.
    /// @param centerY The new center Y, in bins.
    void setSpatialCenterY(double centerY) noexcept { spatialCenterY_ = centerY; }

    /**
     * @brief `DomainWarpedNoise`'s own warp magnitude - how far the
     *        sampled coordinates are displaced before the underlying
     *        fractal-noise field is sampled.
     * @return The current warp strength; meaningless unless
     *         `spatialPattern()` is `DomainWarpedNoise`. Not clamped or
     *         validated here.
     */
    [[nodiscard]] double domainWarpStrength() const noexcept { return domainWarpStrength_; }

    /// @brief Sets `DomainWarpedNoise`'s own warp magnitude.
    /// @param strength The new warp strength.
    void setDomainWarpStrength(double strength) noexcept { domainWarpStrength_ = strength; }

    /**
     * @brief How much `Fractal`'s own recursive midpoint displacement
     *        shrinks at each successive subdivision level, in `(0, 1)` -
     *        lower values produce a smoother curve, higher values a more
     *        jagged one.
     * @return The current roughness; meaningless unless `type()` is
     *         `GeneratorType::Fractal`. Not clamped or validated here.
     */
    [[nodiscard]] double fractalRoughness() const noexcept { return fractalRoughness_; }

    /// @brief Sets `Fractal`'s own roughness.
    /// @param roughness The new roughness.
    void setFractalRoughness(double roughness) noexcept { fractalRoughness_ = roughness; }

    /**
     * @brief How many recursive midpoint-displacement subdivision levels
     *        `Fractal` computes - more levels add finer detail.
     * @return The current iteration count; meaningless unless `type()` is
     *         `GeneratorType::Fractal`. Not clamped or validated here (a
     *         negative value is defensively floored at `0` by
     *         `evaluate()`, not rejected here).
     */
    [[nodiscard]] int fractalIterations() const noexcept { return fractalIterations_; }

    /// @brief Sets `Fractal`'s own iteration count.
    /// @param iterations The new iteration count; intended to be a
    ///        non-negative integer.
    void setFractalIterations(int iterations) noexcept { fractalIterations_ = iterations; }

    /**
     * @brief The other MindWaves this one's own field is combined with,
     *        via `superpositionBlendMode()`, after this MindWave's own
     *        generator is evaluated - see `docs/sound-mind-design.md`'s
     *        "Field Operators" ("Superposition"). Empty by default - a
     *        fresh MindWave superposes nothing, matching this class's own
     *        "nothing happens by accident beyond the base generator"
     *        default philosophy.
     * @return The current superposition stack, in fold order.
     */
    [[nodiscard]] const std::vector<MindWave>& superpositionStack() const noexcept { return superpositionStack_; }

    /// @brief Mutable access to the superposition stack, for in-place edits.
    /// @return The current superposition stack.
    [[nodiscard]] std::vector<MindWave>& superpositionStack() noexcept { return superpositionStack_; }

    /// @brief Sets the superposition stack wholesale.
    /// @param stack The new stack, in fold order.
    void setSuperpositionStack(std::vector<MindWave> stack) { superpositionStack_ = std::move(stack); }

    /// @brief How each `superpositionStack()` member folds into the
    ///        running result - see `SuperpositionBlendMode`'s own docs.
    ///        Meaningless while `superpositionStack()` is empty.
    /// @return The current blend mode.
    [[nodiscard]] SuperpositionBlendMode superpositionBlendMode() const noexcept { return superpositionBlendMode_; }

    /// @brief Sets how each superposed MindWave folds into the running result.
    /// @param mode The new blend mode.
    void setSuperpositionBlendMode(SuperpositionBlendMode mode) noexcept { superpositionBlendMode_ = mode; }

    /**
     * @brief Whether this MindWave's own sampling coordinate is displaced
     *        by another MindWave before its own generator runs - see
     *        `docs/sound-mind-design.md`'s "Field Operators" ("Warp - one
     *        field distorts the coordinates another field is sampled at").
     *        `v0.Y.39.1` Installment A.
     *
     * A single optional source, not a stack (unlike `superpositionStack()`) -
     * the design doc names Warp as a two-field relationship, not an
     * N-way fold. Represented internally as a 0-or-1-length
     * `std::vector<MindWave>` (the same "container of itself" trick
     * `superpositionStack_` already establishes and this codebase's own
     * build already proves compiles - `std::optional<MindWave>` as a
     * direct member has no equivalent incomplete-type guarantee from the
     * standard) rather than exposing that representation directly.
     *
     * @return `true` if `warpSource()` is meaningful.
     */
    [[nodiscard]] bool hasWarpSource() const noexcept { return !warpSourceStack_.empty(); }

    /// @brief The MindWave that distorts this one's own sampling
    ///        coordinate - see `hasWarpSource()`'s own docs. Only
    ///        meaningful when `hasWarpSource()` is `true`.
    /// @return The current warp source.
    [[nodiscard]] const MindWave& warpSource() const noexcept { return warpSourceStack_.front(); }

    /// @brief Sets (and enables) the warp source.
    /// @param source The new warp source.
    void setWarpSource(MindWave source) { warpSourceStack_ = {std::move(source)}; }

    /// @brief Disables warping - `hasWarpSource()` becomes `false`.
    void clearWarpSource() noexcept { warpSourceStack_.clear(); }

    /**
     * @brief How far `warpSource()`'s own field displaces this MindWave's
     *        own sampling coordinate, in the current `axis()`'s own unit
     *        (seconds for `Time`, bins for `Frequency`) - see
     *        `evaluate()`'s own docs for the exact formula. Meaningless
     *        while `hasWarpSource()` is `false`.
     * @return The current warp strength.
     */
    [[nodiscard]] double warpStrength() const noexcept { return warpStrength_; }

    /// @brief Sets the warp displacement's own strength.
    /// @param strength The new strength.
    void setWarpStrength(double strength) noexcept { warpStrength_ = strength; }

    /**
     * @brief The hand-drawn curve a `GeneratorType::Drawn` MindWave samples
     *        as a waveform - see `docs/sound-mind-design.md`'s "MindWave
     *        Functions" ("Drawn shapes ... sampled as a waveform rather
     *        than stamped as paint, and looped across whichever axis it's
     *        bound to"). `v0.Y.39.1` Installment B.
     *
     * Captured via `PickController::selectedPath()` in the Studio (draw an
     * ordinary paint stroke, Pick it, then apply it here - no dedicated
     * curve-drawing mode of its own, mirroring `docs/sound-mind-design.md`'s
     * "Selection" ("Warp")'s own identical workflow). Only `evaluate()`'s
     * own geometry - anchors and Bézier handles - is read; this path's own
     * `gradient()` is never consulted, since a MindWave has no notion of
     * color/opacity along its length.
     *
     * @return The current drawn curve; empty (no nodes, `MindWave`'s own
     *         default) until explicitly captured. Meaningless unless
     *         `type()` is `GeneratorType::Drawn`.
     */
    [[nodiscard]] const Path& drawnPath() const noexcept { return drawnPath_; }

    /// @brief Sets the drawn curve - see `drawnPath()`'s own docs.
    /// @param path The new curve.
    void setDrawnPath(Path path) { drawnPath_ = std::move(path); }

    /**
     * @brief The explicit, hand-authored discrete values a
     *        `GeneratorType::StepGrid` `MindWave` cycles through - see
     *        `docs/sound-mind-design.md`'s "MindWave Functions" ("Step
     *        grids ... discrete values on a grid of steps ... for
     *        rhythmic, mechanical modulation that a smooth curve or
     *        formula is clumsy at"). `v0.Y.39.1` Installment C.
     *
     * A distinct concept from `SteppedNoiseShape::Stepped`'s own
     * `stepCount()`-driven quantized *staircase* (a formula, deriving its
     * levels from `stepCount()` alone) - a `StepGrid`'s own values are
     * directly authored, one per step, with no formula behind them at all.
     * The step *count* is simply this vector's own size - no separate
     * count field, avoiding a "two things that must stay in sync"
     * representation the way `harmonicStrengths()`
     * (`InstrumentConfiguration`) already avoids one for the same reason.
     *
     * @return The current per-step values, in order; each intended to lie
     *         in `[0, 1]`, though not clamped or validated here. Defaults
     *         to a plain four-step rising staircase (`{0.25, 0.5, 0.75,
     *         1.0}`) - visibly doing something the moment it's bound or
     *         previewed, the same "nothing happens by accident" default
     *         philosophy `MindWave`'s own constructor docs already
     *         establish, rather than an empty (and therefore silently
     *         neutral) list.
     */
    [[nodiscard]] const std::vector<double>& stepGridValues() const noexcept { return stepGridValues_; }

    /// @brief Sets the step grid's own values wholesale - see
    ///        `stepGridValues()`'s own docs.
    /// @param values The new per-step values, in order.
    void setStepGridValues(std::vector<double> values) { stepGridValues_ = std::move(values); }

    /**
     * @brief How far `GeneratorType::Continuous` sweeps from a clean sine
     *        cycle toward fractal noise - see `docs/sound-mind-design.md`'s
     *        "Continuous Controls" ("shape, sweeping from a clean curve to
     *        something noisier"). `v0.Y.39.1` Installment D.
     *
     * A **best-effort placeholder** (confirmed with the user): rather than
     * picking among `MindWave`'s own discrete generator types/waveforms
     * (which have no interpolation between them), `evaluate()` computes
     * both a clean sine value and a fractal-noise value at the same point
     * and linearly blends between them by this fraction - a genuinely new
     * evaluation path, not a front door that merely selects existing
     * settings. `docs/sound-mind-design.md`'s own Deferred Decision #7
     * disclaims a real interaction-design pass for MindWaves generally;
     * this generator's own exact formula is subject to revision once that
     * pass happens.
     *
     * Reuses `period()`/`seed()`/`noiseScale()`/`noiseOctaves()`/
     * `noisePersistence()` for the sine cycle's own length and the noise
     * component's own shape - the same shared-field philosophy every other
     * generator type already follows - rather than declaring duplicate
     * fields with identical meaning.
     *
     * @return The current shape fraction; meaningless unless `type()` is
     *         `GeneratorType::Continuous`. Not clamped or validated here,
     *         but intended to lie in `[0, 1]` (`0` pure sine, `1` pure
     *         fractal noise).
     */
    [[nodiscard]] double continuousShape() const noexcept { return continuousShape_; }

    /// @brief Sets the Continuous generator's own shape fraction - see
    ///        `continuousShape()`'s own docs.
    /// @param shape The new shape fraction.
    void setContinuousShape(double shape) noexcept { continuousShape_ = shape; }

    /**
     * @brief How far `GeneratorType::Continuous`'s own sine component
     *        biases earlier or later in its cycle - see `docs/sound-mind-
     *        design.md`'s "Continuous Controls" ("skew, biasing the shape
     *        earlier or later"). `v0.Y.39.1` Installment D.
     *
     * Unlike `continuousShape()`, this maps onto a genuinely continuous
     * quantity with no discreteness to work around: `0.5` (the default)
     * applies no bias; `evaluate()` remaps the full `[0, 1]` range to a
     * `[-pi, pi]` phase offset, added on top of `phaseRadians()`'s own
     * shared phase field (a second, dedicated bias specific to this
     * generator, not a duplicate of that shared one).
     *
     * @return The current skew; meaningless unless `type()` is
     *         `GeneratorType::Continuous`. Not clamped or validated here,
     *         but intended to lie in `[0, 1]`.
     */
    [[nodiscard]] double continuousSkew() const noexcept { return continuousSkew_; }

    /// @brief Sets the Continuous generator's own skew - see
    ///        `continuousSkew()`'s own docs.
    /// @param skew The new skew.
    void setContinuousSkew(double skew) noexcept { continuousSkew_ = skew; }

    /**
     * @brief How much fine turbulence `GeneratorType::Continuous` layers
     *        on top of its own shape/skew result - see `docs/sound-mind-
     *        design.md`'s "Continuous Controls" ("character, adding
     *        harmonics or turbulence"). `v0.Y.39.1` Installment D.
     *
     * Independent of `continuousShape()`'s own clean-vs-noisy blend: a
     * separate, finer-grained noise sample (at a tenth of `noiseScale()`'s
     * own lattice size) is added on top, scaled by this fraction - so
     * turbulence can be dialled in even at `continuousShape() == 0` (a
     * "clean" sine cycle with rough, gritty character added), not only
     * where the shape blend has already introduced noise.
     *
     * @return The current character fraction; meaningless unless `type()`
     *         is `GeneratorType::Continuous`. Not clamped or validated
     *         here, but intended to lie in `[0, 1]` (`0` no added
     *         turbulence).
     */
    [[nodiscard]] double continuousCharacter() const noexcept { return continuousCharacter_; }

    /// @brief Sets the Continuous generator's own character fraction - see
    ///        `continuousCharacter()`'s own docs.
    /// @param character The new character fraction.
    void setContinuousCharacter(double character) noexcept { continuousCharacter_ = character; }

    /**
     * @brief This MindWave's own field value at `point`, after superposing
     *        `superpositionStack()` (if any) on top of its own generator.
     *
     * The frequency axis is log-scaled (see `frequencyToBinIndex()`'s own
     * docs), so a `period()` expressed in raw Hz would pack a different
     * number of visible cycles into the same on-screen space depending on
     * where in the frequency range it sits - the same reasoning
     * `translateFrequencyByBins()`/`PickController`'s own frequency-axis
     * drag math already established for why bins, not Hz, are this axis's
     * own natural, evenly-spaced unit. `period()` on `MindWaveAxis::Frequency`
     * is therefore in bins, converted from `point`'s own Hz via
     * `frequencyToBinIndex()` before dividing by it. `GeneratorType::Spatial`
     * uses both of `point`'s own components directly (time seconds and
     * frequency bins) regardless of `axis()`.
     *
     * As of `v0.Y.39.1` Installment A, `warpSource()` (if `hasWarpSource()`)
     * is evaluated first, at this same `point` - its own `[0, 1]` result is
     * remapped to `[-1, 1]` and scaled by `warpStrength()`, then added to
     * `point`'s own time-seconds *and* bin-index components alike (the same
     * single delta applied to both, a deliberate simplification over
     * `DomainWarpedNoise`'s own two independently-seeded deltas - there is
     * only one warp source here, not two) before anything else in this
     * method reads `point`. This is the general form
     * `SpatialPattern::DomainWarpedNoise` already establishes for one
     * fixed, built-in noise field, now available between any two MindWaves
     * (`docs/sound-mind-design.md`'s own framing). Warping and superposing
     * are independent: `superpositionStack()` members are still evaluated
     * at the original, unwarped `point` - warp only distorts what *this*
     * MindWave's own generator samples from, not what its superposition
     * partners do.
     *
     * The result is always clamped to `[0, 1]` before returning, regardless
     * of which generator shape or superposition blend produced it -
     * defensive only, since e.g. `SuperpositionBlendMode::Add` can
     * otherwise exceed `1.0`.
     *
     * `GeneratorType::Drawn` (`v0.Y.39.1` Installment B) reads `point` the
     * same way every other generator does (`axisPosition`, already
     * warped/looped by `period()`/`phaseRadians_` above) but samples
     * `drawnPath()` instead of a formula - see `drawnPath()`'s own docs for
     * the normalization/first-crossing mechanism. A `drawnPath()` with
     * fewer than two nodes (nothing captured yet), or whose own recorded
     * span along the queried axis is degenerate (zero width), evaluates to
     * a neutral `0.5` rather than erroring.
     *
     * `GeneratorType::StepGrid` (`v0.Y.39.1` Installment C) divides one
     * `period()`-length cycle into `stepGridValues().size()` equal-width
     * steps and returns whichever step `axisPosition` currently falls in,
     * verbatim - see `stepGridValues()`'s own docs.
     *
     * `GeneratorType::Continuous` (`v0.Y.39.1` Installment D) blends a clean
     * sine cycle with fractal noise by `continuousShape()`'s own fraction,
     * biases phase by `continuousSkew()`, and adds fine turbulence scaled
     * by `continuousCharacter()` - see `continuousShape()`'s own docs for
     * the full formula.
     *
     * @param point The canvas position to evaluate.
     * @param config Interprets `point`'s own Hz against `config`'s own
     *        frequency range/bin count, needed whenever a bin index is
     *        computed (`MindWaveAxis::Frequency`, or any `Spatial` pattern).
     * @return This MindWave's own field value at `point`, in `[0, 1]`.
     */
    [[nodiscard]] float evaluate(TimeFrequencyPoint point, const sound_mind::codec::StreamCodecConfig& config) const;

private:
    GeneratorType type_ = GeneratorType::Periodic;
    PeriodicWaveform periodicWaveform_ = PeriodicWaveform::Sine;
    MindWaveAxis axis_ = MindWaveAxis::Time;
    double period_ = 1.0;
    double phaseRadians_ = 0.0;
    double dutyCycle_ = 0.5;
    EnvelopeShape envelopeShape_ = EnvelopeShape::ExponentialDecay;
    double envelopeCenter_ = 0.0;
    double envelopeSteepness_ = 1.0;
    double decayRate_ = 1.0;
    SteppedNoiseShape steppedNoiseShape_ = SteppedNoiseShape::Stepped;
    int stepCount_ = 4;
    std::uint32_t seed_ = 1;
    double noiseScale_ = 1.0;
    int noiseOctaves_ = 4;
    double noisePersistence_ = 0.5;
    SpatialPattern spatialPattern_ = SpatialPattern::Ripples;
    double spatialCenterX_ = 0.0;
    double spatialCenterY_ = 0.0;
    double domainWarpStrength_ = 1.0;
    double fractalRoughness_ = 0.5;
    int fractalIterations_ = 8;
    std::vector<MindWave> superpositionStack_;
    SuperpositionBlendMode superpositionBlendMode_ = SuperpositionBlendMode::Multiply;
    std::vector<MindWave> warpSourceStack_;
    double warpStrength_ = 1.0;
    Path drawnPath_;
    std::vector<double> stepGridValues_ = {0.25, 0.5, 0.75, 1.0};
    double continuousShape_ = 0.0;
    double continuousSkew_ = 0.5;
    double continuousCharacter_ = 0.0;
};

/**
 * @brief Evaluates `wave` across an entire canvas, one value per
 *        `[bin][frame]` cell - the Studio's own live MindWave Preview
 *        overlay (`docs/sound-mind-design.md`'s "Low Frequency
 *        Oscillations") is the first caller, but this is a plain,
 *        reusable evaluation utility, not tied to that one use.
 *
 * Row-major, bin-major layout (see `cellIndex()`'s own docs) - the same
 * layout every other per-cell scalar field in this codebase (a
 * `StreamImage`'s own `leftMagnitudeDb`, a Filter's own per-cell
 * parameter fields) already uses.
 *
 * @param wave The MindWave to evaluate.
 * @param config Interprets each cell's own bin/frame position as a real
 *        time/frequency point, the same config every other domain
 *        conversion in Core already uses.
 * @param canvasWidth How many frame columns to evaluate - `config`'s own
 *        `binCount` supplies the row count.
 * @return `wave`'s own value at every cell, each in `[0, 1]` - size
 *         `config.binCount * canvasWidth`.
 */
[[nodiscard]] std::vector<float> evaluateMindWaveField(const MindWave& wave,
                                                        const sound_mind::codec::StreamCodecConfig& config,
                                                        std::uint32_t canvasWidth);

/**
 * @brief How `reduceMindWaveToSignal()` collapses every bin at one time
 *        sample into that sample's own single value - see its own docs.
 */
enum class ReduceMode {
    /// @brief Reads `wave`'s own value at one representative bin (the
    ///        middle of `StreamCodecConfig::binCount`) - a single
    ///        cross-section, not an aggregate.
    Slice,
    /// @brief Averages `wave`'s own value across every bin.
    Integrate,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(ReduceMode, {
    {ReduceMode::Slice, "slice"},
    {ReduceMode::Integrate, "integrate"},
})
// clang-format on

/**
 * @brief Collapses `wave`'s own 2D field into an ordinary 1D signal by
 *        slicing or integrating across the frequency axis at each of
 *        `sampleCount` evenly-spaced moments across `[0, timeSpanSeconds)` -
 *        `docs/sound-mind-design.md`'s "Field Operators" ("Reduce ...
 *        collapses a 2D field into an ordinary 1D signal, by slicing or
 *        integrating along one axis (typically frequency)"). `v0.Y.39.1`
 *        Installment A.
 *
 * **Only ever collapses frequency, never time** - a deliberate scope
 * choice, not an oversight: frequency has a well-defined, bounded range to
 * integrate across (`[0, binCount)`), matching the design doc's own
 * "typically frequency" framing; time has no equivalent bound (there is no
 * canonical "how much time" to integrate a time-collapsing Reduce across),
 * so it has no well-defined meaning here and isn't attempted.
 *
 * The returned signal has no inherent real-world timescale of its own -
 * `timeSpanSeconds` only spaces the `sampleCount` evaluation points, it
 * doesn't tie the result to any canvas position. A caller that wants the
 * signal to represent one full cycle of `wave`'s own shape, indexed by a
 * `[0, 1]` progress fraction (e.g. `InstrumentConfiguration`'s own vibrato/
 * tremolo binding, sampled by a stroke's own `pathT` - see
 * `applyInstrumentPaintOperation()`'s own docs), should pass `wave.period()`
 * as `timeSpanSeconds`.
 *
 * @param wave The MindWave to reduce.
 * @param config Interprets each bin as a real frequency, the same config
 *        every other per-cell domain conversion in Core already uses.
 * @param sampleCount How many signal samples to produce; floored at `1`.
 * @param timeSpanSeconds The real time span the `sampleCount` samples are
 *        evenly spaced across, starting at `0`.
 * @param mode Whether each sample slices one bin or integrates across all
 *        of them - see `ReduceMode`'s own docs.
 * @return A signal of exactly `std::max(1u, sampleCount)` values, each in
 *         `[0, 1]`.
 */
[[nodiscard]] std::vector<float> reduceMindWaveToSignal(const MindWave& wave,
                                                         const sound_mind::codec::StreamCodecConfig& config,
                                                         std::uint32_t sampleCount, double timeSpanSeconds,
                                                         ReduceMode mode);

/// @brief Serializes a MindWave to its JSON representation.
void to_json(nlohmann::json& json, const MindWave& mindWave);

/// @brief Parses a MindWave from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, MindWave& mindWave);

/**
 * @brief A `MindWave` given real, `Project`-scoped identity - `v0.Y.31.1`
 *        Installment C1's own answer to "identity/storage arrives once
 *        something actually binds to a MindWave by reference" (Installment
 *        A's own docs) - confirmed with the user as a separate wrapper
 *        rather than adding id/name fields to `MindWave` itself.
 *
 * `MindWave` itself stays exactly the bare value type it's always been -
 * still the right shape for a `superpositionStack()` member, which never
 * needs (and should never carry) its own identity. Only a MindWave actually
 * registered in a `Project`'s own `Project::mindWaves()` collection - the
 * kind something else can bind to by id - gets one of these.
 *
 * Deliberately a plain public-field struct, not a class with getters/
 * setters, matching `TimeFrequencyPoint`'s/`FrameBinRange`'s own precedent
 * for a small data holder with no invariants to protect (unlike `Layer`,
 * whose id is fixed after construction - a `NamedMindWave`'s `id` and
 * `name` are both freely reassignable, matching `Project::addMindWave()`'s
 * own "caller-visible mutable fields" needs for the bare-bones management
 * panel a later installment adds).
 */
struct NamedMindWave {
    /// @brief This entry's identity within its Project - assigned by
    ///        `Project::addMindWave()`, not meant to be picked by hand.
    MindWaveId id = 0;
    /// @brief Display name. `Project` is responsible for keeping names
    ///        unique within itself, the same division `Layer::name()`'s
    ///        own docs already draw for layer names.
    std::string name;
    /// @brief The actual generator/parameters this entry names.
    MindWave wave;
};

/// @brief Serializes a NamedMindWave to its JSON representation.
void to_json(nlohmann::json& json, const NamedMindWave& namedMindWave);

/// @brief Parses a NamedMindWave from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, NamedMindWave& namedMindWave);

}  // namespace sound_mind::core
