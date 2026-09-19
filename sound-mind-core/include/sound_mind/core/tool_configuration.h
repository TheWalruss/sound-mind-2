#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/blend_mode.h"
#include "sound_mind/core/gradient.h"
#include "sound_mind/core/mind_grain.h"
#include "sound_mind/core/mind_shot.h"

namespace sound_mind::core {

/// @brief Opaque identifier for a `NamedMindWave` within a Project - a
/// deliberate, exact duplicate of `mind_wave.h`'s own `MindWaveId` alias
/// (matching `docs/sound-mind-architecture.md`'s own Decision #59
/// "duplicated, not shared" precedent, and `filter_configuration.h`'s/
/// `layer.h`'s own identical duplication for the same reason - Decision
/// #80): including `mind_wave.h` here would cycle back through `path.h` ->
/// `operation.h` -> `layer.h` -> this header. A type alias can be
/// redeclared identically in multiple headers with no ODR concern.
using MindWaveId = std::uint64_t;

/**
 * @brief Which kind of painting tool a `ToolConfiguration` configures -
 *        see `docs/sound-mind-design.md`'s "Tool Configuration".
 *
 * @note Only `Procedural`, (as of `v0.Y.32.1`, Sound Mind Instruments)
 *       `Instrument`, (as of `v0.Y.33.1`) `MindShot`/`MindGrain`, and (as of
 *       `v0.Y.34.1`) `Heal`/`Soften` (Installment A)/`Smudge`/`OrderChaos`
 *       (Installment B) exist as real, paintable tools so far - `Clone`
 *       remains this milestone's own final, not-yet-started installment.
 *       Adding a value here ahead of its own tool actually working is
 *       deliberate groundwork for the Tool Configuration Panel/Wizard's
 *       dynamic-per-type UI, not a claim that tool is usable.
 */
enum class ToolType {
    Procedural,
    Instrument,
    MindShot,
    MindGrain,
    Smudge,
    OrderChaos,
    Heal,
    Soften,
    Clone,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(ToolType, {
    {ToolType::Procedural, "procedural"},
    {ToolType::Instrument, "instrument"},
    {ToolType::MindShot, "mindShot"},
    {ToolType::MindGrain, "mindGrain"},
    {ToolType::Smudge, "smudge"},
    {ToolType::OrderChaos, "orderChaos"},
    {ToolType::Heal, "heal"},
    {ToolType::Soften, "soften"},
    {ToolType::Clone, "clone"},
})
// clang-format on

/**
 * @brief A brush tip's geometric footprint - see `docs/sound-mind-design.md`'s
 *        "Procedural Brushes". `ProceduralConfiguration`'s own parameter -
 *        an `Instrument`'s own "shape" comes from its harmonic content, not
 *        a geometric footprint (see `InstrumentConfiguration`'s own docs).
 */
enum class BrushTipShape {
    Circle,
    Square,
    Diamond,
    Triangle,
    SingleStroke,
    Cross,
    Star,
    Corner,
    Arc,
    DotSpatter,
    Dapple,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(BrushTipShape, {
    {BrushTipShape::Circle, "circle"},
    {BrushTipShape::Square, "square"},
    {BrushTipShape::Diamond, "diamond"},
    {BrushTipShape::Triangle, "triangle"},
    {BrushTipShape::SingleStroke, "singleStroke"},
    {BrushTipShape::Cross, "cross"},
    {BrushTipShape::Star, "star"},
    {BrushTipShape::Corner, "corner"},
    {BrushTipShape::Arc, "arc"},
    {BrushTipShape::DotSpatter, "dotSpatter"},
    {BrushTipShape::Dapple, "dapple"},
})
// clang-format on

/**
 * @brief How a stroke's own brush stamps are spaced along its Path - see
 *        `docs/sound-mind-design.md`'s "Stamp Intervals".
 *
 * Independent of *how* the Path itself was built (freehand capture or
 * the Path tool's own deliberate node placement - see `path.h`'s own
 * docs on why both end up the same underlying representation): this is
 * a property of the brush stamping *along* that Path, the same as tip
 * shape or size, not of how the geometry was drawn. Shared by every
 * `ToolConfiguration` subtype - which cells a stamp *lands on* is a
 * path-sampling concern independent of what a stamp actually paints once
 * it lands.
 */
enum class StampMode {
    /// @brief Stamped exactly as densely as the stroke's own raw input
    ///        was sampled - the only mode that existed before Stamp
    ///        Intervals, and still the default. **Not** an even,
    ///        fixed-distance spacing, and independent of brush size,
    ///        unlike every other mode: a slowly-drawn stroke naturally
    ///        samples (and therefore stamps) more densely over a given
    ///        physical distance than a quickly-drawn one, since spacing
    ///        follows raw input sampling, not a chosen interval. Named
    ///        `Stroke` (renamed from `Continuous`, which wrongly implied
    ///        a uniform density) rather than after the *effect* the other
    ///        modes are named for (their own chosen spacing), since this
    ///        one has no chosen spacing of its own to name.
    Stroke,
    /// @brief Evenly spaced stamps measured along the Path's own arc
    ///        length, in the same seconds-equivalent normalized space
    ///        `ToolConfiguration::size()` already uses - a "dotted brush"
    ///        effect, spacing following the curve however it bends.
    AlongCurve,
    /// @brief One stamp everywhere the Path crosses a time-axis line
    ///        `stampInterval()` seconds apart - a rhythmic, grid-like
    ///        placement independent of the Path's own actual shape.
    TimeAxis,
    /// @brief One stamp everywhere the Path crosses a frequency-axis
    ///        line `stampInterval()` Hz apart.
    FrequencyAxis,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(StampMode, {
    {StampMode::Stroke, "stroke"},
    {StampMode::AlongCurve, "alongCurve"},
    {StampMode::TimeAxis, "timeAxis"},
    {StampMode::FrequencyAxis, "frequencyAxis"},
    // Legacy alias, pre-rename (v0.0.32.3) - reads a project file saved
    // before this rename (`"stampMode": "continuous"`) back as `Stroke`;
    // never written by to_json() (Stroke's own "stroke" entry above is
    // listed first, so to_json()'s pair lookup always finds and writes
    // that one instead - see NLOHMANN_JSON_SERIALIZE_ENUM's own
    // first-match semantics for both directions).
    {StampMode::Stroke, "continuous"},
})
// clang-format on

/**
 * @brief Abstract base for a named, savable/shareable painting-tool setup -
 *        see `docs/sound-mind-design.md`'s "Tool Configuration".
 *
 * Every `PaintOperation` carries its own `ToolConfiguration` (a snapshot
 * of whatever was configured at the moment it was painted, not a
 * reference into a shared list) - a separate, project-owned collection of
 * named presets (the Tool Configuration Panel's own "Tool Preset"
 * drop-down draws from) is a later installment of this same milestone.
 *
 * **Polymorphic since `v0.Y.32.1` (Sound Mind Instruments)** - the
 * previous, single flat class's own doc already anticipated this moment:
 * with only `Procedural`'s own parameters real, "plain typed fields are
 * simpler and safer than an opaque bag would be, at the cost of needing
 * real rework (a tagged union, or per-type subclassing) once a second
 * tool type's parameters actually need to coexist with these" - `Instrument`
 * is that second type (see `docs/sound-mind-architecture.md`'s Decision on
 * this milestone for why subclassing was chosen over a tagged union).
 * Owned everywhere via `std::unique_ptr<ToolConfiguration>`, never held or
 * passed by value (an abstract class can't be) - clone() is the "virtual
 * copy constructor" every owner uses to get its own independent copy
 * (`PaintOperation::translatedCopy()`, re-applying a Picked stroke's
 * config, ...), the same role a real copy constructor would play for a
 * concrete value type.
 */
class ToolConfiguration {
public:
    virtual ~ToolConfiguration() = default;

    /// @brief Which kind of tool this configures - fixed per concrete
    ///        subtype, never reassigned.
    /// @return This configuration's own tool type.
    [[nodiscard]] virtual ToolType type() const noexcept = 0;

    /// @brief An independent copy of this configuration, of the same
    ///        concrete subtype - the "virtual copy constructor" every
    ///        owner uses in place of a real (impossible, on an abstract
    ///        type) copy constructor.
    /// @return A new, owned, deep copy.
    [[nodiscard]] virtual std::unique_ptr<ToolConfiguration> clone() const = 0;

    /// @brief This configuration's own saved name.
    /// @return The name it was last saved under, or an empty string for
    ///         one that's never been saved as a named preset.
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    /// @brief Names (or renames) this configuration.
    /// @param name The new name; uniqueness among a project's saved
    ///        presets is the project's responsibility, not enforced here -
    ///        the same division `Layer::setName()`'s own docs draw.
    void setName(std::string name) { name_ = std::move(name); }

    /**
     * @brief How sharply a stamp's own effect fades out toward the edge
     *        of its own footprint - see each concrete subtype's own docs
     *        for exactly what "footprint" means for it (a 2D geometric
     *        blob for `ProceduralConfiguration`; a time-axis-only fade for
     *        `InstrumentConfiguration`, whose own footprint in frequency
     *        is the harmonic series itself, not a blended blob).
     * @return A value in `[0, 1]` - `0` is a hard edge, `1` the softest
     *         falloff; not clamped or validated here.
     */
    [[nodiscard]] float falloff() const noexcept { return falloff_; }

    /// @brief Sets the stamp's own edge softness - see falloff()'s own docs.
    /// @param falloff Intended to be in `[0, 1]`; not clamped or validated here.
    void setFalloff(float falloff) noexcept { falloff_ = falloff; }

    /**
     * @brief The stamp's own size.
     *
     * In the same seconds-equivalent normalized space `fitPathToPoints()`'s
     * own `frequencyToTimeScale` parameter establishes (see `path.h`) -
     * resolution/project-agnostic, converted to real canvas pixels only at
     * the point of actually stamping or displaying it. See each concrete
     * subtype's own docs for exactly what this radius bounds.
     *
     * @return The stamp's radius, in seconds-equivalent units; not clamped
     *         or validated here.
     */
    [[nodiscard]] double size() const noexcept { return size_; }

    /// @brief Sets the stamp's own size.
    /// @param size The new radius, in seconds-equivalent units (see
    ///        size()'s own docs); intended to be positive, not clamped or
    ///        validated here.
    void setSize(double size) noexcept { size_ = size; }

    /**
     * @brief How this tool's own stamps are spaced along whatever Path
     *        they're applied to.
     *
     * Virtual (as of `v0.Y.34.1` Installment C) - `FixedStampPlacementConfiguration`
     * overrides this to always report `AlongCurve`, unconditionally, for the
     * four tool types that need predictable, evenly-spaced stamps to look
     * and sound right (see its own docs). `setStampMode()` still exists and
     * still writes the same literal `stampMode_` on those types too -
     * `storedStampMode()` (below) is how a caller (specifically
     * `ToolConfigurationPanel::changeToolType()`'s own "carry every shared
     * field over from the outgoing configuration" step) reads that literal
     * value back, bypassing this override, so a value the user actually
     * chose isn't silently replaced by whatever a *forced* type happened to
     * report while it was briefly the active tool.
     *
     * @return The currently configured stamp mode; `Stroke` by
     *         default (the only mode that existed before Stamp
     *         Intervals).
     */
    [[nodiscard]] virtual StampMode stampMode() const noexcept { return stampMode_; }

    /// @brief Sets how this tool's own stamps are spaced along a Path.
    /// @param mode The new stamp mode.
    void setStampMode(StampMode mode) noexcept { stampMode_ = mode; }

    /// @brief The literal, stored stamp mode - `stampMode()`'s own value on
    ///        any tool type that doesn't override it, but (unlike
    ///        `stampMode()`) still the *last value `setStampMode()` was
    ///        actually called with* even on a `FixedStampPlacementConfiguration`
    ///        subtype, whose own `stampMode()` override otherwise hides it.
    ///        See `stampMode()`'s own docs for why this exists, and when to
    ///        reach for it instead of the ordinary getter.
    /// @return The literal stored value, ignoring any override.
    [[nodiscard]] StampMode storedStampMode() const noexcept { return stampMode_; }

    /**
     * @brief The spacing `stampMode()` places stamps at - meaningless
     *        while `stampMode()` is `Stroke` (which always follows the
     *        raw input's own sampling density instead).
     *
     * The unit depends on `stampMode()`: seconds-equivalent arc length
     * for `AlongCurve` (the same normalized space `size()` uses), plain
     * seconds for `TimeAxis`, Hz for `FrequencyAxis`.
     *
     * Virtual, for the same reason `stampMode()` is - see its own docs.
     * `FixedStampPlacementConfiguration` overrides this to always report
     * `66%` of `size()`, live (recomputed from whatever `size()` currently
     * is, not a stored snapshot taken once).
     *
     * @return The current interval; not clamped or validated here, but
     *         a non-positive value stamps nothing (see
     *         `sampleStroke()`'s own docs in `paint_application.cpp`).
     */
    [[nodiscard]] virtual double stampInterval() const noexcept { return stampInterval_; }

    /// @brief Sets `stampMode()`'s own spacing.
    /// @param interval The new interval, in whatever unit stampInterval()'s
    ///        own docs specify for the current stampMode(); intended to be
    ///        positive.
    void setStampInterval(double interval) noexcept { stampInterval_ = interval; }

    /// @brief The literal, stored stamp interval - see `storedStampMode()`'s
    ///        own docs for why this exists alongside the ordinary,
    ///        possibly-overridden `stampInterval()` getter.
    /// @return The literal stored value, ignoring any override.
    [[nodiscard]] double storedStampInterval() const noexcept { return stampInterval_; }

    /// @brief This tool's own default gradient - seeds a new Path's own
    ///        gradient (see `path.h`) whenever painting starts with this
    ///        configuration; the artist can then further customize that
    ///        copy per-stroke without affecting this configuration's own.
    /// @return The default gradient painting with this tool starts from.
    [[nodiscard]] const Gradient& defaultGradient() const noexcept { return defaultGradient_; }

    /// @brief Mutable access to this tool's own default gradient, for
    ///        in-place edits.
    /// @return The default gradient painting with this tool starts from.
    [[nodiscard]] Gradient& defaultGradient() noexcept { return defaultGradient_; }

protected:
    ToolConfiguration() = default;

    /// @brief Protected, not public - a concrete subtype's own clone()
    ///        uses this (via its own implicitly-generated copy
    ///        constructor) to copy these shared fields; no other code can
    ///        copy a `ToolConfiguration`, and an abstract base can never
    ///        be sliced through a by-value parameter/return in the first
    ///        place, so nothing further needs to be explicitly deleted
    ///        here.
    ToolConfiguration(const ToolConfiguration&) = default;

private:
    std::string name_;
    float falloff_ = 0.5f;
    double size_ = 0.2;
    StampMode stampMode_ = StampMode::Stroke;
    double stampInterval_ = 0.1;
    Gradient defaultGradient_;
};

/**
 * @brief A geometric-tip paintbrush - the original, `v0.Y.24.1` (Basic
 *        Painting) tool: stamps a shape from `docs/sound-mind-design.md`'s
 *        "Procedural Brushes" library, blended toward the stroke's own
 *        gradient target within `falloff()`'s own soft-edged 2D footprint
 *        (time and frequency both).
 */
class ProceduralConfiguration : public ToolConfiguration {
public:
    /// @brief Constructs a configuration with a plain, medium circular
    ///        tip and a fresh, fully transparent default gradient.
    ProceduralConfiguration() = default;

    [[nodiscard]] ToolType type() const noexcept override { return ToolType::Procedural; }

    [[nodiscard]] std::unique_ptr<ToolConfiguration> clone() const override {
        return std::make_unique<ProceduralConfiguration>(*this);
    }

    /// @brief The brush tip's geometric footprint.
    /// @return The currently configured tip shape.
    [[nodiscard]] BrushTipShape tipShape() const noexcept { return tipShape_; }

    /// @brief Sets the brush tip's geometric footprint.
    /// @param shape The new tip shape.
    void setTipShape(BrushTipShape shape) noexcept { tipShape_ = shape; }

private:
    BrushTipShape tipShape_ = BrushTipShape::Circle;
};

/**
 * @brief A Sound Mind Instrument - `v0.Y.32.1`'s own small parametric sound
 *        model, per `docs/sound-mind-design.md`'s "Sound Mind Instruments":
 *        synthesizes a stamp around the stroke's own pitch from a harmonic
 *        series above the fundamental, stretched sharp of a pure integer
 *        series by `inharmonicity()`.
 *
 * **This installment's own scope**: just the harmonic series and
 * inharmonicity - the noise component, body resonance, and ADSR envelope
 * the design doc also describes are deliberately not here yet (each its
 * own follow-up installment, confirmed with the user alongside this one -
 * see `docs/sound-mind-architecture.md`'s own Decision on this class).
 *
 * **`v0.Y.39.1` Installment A adds `vibratoMindWave()`/`tremoloMindWave()`**:
 * Reduce's own new per-note modulation hook for Instruments (see
 * `reduceMindWaveToSignal()`'s own docs and `applyInstrumentPaintOperation()`'s
 * own docs for the mechanism) - each bound wave is first collapsed to a 1D
 * signal via `reduceMindWaveToSignal(..., wave.period(), ReduceMode::
 * Integrate)`, then sampled once per stamp at that stamp's own `pathT`
 * (0..1 progress along the *whole stroke*, not a per-note clock - this
 * sidesteps the separate, still-unbuilt "operation-relative MindWave
 * binding" architecture entirely, at the cost of a genuinely fresh
 * per-note retrigger feel, which that future architecture would still add).
 * Ordinary `std::optional<MindWaveId>` live-reference bindings, the same
 * pattern every other "bind X to a MindWave" parameter in this codebase
 * uses (see `FilterConfiguration::blurSigmaMindWave()`'s own docs for the
 * general mechanism) - not a snapshot.
 *
 * **No `tipShape()`** - an Instrument's own "shape" in frequency *is* the
 * harmonic series (each harmonic a single bin-exact partial, not a
 * blended-footprint blob); `falloff()`/`size()` (inherited from the base)
 * still apply, but only along the *time* axis, fading a stamp in/out
 * across its own radius the same way a Procedural stamp's edge softens,
 * not across frequency (see `applyPaintOperation()`'s own docs).
 */
class InstrumentConfiguration : public ToolConfiguration {
public:
    /// @brief Constructs a configuration with a plausible default
    ///        harmonic series (a fundamental plus three overtones,
    ///        each half the strength of the one before) and no
    ///        inharmonicity (a pure integer series).
    InstrumentConfiguration() = default;

    [[nodiscard]] ToolType type() const noexcept override { return ToolType::Instrument; }

    [[nodiscard]] std::unique_ptr<ToolConfiguration> clone() const override {
        return std::make_unique<InstrumentConfiguration>(*this);
    }

    /**
     * @brief Each harmonic's own strength above the fundamental.
     *
     * Index `0` is the fundamental itself (harmonic 1); index `n` is
     * harmonic `n + 1`. A harmonic beyond the end of this list simply
     * isn't synthesized - there's no implicit "strength 0" tail.
     * Strengths aren't clamped or normalized here; a value above `1.0`
     * (an overtone louder than the fundamental) is accepted as-is.
     *
     * @return The current per-harmonic strengths, fundamental first.
     */
    [[nodiscard]] const std::vector<double>& harmonicStrengths() const noexcept { return harmonicStrengths_; }

    /// @brief Sets the harmonic series wholesale - see harmonicStrengths()'s
    ///        own docs.
    /// @param strengths The new per-harmonic strengths, fundamental first.
    void setHarmonicStrengths(std::vector<double> strengths) { harmonicStrengths_ = std::move(strengths); }

    /**
     * @brief How far the harmonic series stretches sharp of a pure
     *        integer series, the way a real vibrating body's own overtones
     *        do - `0` is a perfectly harmonic series (harmonic `n` at
     *        exactly `n` times the fundamental); higher values stretch
     *        higher harmonics progressively sharper.
     *
     * Applied per harmonic `n` (1-indexed) as `n * fundamentalHz *
     * sqrt(1 + inharmonicity * n^2)` - the same stretched-partial formula
     * real string/bar physics follows (piano strings in particular),
     * chosen for being simple, well-understood, and audibly plausible
     * rather than derived from this project's own first-principles model
     * of any specific instrument.
     *
     * @return The current inharmonicity coefficient; not clamped or
     *         validated here, but intended to be small and non-negative
     *         (e.g. `0` to `0.05`) for a physically plausible stretch.
     */
    [[nodiscard]] double inharmonicity() const noexcept { return inharmonicity_; }

    /// @brief Sets the inharmonicity coefficient - see inharmonicity()'s
    ///        own docs.
    /// @param inharmonicity The new coefficient.
    void setInharmonicity(double inharmonicity) noexcept { inharmonicity_ = inharmonicity; }

    /**
     * @brief The MindWave (if any) driving this Instrument's vibrato
     *        (per-note pitch modulation) - see this class's own docs for
     *        the pathT-sampled Reduce mechanism.
     * @return The bound MindWave's id, or `std::nullopt` for no vibrato
     *         (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> vibratoMindWave() const noexcept { return vibratoMindWave_; }

    /// @brief Sets (or clears) which MindWave drives vibrato - see
    ///        vibratoMindWave()'s own docs.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setVibratoMindWave(std::optional<MindWaveId> mindWaveId) noexcept { vibratoMindWave_ = mindWaveId; }

    /**
     * @brief How far `vibratoMindWave()` (when bound) can bend each
     *        harmonic's own frequency, in semitones - applied as
     *        `harmonicHz *= pow(2, vibratoDepthSemitones/12 * (modulator*2-1))`,
     *        so the modulator's own `[0, 1]` range maps to a symmetric
     *        `+/-vibratoDepthSemitones` bend. Meaningless while
     *        `vibratoMindWave()` is unbound.
     * @return The current depth, in semitones; not clamped or validated
     *         here.
     */
    [[nodiscard]] double vibratoDepthSemitones() const noexcept { return vibratoDepthSemitones_; }

    /// @brief Sets the vibrato depth - see vibratoDepthSemitones()'s own
    ///        docs.
    /// @param semitones The new depth, in semitones.
    void setVibratoDepthSemitones(double semitones) noexcept { vibratoDepthSemitones_ = semitones; }

    /**
     * @brief The MindWave (if any) driving this Instrument's tremolo
     *        (per-note amplitude modulation) - see this class's own docs
     *        for the pathT-sampled Reduce mechanism.
     * @return The bound MindWave's id, or `std::nullopt` for no tremolo
     *         (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> tremoloMindWave() const noexcept { return tremoloMindWave_; }

    /// @brief Sets (or clears) which MindWave drives tremolo - see
    ///        tremoloMindWave()'s own docs.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setTremoloMindWave(std::optional<MindWaveId> mindWaveId) noexcept { tremoloMindWave_ = mindWaveId; }

    /**
     * @brief How far `tremoloMindWave()` (when bound) can dip each
     *        harmonic's own strength, as a `[0, 1]` fraction - applied as
     *        `strength *= 1 - tremoloDepth*(1-modulator)`, so the
     *        modulator's own `1` leaves strength unchanged and `0` dips it
     *        by the full `tremoloDepth` fraction. Meaningless while
     *        `tremoloMindWave()` is unbound.
     * @return The current depth; not clamped or validated here, but
     *         intended to stay within `[0, 1]`.
     */
    [[nodiscard]] double tremoloDepth() const noexcept { return tremoloDepth_; }

    /// @brief Sets the tremolo depth - see tremoloDepth()'s own docs.
    /// @param depth The new depth, intended to be within `[0, 1]`.
    void setTremoloDepth(double depth) noexcept { tremoloDepth_ = depth; }

private:
    std::vector<double> harmonicStrengths_ = {1.0, 0.5, 0.25, 0.125};
    double inharmonicity_ = 0.0;
    std::optional<MindWaveId> vibratoMindWave_;
    double vibratoDepthSemitones_ = 0.5;
    std::optional<MindWaveId> tremoloMindWave_;
    double tremoloDepth_ = 0.3;
};

/**
 * @brief A Mind Shot brush - `v0.Y.33.1` Installment A, per `docs/sound-
 *        mind-design.md`'s "Mind Shots": stamps a previously captured
 *        selection back exactly as it was when captured.
 *
 * **Snapshots the captured `Clip` directly, rather than only holding a
 * `MindShotId` reference into `Project::mindShots()`** - the same
 * "a config is a snapshot, not a live reference into a shared, mutable
 * list" reasoning `ToolConfiguration`'s own class docs already establish
 * for every tool type ("so re-editing this operation later can't be
 * affected by unrelated later changes to a saved preset of the same
 * name"). Here it also solves a real correctness question for free: an
 * already-painted stroke keeps rendering identically even if its source
 * Mind Shot is later renamed or removed from the project's library -
 * exactly what "paints back exactly as it was when captured, independent
 * of later changes to its source" already promises.
 *
 * **No `tipShape()`/meaningful `falloff()`/`size()` use** - a Mind Shot
 * stamps its own captured content at its own native size, centered on each
 * stamp position (see `applyPaintOperation()`'s own docs), combined with
 * whatever's already there via `blendMode()` - the same
 * `applyBlendedCell()` dispatch `PasteOperation` already uses, not
 * Procedural/Instrument's gradient/falloff blend. `BlendMode::Overwrite`
 * (the default) reproduces this class's own pre-`v0.Y.37.1` hard-overwrite
 * behavior exactly.
 */
class MindShotConfiguration : public ToolConfiguration {
public:
    /// @brief Constructs a configuration with no Mind Shot selected yet
    ///        (an empty `clip()`) - paints nothing until `setClip()` is
    ///        called with a real capture, the same "nothing happens by
    ///        accident" default convention `InstrumentConfiguration`'s own
    ///        zero-inharmonicity default follows.
    MindShotConfiguration() = default;

    [[nodiscard]] ToolType type() const noexcept override { return ToolType::MindShot; }

    [[nodiscard]] std::unique_ptr<ToolConfiguration> clone() const override {
        return std::make_unique<MindShotConfiguration>(*this);
    }

    /// @brief Which library entry `clip()` was last set from, if any - for
    ///        UI purposes only (so a Tool Configuration Panel showing this
    ///        configuration can highlight the right entry in its own Mind
    ///        Shot picker); never consulted by painting itself, which only
    ///        ever reads `clip()` directly.
    /// @return The source entry's own id, or `std::nullopt` if `clip()`
    ///         was never set from a library entry (a fresh configuration,
    ///         or one loaded from a project file saved before this field
    ///         existed).
    [[nodiscard]] std::optional<MindShotId> sourceMindShotId() const noexcept { return sourceMindShotId_; }

    /**
     * @brief Sets which captured content this configuration paints -
     *        snapshotting `clip` directly (see this class's own docs on
     *        why).
     * @param sourceId The library entry `clip` was captured from, for
     *        `sourceMindShotId()`'s own UI-only purpose; `std::nullopt` if
     *        unknown/not applicable.
     * @param clip The captured content to paint, copied in.
     */
    void setClip(std::optional<MindShotId> sourceId, Clip clip) {
        sourceMindShotId_ = sourceId;
        clip_ = std::move(clip);
    }

    /// @brief The captured content this configuration paints.
    /// @return The current clip - `frameCount()`/`binCount()` are both
    ///         `0` (nothing to paint) until `setClip()` is called.
    [[nodiscard]] const Clip& clip() const noexcept { return clip_; }

    /**
     * @brief How this configuration's own stamp combines with whatever's
     *        already there - `v0.Y.37.1` (Deferred Blend Modes).
     *
     * Dispatched through `sound_mind::core::applyBlendedCell()` by
     * `blitClipCentered()`.
     *
     * @return The current blend mode; `BlendMode::Overwrite` (this class's
     *         own pre-`v0.Y.37.1` hard-overwrite behavior) by default - a
     *         configuration saved before this milestone existed always
     *         gets this value, reproducing its own prior, only-ever-
     *         overwrite behavior exactly.
     */
    [[nodiscard]] BlendMode blendMode() const noexcept { return blendMode_; }

    /// @brief Sets this configuration's own blend mode.
    /// @param mode The new mode - see blendMode()'s own docs.
    void setBlendMode(BlendMode mode) noexcept { blendMode_ = mode; }

private:
    std::optional<MindShotId> sourceMindShotId_;
    Clip clip_;
    BlendMode blendMode_ = BlendMode::Overwrite;
};

/**
 * @brief A Mind Grain brush - `v0.Y.33.1` Installment B, per `docs/sound-
 *        mind-design.md`'s "Mind Grains": stamps a *live* reference to a
 *        region on another layer, redrawn fresh from that layer's own
 *        current content every time it's actually rendered.
 *
 * **The deliberate opposite of `MindShotConfiguration`**: holds no pixel
 * content at all, only `sourceLayerId()`/`bounds()` - the reference
 * itself. `applyPaintOperation()`'s own `MindGrainConfiguration` branch
 * resolves the actual pixels fresh from `sourceLayerId()`'s own current
 * content at paint-application time (via a caller-supplied
 * `LayerContentResolver`), the same way `NamedMindGrain`'s own docs
 * describe. **How "live" this actually is, in this installment**: a
 * grain-painted layer only re-samples its source the next time *that*
 * layer's own content is rebuilt for any reason (a new stroke there,
 * undo/redo, project load) - not the instant the source layer changes
 * elsewhere. Confirmed with the user as this installment's own scope,
 * over a full, immediately-reactive cross-layer rebuild cascade.
 *
 * **Only paintable on a layer above `sourceLayerId()`** - see
 * `isLayerAbove()`'s own docs for where this is actually enforced (stroke
 * start, and defensively against layer reorder/removal); this class
 * itself never checks it - a `MindGrainConfiguration` can be constructed
 * with any `sourceLayerId()` at all, same as `MindShotConfiguration` can
 * be constructed with an empty `Clip`.
 *
 * **No `tipShape()`/meaningful `falloff()`/`size()` use, same as
 * `MindShotConfiguration`** - whatever region `sourceLayerId()`'s own
 * current content has at `bounds()`, centered on each stamp position, not
 * scaled, but combined with whatever's already there via `blendMode()`,
 * same as `MindShotConfiguration`'s own `v0.Y.37.1` update.
 */
class MindGrainConfiguration : public ToolConfiguration {
public:
    /// @brief Constructs a configuration with no Mind Grain selected yet
    ///        (`sourceLayerId()` is `0`, an id no real layer below the
    ///        `Background` layer's own id could ever have - see
    ///        `LayerId`'s own docs) - paints nothing until `setReference()`
    ///        is called with a real capture.
    MindGrainConfiguration() = default;

    [[nodiscard]] ToolType type() const noexcept override { return ToolType::MindGrain; }

    [[nodiscard]] std::unique_ptr<ToolConfiguration> clone() const override {
        return std::make_unique<MindGrainConfiguration>(*this);
    }

    /// @brief Which library entry this configuration references, if any -
    ///        for UI purposes only (so a Tool Configuration Panel showing
    ///        this configuration can highlight the right entry in its own
    ///        Mind Grain picker); never consulted by painting itself,
    ///        which only ever reads `sourceLayerId()`/`bounds()` directly.
    /// @return The source entry's own id, or `std::nullopt` if this
    ///         configuration was never set from a library entry (a fresh
    ///         configuration, or one loaded from a project file saved
    ///         before this field existed).
    [[nodiscard]] std::optional<MindGrainId> sourceMindGrainId() const noexcept { return sourceMindGrainId_; }

    /**
     * @brief Sets which region this configuration reads its live content
     *        from.
     * @param sourceId The library entry this reference was picked from,
     *        for `sourceMindGrainId()`'s own UI-only purpose;
     *        `std::nullopt` if unknown/not applicable.
     * @param sourceLayerId The layer to read live content from.
     * @param bounds The region within `sourceLayerId` to read.
     */
    void setReference(std::optional<MindGrainId> sourceId, LayerId sourceLayerId, TimeFrequencyRect bounds) {
        sourceMindGrainId_ = sourceId;
        sourceLayerId_ = sourceLayerId;
        bounds_ = bounds;
    }

    /// @brief Which layer this configuration reads its live content from.
    /// @return The current source layer id - `0` (see the default
    ///         constructor's own docs) until `setReference()` is called.
    [[nodiscard]] LayerId sourceLayerId() const noexcept { return sourceLayerId_; }

    /// @brief The region within `sourceLayerId()` this configuration reads
    ///        its live content from.
    /// @return The current bounds - default-constructed (a degenerate,
    ///         zero-area rect) until `setReference()` is called.
    [[nodiscard]] const TimeFrequencyRect& bounds() const noexcept { return bounds_; }

    /// @copydoc MindShotConfiguration::blendMode()
    [[nodiscard]] BlendMode blendMode() const noexcept { return blendMode_; }

    /// @copydoc MindShotConfiguration::setBlendMode()
    void setBlendMode(BlendMode mode) noexcept { blendMode_ = mode; }

private:
    std::optional<MindGrainId> sourceMindGrainId_;
    LayerId sourceLayerId_ = 0;
    TimeFrequencyRect bounds_;
    BlendMode blendMode_ = BlendMode::Overwrite;
};

/**
 * @brief An intermediate base for tool types whose own stamp placement is
 *        always `AlongCurve`, at `66%` of `size()`, unconditionally -
 *        `HealConfiguration`/`SoftenConfiguration`/`SmudgeConfiguration`/
 *        `OrderChaosConfiguration` (`v0.Y.34.1` Installment C), confirmed
 *        with the user: these four tools' own results only look/sound good
 *        with stamps evenly spaced along the path at a size-proportional
 *        interval - `Stroke` mode's own raw-input-density spacing (or any
 *        of the other, differently-spaced modes) can leave visible gaps or
 *        uneven overlap for a blur/rearrange effect in a way it never does
 *        for a color-blended brush stamp. Rather than trust every caller to
 *        configure this by hand, it's enforced structurally: neither
 *        `stampMode()` nor `stampInterval()` can be set to anything else on
 *        any of the four, from any entry point (the Tool Configuration
 *        Panel, a loaded project file, or direct construction) - see
 *        `ToolConfiguration::stampMode()`'s own docs for why the setters
 *        still exist and still write their own now-ignored stored values
 *        harmlessly. The Tool Configuration Panel hides its own Stamp
 *        Mode/Stamp Interval controls entirely for these four tool types,
 *        rather than showing disabled controls with no effect.
 *
 * `size()` itself is unaffected - still each tool's own real, user-set
 * footprint radius; only the derived placement interval is forced.
 */
class FixedStampPlacementConfiguration : public ToolConfiguration {
public:
    [[nodiscard]] StampMode stampMode() const noexcept final { return StampMode::AlongCurve; }

    /// @brief `66%` of `size()`, recomputed live from whatever `size()`
    ///        currently is - not a stored snapshot taken once, so changing
    ///        Brush Size keeps the stamp interval proportional to it.
    /// @return `size() * 0.66`.
    [[nodiscard]] double stampInterval() const noexcept final { return size() * 0.66; }

protected:
    /// @brief Default-constructs with the base class's own defaults; only
    ///        reachable through a concrete subtype's own constructor.
    FixedStampPlacementConfiguration() = default;

    /// @brief Copy-constructs from another instance; only reachable through
    ///        a concrete subtype's own `clone()`.
    FixedStampPlacementConfiguration(const FixedStampPlacementConfiguration&) = default;
};

/**
 * @brief Temporal blur, per `docs/sound-mind-design.md`'s "Heal": within
 *        the stroke's own ordinary 2D falloff-weighted footprint (the same
 *        one `ProceduralConfiguration` uses), each pixel blends toward a
 *        plain box average of its own neighbors *along the time axis
 *        only, at the same frequency bin* - the intended "erase a stray
 *        mark without disturbing the surrounding texture" use, since a
 *        genuine defect is usually a brief moment in time, not a whole
 *        frequency band.
 *
 * **Adds no fields of its own** - confirmed with the user: `size()` doubles
 * as both the stamp's own footprint radius (as for every tool type) *and*
 * the temporal blur window's own half-width (how many neighboring frames
 * get averaged); `falloff()` still softens the footprint's own edge,
 * exactly as it always does; and the stroke's own gradient stop *opacity*
 * (evaluated along the path, same as every other tool type) sets how
 * strongly each pixel blends toward its own local average - the stop's
 * *intensity* (the Color swatch) goes unused, the same "some shared
 * controls are visible but inert for this tool type" precedent
 * `MindShotConfiguration`/`MindGrainConfiguration` already established
 * (there's no gradient "target loudness" for a blur to paint toward -
 * only how much of the locally-averaged value to keep). Stamp placement is
 * fixed too - see `FixedStampPlacementConfiguration`'s own docs.
 *
 * See `applyPaintOperation()`'s own `HealConfiguration` dispatch branch for
 * the actual blur math - it never touches `sharedPhaseRadians`, the same
 * "blur only ever touches amplitude, never phase" precedent
 * `filter_application.cpp`'s own `UniformBlur`/`DirectionalBlur`/
 * `EdgePreservingBlur` filters already established.
 */
class HealConfiguration : public FixedStampPlacementConfiguration {
public:
    HealConfiguration() = default;

    [[nodiscard]] ToolType type() const noexcept override { return ToolType::Heal; }

    [[nodiscard]] std::unique_ptr<ToolConfiguration> clone() const override {
        return std::make_unique<HealConfiguration>(*this);
    }
};

/**
 * @brief Radial blur, per `docs/sound-mind-design.md`'s "Soften": the same
 *        idea as `HealConfiguration` above, but isotropic - each pixel
 *        blends toward a plain box average of its own neighbors across
 *        *both* the time and frequency axes, not time alone, for a
 *        uniform, undirected softening rather than Heal's own
 *        defect-erasing, time-axis-only blend.
 *
 * **Adds no fields of its own**, for exactly the same reasons
 * `HealConfiguration`'s own docs give - `size()` doubles as both the
 * footprint radius and the (now 2D) blur window's own half-extent in each
 * direction, `falloff()` softens the footprint edge, and the stroke's own
 * gradient stop opacity sets blend strength (intensity unused). Stamp
 * placement is fixed too - see `FixedStampPlacementConfiguration`'s own
 * docs. See `applyPaintOperation()`'s own `SoftenConfiguration` dispatch
 * branch.
 */
class SoftenConfiguration : public FixedStampPlacementConfiguration {
public:
    SoftenConfiguration() = default;

    [[nodiscard]] ToolType type() const noexcept override { return ToolType::Soften; }

    [[nodiscard]] std::unique_ptr<ToolConfiguration> clone() const override {
        return std::make_unique<SoftenConfiguration>(*this);
    }
};

/**
 * @brief Directional smear, per `docs/sound-mind-design.md`'s "Smudge":
 *        "pushes pixels along the stroke direction, stretching and
 *        blending sound in time and frequency." Confirmed with the user as
 *        the simpler of two candidate designs (over a classic, stateful
 *        "brush load" carried across the whole stroke): each pixel within
 *        the stroke's own ordinary 2D falloff-weighted footprint blends
 *        toward a plain average sampled *along a line through that pixel,
 *        oriented along the stroke's own local direction, spanning the
 *        distance to the neighboring stamp* - computed independently per
 *        stamp, the same execution shape every other tool type already
 *        uses (no new state carried between stamps). Forced to `AlongCurve`
 *        placement (see `FixedStampPlacementConfiguration`'s own docs), so
 *        every stamp's own neighbor is a real, evenly-spaced one rather than
 *        `Stroke` mode's own raw-input-density samples - overlapping stamps
 *        still compound into a continuous smeared trail as the stroke
 *        progresses, the same "overlapping stamps compound naturally"
 *        precedent every gradient-blended tool type already follows.
 *
 * **Adds no fields of its own**, the same reasoning `HealConfiguration`'s
 * own docs give: `size()` doubles as the footprint radius, `falloff()`
 * softens the footprint edge, and the stroke's own gradient stop opacity
 * sets blend strength (intensity unused). The smear's own *direction* and
 * *length* come from the stroke's own geometry (consecutive stamp
 * positions), not a configurable parameter - see
 * `applyPaintOperation()`'s own `SmudgeConfiguration` dispatch branch for
 * the exact line-sampling math. A single-point stroke (a tap, with no
 * neighboring stamp to smear toward) is a no-op - there's no direction to
 * smear along.
 */
class SmudgeConfiguration : public FixedStampPlacementConfiguration {
public:
    SmudgeConfiguration() = default;

    [[nodiscard]] ToolType type() const noexcept override { return ToolType::Smudge; }

    [[nodiscard]] std::unique_ptr<ToolConfiguration> clone() const override {
        return std::make_unique<SmudgeConfiguration>(*this);
    }
};

/**
 * @brief Pushes a region toward spectral order or spectral chaos, per
 *        `docs/sound-mind-design.md`'s "Order/Chaos" - a single tool type
 *        (matching `ToolType::OrderChaos`'s own single enum value) spanning
 *        both directions of one continuum via `amount()`, rather than two
 *        separate tools: negative values push toward chaos, positive
 *        toward order, `0` (the default) has no effect. Confirmed with the
 *        user through a dedicated design pass grounded in edge-of-chaos
 *        criticality (the legacy Python Studio's own inspiration) rather
 *        than a formal entropy metric (Lyapunov exponent, recurrence
 *        quantification, permutation entropy) - a concrete, directly
 *        implementable mechanic instead:
 *
 * - **Chaos** (`amount() < 0`): within the stroke's own footprint, swaps
 *   the intensity of a random subset of pixel pairs - at `amount() == -1`,
 *   every eligible pixel participates in one random permutation among
 *   itself, scrambling the footprint's own arrangement while - at full
 *   opacity - leaving its total energy, average, and histogram *exactly*
 *   unchanged (a pure permutation moves values around without creating or
 *   destroying any of them; below full opacity, each swap is only
 *   partially blended in, the same as everywhere else opacity applies, so
 *   the preservation is no longer exact). Smaller magnitudes swap a
 *   proportionally smaller random subset, leaving the rest of the footprint
 *   untouched.
 * - **Order** (`amount() > 0`): within the same footprint, builds a
 *   horizontal (per-frame) and a vertical (per-bin) energy profile, finds
 *   each one's own peak (the loudest column/row), and re-sorts a random
 *   subset of pixels so the brightest end up closest to those two peak
 *   lines and the darkest end up farthest - concentrating energy into an
 *   emergent horizontal/vertical cross rather than leaving it scattered.
 *   Audibly: noise pulled toward the horizontal line becomes a tone; a
 *   transient pulled toward the vertical line becomes sharper; a
 *   diagonal/chaotic tone pulled toward the horizontal line steadies into
 *   one.
 *
 * **This is the first tool type in this milestone to need a field of its
 * own** - `amount()` - unlike `HealConfiguration`/`SoftenConfiguration`/
 * `SmudgeConfiguration`, which all reuse `size()`/`falloff()`/opacity
 * entirely. The stroke's own gradient stop opacity is *still* reused, for
 * a second, orthogonal purpose: `amount()` decides *what fraction of the
 * footprint's own pixels participate* in the swap/reorder at all, while
 * opacity decides *how strongly each participating pixel's own new value
 * actually replaces the original* (a swap or reorder blended only
 * partially back toward the original, the same "opacity is always the
 * final blend-strength dial" precedent every other tool type already
 * follows) - intensity (the Color swatch) still goes unused, same as
 * Heal/Soften/Smudge. Never touches `sharedPhaseRadians` - only pixel
 * *intensity* is swapped/reordered, matching every other blur/rearrange
 * tool type's own "amplitude only" precedent. Stamp placement is fixed too,
 * the same as Heal/Soften/Smudge - see
 * `FixedStampPlacementConfiguration`'s own docs.
 */
class OrderChaosConfiguration : public FixedStampPlacementConfiguration {
public:
    /// @brief Constructs a configuration with `amount()` at `0` - no
    ///        effect until set.
    OrderChaosConfiguration() = default;

    [[nodiscard]] ToolType type() const noexcept override { return ToolType::OrderChaos; }

    [[nodiscard]] std::unique_ptr<ToolConfiguration> clone() const override {
        return std::make_unique<OrderChaosConfiguration>(*this);
    }

    /// @brief How far, and in which direction, this configuration pushes a
    ///        painted region along the order/chaos continuum.
    /// @return A value in `[-1, 1]`: negative for chaos, positive for
    ///         order, `0` (the default) for no effect. Not clamped by this
    ///         class itself - see `setAmount()`'s own docs.
    [[nodiscard]] double amount() const noexcept { return amount_; }

    /// @brief Sets `amount()`.
    /// @param amount The new value - conventionally `[-1, 1]`, but not
    ///        clamped here (the same "the Panel's own spin box enforces the
    ///        meaningful range, this class doesn't second-guess it"
    ///        precedent `ToolConfiguration::setFalloff()`'s own docs
    ///        establish).
    void setAmount(double amount) noexcept { amount_ = amount; }

private:
    double amount_ = 0.0;
};

/// @brief Serializes any concrete `ToolConfiguration` to its JSON
///        representation - dispatches on `type()` internally (a `"type"`
///        discriminator field, plus every field common to every subtype,
///        plus whichever subtype-specific fields `type()` calls for) - see
///        `toolConfigurationFromJson()` for the inverse.
/// @param json Overwritten with the serialized representation.
/// @param config The configuration to serialize.
void to_json(nlohmann::json& json, const ToolConfiguration& config);

/**
 * @brief Parses a `ToolConfiguration` from its JSON representation,
 *        constructing whichever concrete subtype its own `"type"` field
 *        names.
 *
 * The polymorphic counterpart to a plain ADL `from_json()` - which can't
 * construct a fresh object of a type only known at runtime (from the JSON
 * itself) the way this factory function can; nothing else in this
 * codebase can hand back a `ToolConfiguration` by value, since the class
 * is abstract.
 *
 * @param json The JSON representation to parse.
 * @return The parsed, owned configuration.
 * @throws nlohmann::json::exception on malformed or missing required data.
 * @throws std::invalid_argument for a `"type"` this factory doesn't yet
 *         know how to construct (any value past `Procedural`/`Instrument`/
 *         `MindShot`/`MindGrain`/`Heal`/`Soften`/`Smudge`/`OrderChaos` - see
 *         `ToolType`'s own docs on which are real so far).
 */
[[nodiscard]] std::unique_ptr<ToolConfiguration> toolConfigurationFromJson(const nlohmann::json& json);

}  // namespace sound_mind::core
