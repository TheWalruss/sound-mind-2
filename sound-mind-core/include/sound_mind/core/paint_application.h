#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/gradient.h"
#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/operation.h"
#include "sound_mind/core/paint_operation.h"

namespace sound_mind::core {

/**
 * @brief Resolves a `MindWaveId` against a `Project`'s own MindWave
 *        library - `applyPaintOperation()`'s own `InstrumentConfiguration`
 *        vibrato/tremolo binding entry point (`v0.Y.39.1` Installment A).
 *
 * Deliberately a per-id resolver, not a single pre-resolved pair of
 * pointers (unlike `FilterParameterMindWaves`, which `compositeProject()`
 * resolves once against a Filter layer's own single, current
 * `FilterConfiguration`) - `rebuildPaintedContent()` replays a whole
 * *history* of `PaintOperation`s on one layer, each carrying its own
 * `InstrumentConfiguration` snapshot from whenever it was painted, so two
 * different strokes on the same layer can legitimately be bound to two
 * different MindWaves (or none). `applyPaintOperation()` re-resolves each
 * operation's own `vibratoMindWave()`/`tremoloMindWave()` id, every replay,
 * against whatever this resolver returns right now - the same "live
 * reference, not a snapshot" contract every other MindWave binding in this
 * codebase already keeps (see `InstrumentConfiguration`'s own class docs).
 *
 * Paint rebuild has no `Project` access of its own (unlike `compositor.cpp`,
 * which calls `compositeProject()` with one already in hand) -
 * `PaintController` supplies this resolver, reading straight from its own
 * live `Project`, mirroring `resolveFilterParameterMindWaves()`'s exact
 * per-id building block but living in Studio instead of Core.
 *
 * A default-constructed (empty) `MindWaveResolver` is a valid, meaningful
 * value - "no way to resolve a MindWaveId," which `applyPaintOperation()`
 * treats as "every binding resolves to `nullptr`" (no vibrato/tremolo at
 * all), the same "nothing to offer" convention `LayerContentResolver`'s own
 * empty default already establishes - callers with no Project to resolve
 * against (most of the test suite) simply omit this parameter.
 *
 * @param mindWaveId The id to resolve.
 * @return A pointer to that id's own current `MindWave`, or `nullptr` if it
 *         no longer resolves (its `NamedMindWave` was removed from the
 *         project) - the same graceful-dangling-id contract
 *         `resolveOpacityMindWave()`'s own docs establish. The pointer is
 *         only valid for the duration of the call it was returned from -
 *         never cached past that.
 */
using MindWaveResolver = std::function<const MindWave*(MindWaveId)>;

/**
 * @brief Resolves a layer's own *current* rendered content, by id - what
 *        a `MindGrainConfiguration` stamp needs to read its live source
 *        from (see `applyPaintOperation()`'s own `MindGrainConfiguration`
 *        branch), since every other tool type here needs no visibility
 *        into any layer but the one actually being painted.
 *
 * A default-constructed (empty) `LayerContentResolver` is a valid,
 * meaningful value - "no way to resolve another layer's content," which
 * `applyPaintOperation()` treats as a no-op for a Mind Grain stamp (the
 * same "nothing to paint" convention an empty `Clip` already gets for a
 * Mind Shot) rather than an error - callers with no Mind Grain support to
 * offer (most of the test suite) simply omit this parameter.
 *
 * @param layerId The layer to resolve.
 * @return A pointer to that layer's own current content, or `nullptr` if
 *         the layer doesn't exist or has no content yet. The pointer is
 *         only valid for the duration of the call it was returned from -
 *         never cached past that.
 */
using LayerContentResolver = std::function<const sound_mind::codec::StreamImage*(LayerId layerId)>;

/**
 * @brief The flat index into a `StreamImage`'s own row-major
 *        `leftMagnitudeDb`/`rightMagnitudeDb`/`sharedPhaseRadians` arrays
 *        for a given `(bin, frame)` cell.
 *
 * Every content-editing `Operation`'s own apply function
 * (`applyPaintOperation()`, `applyFillOperation()`, `applyPasteOperation()`,
 * and `filter_application.cpp`'s own `applyFrequencyAxisGradient()`) needs
 * this same `bin * frameCount + frame` arithmetic - shared here as one
 * spelling rather than four near-identical ones (Refactor & Clean Up,
 * `v0.Y.29.1`).
 *
 * @param bin The row index.
 * @param frame The column index.
 * @param frameCount The row length (`StreamImage::frameCount`).
 * @return `bin * frameCount + frame`, as a `std::size_t`.
 */
template <typename BinIndex, typename FrameIndex>
[[nodiscard]] constexpr std::size_t cellIndex(BinIndex bin, FrameIndex frame, std::uint32_t frameCount) noexcept {
    return static_cast<std::size_t>(bin) * static_cast<std::size_t>(frameCount) + static_cast<std::size_t>(frame);
}

/**
 * @brief Blends `left`/`right` toward `stop`'s own target intensities, in
 *        place: `newValue = oldValue + (targetValue - oldValue) *
 *        (opacity * weight)`.
 *
 * Every `Gradient`-driven amplitude edit in this codebase shares this
 * exact formula - `applyPaintOperation()`'s own per-pixel falloff-weighted
 * blend, `applyFillOperation()`'s uniform blend, and
 * `filter_application.cpp`'s `FrequencyAxisGradient`/Equalizer blend -
 * shared here as one spelling rather than three near-identical ones
 * (Refactor & Clean Up, `v0.Y.29.1`).
 *
 * @param left The left-channel dB value to blend toward
 *        `stop.leftIntensity`, in place.
 * @param right The right-channel dB value to blend toward
 *        `stop.rightIntensity`, in place.
 * @param stop Supplies both channels' own target intensity and opacity.
 * @param weight An additional multiplier on both channels' own opacity -
 *        `1.0` (the default) for callers with no extra per-pixel weight
 *        of their own (Fill, FrequencyAxisGradient); `applyPaintOperation()`
 *        passes its own `[0, 1]` per-pixel falloff weight here instead.
 */
constexpr void blendTowardStop(float& left, float& right, const GradientStop& stop, float weight = 1.0f) noexcept {
    left += (stop.leftIntensity - left) * (stop.leftOpacity * weight);
    right += (stop.rightIntensity - right) * (stop.rightOpacity * weight);
}

/**
 * @brief Evaluates a cubic Bézier curve at parameter `t` in `[0, 1]`.
 *
 * Exposed here (rather than staying private to whichever `.cpp` first
 * needed it) specifically so `path.cpp`'s own `containsPoint()` - the
 * point-in-closed-path test behind a Lasso selection's membership,
 * `v0.Y.35.1` Installment A - can tessellate a `Path`'s own curved
 * (`Smooth`) segments into straight sub-segments using the exact same
 * formula `sampleStrokeDense()` already uses to turn a Path into brush
 * stamp positions, rather than a second, independently-written copy of
 * this one small formula.
 *
 * @param p0 The segment's own start anchor.
 * @param p1 The start anchor's own outgoing handle (or `p0` itself, for a
 *        `Corner` node with no handle - the curve degenerates to a
 *        straight line in that case, not an error).
 * @param p2 The end anchor's own incoming handle (or `p3` itself, same
 *        reasoning as `p1`).
 * @param p3 The segment's own end anchor.
 * @param t Where along the curve to evaluate, `0` (`p0`) to `1` (`p3`).
 * @return The interpolated point.
 */
[[nodiscard]] TimeFrequencyPoint evaluateCubicBezier(const TimeFrequencyPoint& p0, const TimeFrequencyPoint& p1,
                                                       const TimeFrequencyPoint& p2, const TimeFrequencyPoint& p3,
                                                       double t) noexcept;

/**
 * @brief One rectangle's own clamped frame/bin range within a
 *        `StreamImage` of the given `frameCount`/`binCount` - see
 *        `rangeFor()`'s own docs. A range with `frameHigh < frameLow` (or
 *        `binHigh < binLow`) means `bounds` fell entirely outside the
 *        image (nothing to do), the same "empty" convention `Clip`'s own
 *        default-constructed state already uses.
 */
struct FrameBinRange {
    int frameLow = 0;    ///< The lowest (inclusive) frame/column index.
    int frameHigh = -1;  ///< The highest (inclusive) frame/column index.
    int binLow = 0;      ///< The lowest (inclusive) bin/row index.
    int binHigh = -1;    ///< The highest (inclusive) bin/row index.
};

/**
 * @brief `bounds`'s own time/frequency extent, converted to a clamped
 *        frame/bin range within a `frameCount`-wide, `config.binCount`-tall
 *        `StreamImage`.
 *
 * `applyFillOperation()` and `sound_mind::core::captureClip()`/
 * `applyPasteOperation()` (see `fill_application.h`/`paste_application.h`)
 * each need to turn a `TimeFrequencyRect` into concrete array indices the
 * same way - shared here (Refactor & Clean Up, `v0.Y.29.1`) so a fill's
 * own bounds and a paste's own clip-capture/placement origin always agree
 * on exactly the same corner for the same `bounds`, rather than risking
 * two independently-maintained copies of this rounding/clamping drifting
 * apart.
 *
 * @param bounds The rectangle to convert - `startTimeSeconds`/
 *        `endTimeSeconds` and `lowFrequencyHz`/`highFrequencyHz` need not
 *        already be in low-to-high order.
 * @param config The project's own Stream codec configuration.
 * @param frameCount The row length to clamp against (a `StreamImage`'s own
 *        `frameCount` - not always `config`'s own, since a layer's content
 *        can have fewer/more frames than the project's current settings).
 * @return The clamped `[frameLow, frameHigh] x [binLow, binHigh]` range -
 *         see `FrameBinRange`'s own docs for what an empty result means.
 */
[[nodiscard]] FrameBinRange rangeFor(const TimeFrequencyRect& bounds, const sound_mind::codec::StreamCodecConfig& config,
                                       std::uint32_t frameCount) noexcept;

/**
 * @brief The fractional log-scale bin (row) index `frequencyHz` maps to in
 *        `config`'s own Stream storage.
 *
 * The exact mapping `sound_mind::codec::encode()` uses internally to place
 * a real frequency into its log-spaced bin - re-derived here (rather than
 * reused directly) since that formula lives in Codec's private
 * implementation, not its public API; painting needs to target the same
 * pixel a given Hz value would actually be stored at, so this must be kept
 * in sync with `stream_frame_codec.h`'s own `logBinToLinearBinIndex()`/
 * `linearBinToLogBinIndex()` if that formula ever changes.
 *
 * @param frequencyHz The frequency to convert; clamped to
 *        `[config.minFrequencyHz, min(config.maxFrequencyHz, nyquist)]`
 *        first, the same clamping `encode()` itself applies.
 * @param config The project's own Stream codec configuration.
 * @return The fractional bin index, in `[0, config.binCount - 1]`.
 */
[[nodiscard]] float frequencyToBinIndex(float frequencyHz, const sound_mind::codec::StreamCodecConfig& config) noexcept;

/**
 * @brief The fractional column (frame) index `timeSeconds` maps to in
 *        `config`'s own Stream storage - the time axis is linear (see
 *        `docs/sound-mind-design.md`'s "Time Scale"), so this is a plain
 *        `timeSeconds * sampleRateHz / hopLength`, unlike the log-scale
 *        frequency axis.
 * @param timeSeconds The time to convert; not clamped - a caller painting
 *        near a layer's own edge may legitimately need an out-of-range
 *        index to know a stamp falls (partially) outside stored content.
 * @param config The project's own Stream codec configuration.
 * @return The fractional frame index.
 */
[[nodiscard]] double timeToFrameIndex(double timeSeconds, const sound_mind::codec::StreamCodecConfig& config) noexcept;

/**
 * @brief The inverse of frequencyToBinIndex(): the frequency a fractional
 *        bin (row) index corresponds to.
 *
 * Needed wherever a real position (a mouse click, a canvas pixel) has to
 * be converted *into* time/frequency space rather than the other
 * direction painting itself needs - see `sound-mind-studio`'s canvas
 * mouse-to-domain conversion.
 *
 * @param binIndex The fractional bin index to convert; clamped to
 *        `[0, config.binCount - 1]` first.
 * @param config The project's own Stream codec configuration.
 * @return The corresponding frequency, in Hz.
 */
[[nodiscard]] float binIndexToFrequency(float binIndex, const sound_mind::codec::StreamCodecConfig& config) noexcept;

/**
 * @brief `frequencyHz`, shifted by `deltaBins` in the log-scaled bin
 *        space `frequencyToBinIndex()`/`binIndexToFrequency()` establish
 *        - not a raw Hz shift.
 *
 * The frequency axis is log-scaled, so adding a fixed Hz amount to two
 * different frequencies doesn't shift them by the same *bin* (on-screen
 * pixel-equivalent) amount - a whole-object move (`Path::translated()`,
 * `sound_mind::core::translated(TimeFrequencyRect, ...)`) or a path node/
 * handle drag (`PickController::continuePathNodeDrag()`) built from a
 * raw Hz delta would visibly distort the moved shape instead of just
 * translating it, worse the wider a frequency range it spans - and near
 * `minFrequencyHz`, can drive a bound negative entirely. Converting
 * `frequencyHz` to its own bin position first, shifting *that*, then
 * converting back keeps a dragged object's own on-screen shape intact
 * and tracks the mouse - itself moving in screen-space pixels - 1:1.
 *
 * @param frequencyHz The frequency to shift.
 * @param deltaBins How far to shift, in bins - not Hz.
 * @param config The project's own Stream codec configuration.
 * @return The shifted frequency, in Hz.
 */
[[nodiscard]] float translateFrequencyByBins(float frequencyHz, double deltaBins,
                                               const sound_mind::codec::StreamCodecConfig& config) noexcept;

/**
 * @brief The inverse of timeToFrameIndex(): the time a fractional frame
 *        (column) index corresponds to.
 * @param frameIndex The fractional frame index to convert.
 * @param config The project's own Stream codec configuration.
 * @return The corresponding time, in seconds.
 */
[[nodiscard]] double frameIndexToTime(double frameIndex, const sound_mind::codec::StreamCodecConfig& config) noexcept;

/**
 * @brief Applies a `PaintOperation`'s own stroke directly onto a
 *        `StreamImage`'s amplitude planes, in place - dispatches on
 *        `operation.config().type()` to whichever concrete tool's own
 *        stamp algorithm applies (see `docs/sound-mind-design.md`'s
 *        "Procedural Brushes"/"Sound Mind Instruments"/"Mind Shots"/"Mind
 *        Grains"/"Heal"/"Soften"/"Smudge"/"Order/Chaos" and "What Editing
 *        Does": painting amplitude pixels brighter/darker changes that
 *        frequency's loudness at that time). Any tool type past
 *        `Procedural`/`Instrument`/`MindShot`/`MindGrain`/`Heal`/`Soften`/
 *        `Smudge`/`OrderChaos` (namely `Clone`) paints nothing yet,
 *        matching `ToolConfiguration`'s own "groundwork, not yet
 *        functional" note for that tool type.
 *
 * Stamps are placed repeatedly along `operation.path()`, spaced per
 * `operation.config().stampMode()` (see `docs/sound-mind-design.md`'s
 * "Stamp Intervals") the same way regardless of tool type: `Stroke`
 * (the default) stamps exactly as densely as the stroke's own raw input
 * was sampled - dense enough that consecutive stamps overlap into a
 * solid stroke rather than a series of dots, but not a fixed, chosen
 * spacing the way every other mode below uses;
 * `AlongCurve`/`TimeAxis`/`FrequencyAxis` instead space stamps
 * `stampInterval()` apart - by arc length, or wherever the path crosses a
 * time/frequency grid line, respectively - producing visibly separate
 * stamps rather than a solid stroke. What each stamp actually *paints*
 * differs by tool type:
 *
 * - **`ProceduralConfiguration`**: every pixel within the tip's own 2D
 *   (time and frequency) radius blends toward
 *   `operation.path().gradient()`'s target intensity (written directly as
 *   the new dB value - a `GradientStop`'s `leftIntensity`/`rightIntensity`
 *   *are* target `leftMagnitudeDb`/`rightMagnitudeDb` values, an
 *   interpretation choice recorded in `docs/sound-mind-architecture.md`'s
 *   Decisions Made), weighted by that stop's own opacity *and* the tip's
 *   own per-pixel falloff (`1` at the stamp's center, fading to `0` at its
 *   edge) - `newDb = oldDb + (targetDb - oldDb) * (opacity *
 *   falloffWeight)`. Only `BrushTipShape::Circle`/`Square`/`Diamond` have
 *   a real, distinct footprint so far - every other tip shape falls back
 *   to `Circle`'s own footprint (not a crash or undefined pixel data,
 *   just not yet visually distinct).
 * - **`InstrumentConfiguration`**: one bin-exact spike per harmonic above
 *   the stroke's own frequency at that point (see
 *   `InstrumentConfiguration::inharmonicity()`'s own docs for the
 *   stretched-partial formula), each blended toward the same gradient
 *   target as above but only along the *time* axis - `falloff()`/`size()`
 *   still bound and soften that blend, just across time alone, since a
 *   harmonic partial is a single exact frequency, not a 2D geometric blob -
 *   and scaled by that harmonic's own strength
 *   (`InstrumentConfiguration::harmonicStrengths()`). A harmonic stretched
 *   past the configured/Nyquist frequency range is skipped entirely
 *   (rather than clamped to the top bin, which would otherwise stack
 *   multiple high harmonics onto one bin).
 * - **`MindShotConfiguration`**: a hard, Normal-only overwrite of the
 *   configured `Clip`'s own cells, centered on the stamp position - no
 *   gradient, no falloff, no scaling by `size()`. "Paints back exactly as
 *   it was when captured" (`docs/sound-mind-design.md`'s "Mind Shots")
 *   taken literally: every cell the clip covers is written verbatim, the
 *   same direct-overwrite blit `applyPasteOperation()` already uses, just
 *   centered on a stamp position instead of an explicit placement
 *   rectangle. A no-op if no Mind Shot has ever been configured (an empty
 *   clip).
 * - **`MindGrainConfiguration`**: the same hard, Normal-only overwrite blit
 *   as `MindShotConfiguration` above, but the cells it stamps are never
 *   captured up front - each and every stamp re-resolves
 *   `resolveLayerContent(config.sourceLayerId())` and re-extracts a fresh
 *   `Clip` from `config.bounds()` of whatever that call returns, right
 *   before blitting it. "Live" here means "as of whenever this function is
 *   called" (see `rebuildPaintedContent()`'s own docs) - painting a Mind
 *   Grain stroke reads its source layer's content at the moment the stroke
 *   itself is (re)applied, not a snapshot frozen at configure time the way
 *   a Mind Shot is. This function itself never decides *when* that
 *   happens - `PaintController::rebuildLayerContent()` (Studio-side) is
 *   what actually cascades a fresh call here the instant a Mind Grain's own
 *   source layer changes, rather than waiting for this stroke's own layer
 *   to rebuild for some unrelated reason. A no-op if `resolveLayerContent`
 *   is empty, the resolved layer doesn't exist/has no content, or the
 *   resulting clip is empty (`config.bounds()` outside the source's own
 *   extent).
 * - **`HealConfiguration`**: within the same 2D falloff-weighted footprint
 *   `ProceduralConfiguration` uses, every pixel blends toward a plain box
 *   average of its own neighboring cells *along the time axis only, same
 *   bin* - the window's own half-width is `size()`'s own frame-radius
 *   (reused, not a separate parameter - see `HealConfiguration`'s own
 *   docs), and blend strength is the stroke's own gradient stop *opacity*
 *   at that point (intensity unused - there's no fixed target to paint
 *   toward, only how much of the local average to keep). Never touches
 *   `sharedPhaseRadians`, matching `filter_application.cpp`'s own blur
 *   filters. The box average is computed from a snapshot of the canvas
 *   taken *before* each individual stamp - not the same buffer being
 *   written into mid-stamp - so a stamp's own blend never picks up a
 *   scanline-order bias from cells it already touched earlier in that same
 *   stamp (overlapping *stamps*, or a *repeated* stroke, still compound
 *   normally on top of each other, the same as every other tool type -
 *   only a single stamp's own internal blend is order-independent).
 * - **`SoftenConfiguration`**: the same blend as `HealConfiguration` above,
 *   but isotropic - the box average spans both the time and frequency axes
 *   (both reusing `size()`'s own radius), for a uniform, undirected
 *   softening rather than Heal's own time-axis-only, defect-erasing blend.
 * - **`SmudgeConfiguration`**: the same shape as `HealConfiguration` again,
 *   but the average is sampled along a *line* through each footprint pixel
 *   - oriented along the stroke's own local direction (from the previous
 *   stroke sample to this one, or the next one for the very first sample)
 *   and spanning that same hop's own distance - rather than an axis-aligned
 *   box. A single-point stroke (no neighboring sample to derive a direction
 *   from) is a no-op.
 * - **`OrderChaosConfiguration`**: within the same footprint, `amount()`'s
 *   own sign picks Chaos (negative - randomly permutes a fraction of the
 *   footprint's own pixel values among themselves, preserving their total/
 *   average/histogram exactly at full opacity) or Order (positive - finds
 *   the footprint's own loudest frame/bin and reassigns a fraction of
 *   pixels so the brightest end up closest to those two lines, the darkest
 *   farthest, concentrating energy into an emergent cross); `0` (the
 *   default) is a no-op. Blend strength is still the stroke's own gradient
 *   stop opacity, same as every blur/rearrange tool type - `amount()`
 *   itself controls *how much of the footprint participates*, an orthogonal
 *   dial (see `OrderChaosConfiguration`'s own docs for why both exist).
 *
 * Overlapping stamps (a slow-moving stroke, the stroke's own path
 * doubling back on itself, an Instrument's own two harmonics landing on
 * the same bin, a Mind Shot/Mind Grain restamped repeatedly along a
 * dragged stroke, or a Heal/Soften/Smudge/Order-Chaos brush passed over the
 * same area more than once) compound naturally - for the gradient-blended
 * tool types (Procedural/Instrument/Heal/Soften/Smudge/OrderChaos), the
 * same way a real brush laid down more heavily builds up more effect; for a
 * Mind Shot or Mind Grain, each later stamp's own hard overwrite simply
 * wins over an earlier one
 * wherever they overlap. Neither is specially guarded against.
 *
 * @param operation The stroke to apply - its own `path()`/`config()`
 *        fully describe the stamp.
 * @param frequencyToTimeScale The same per-project normalization scale
 *        `fitPathToPoints()` takes (see `path.h`'s own docs) - needed here
 *        too, since `ToolConfiguration::size()` is in that same seconds-
 *        equivalent normalized space; must be positive.
 * @param content The `StreamImage` to paint into, mutated in place - its
 *        own `config` supplies the `frequencyToBinIndex()`/
 *        `timeToFrameIndex()` mapping actually used.
 * @param resolveLayerContent Resolves another layer's own current content
 *        by id - only consulted when `operation.config()` is a
 *        `MindGrainConfiguration` (see that branch above); every other
 *        tool type ignores it entirely. Defaults to an empty resolver,
 *        which makes any `MindGrainConfiguration` stroke a no-op - the
 *        correct behavior for every call site with no Mind Grain support
 *        to offer (nearly all of the existing test suite).
 * @param resolveMindWave Resolves `operation.config()`'s own
 *        `vibratoMindWave()`/`tremoloMindWave()` ids, if it's an
 *        `InstrumentConfiguration` - see `MindWaveResolver`'s own docs.
 *        Every other tool type ignores it entirely. Defaults to an empty
 *        resolver, meaning "no vibrato/tremolo at all" - the correct
 *        behavior for every call site with no Project to resolve against
 *        (nearly all of the existing test suite).
 */
void applyPaintOperation(const PaintOperation& operation, double frequencyToTimeScale,
                          sound_mind::codec::StreamImage& content,
                          const LayerContentResolver& resolveLayerContent = {},
                          const MindWaveResolver& resolveMindWave = {});

/**
 * @brief Rebuilds a layer's own painted content from scratch: a copy of
 *        `base` with every one of `operations` (in order) applied on top -
 *        the replay `docs/sound-mind-design.md`'s non-destructive model
 *        describes, needed so undo()/redo() (see `OperationLog`'s own
 *        docs) is actually reflected back in what's rendered/played, not
 *        just in which operations are logged as active.
 *
 * @param base The layer's own pre-paint content (its state before any
 *        `PaintOperation` ever targeted it - typically a snapshot taken
 *        the first time one does, per `Layer`'s own docs).
 * @param operations The operations to replay, in order - typically
 *        `OperationLog::activeOperationsTargeting()`'s own result for this
 *        layer. Each entry is dispatched to whichever concrete apply
 *        function matches its own runtime type (`applyPaintOperation()`
 *        for a `PaintOperation`, `applyFillOperation()` for a
 *        `FillOperation` - see `fill_application.h` -, `applyPasteOperation()`
 *        for a `PasteOperation` - see `paste_application.h`); any other/
 *        future `Operation` subtype is skipped rather than erroring, the
 *        same forward-tolerant handling every other `dynamic_cast`-based
 *        dispatch in this codebase already uses.
 * @param frequencyToTimeScale Passed through to applyPaintOperation() for
 *        each `PaintOperation` replayed - see its own docs. `FillOperation`
 *        and `PasteOperation` have no equivalent need for it (see their own
 *        docs).
 * @param resolveLayerContent Passed through to applyPaintOperation() for
 *        each `PaintOperation` replayed, unused by every other operation
 *        type - see its own docs. Defaults to an empty resolver, meaning
 *        "no Mind Grain support" (any `MindGrainConfiguration` stroke
 *        replayed becomes a no-op) unless the caller supplies one. This
 *        function itself has no notion of *when* it's called or why - a
 *        Mind Grain stroke replayed here always reads its source layer's
 *        content as of whatever `resolveLayerContent` returns *right now*,
 *        whether that's this layer's own routine rebuild or a caller
 *        deliberately cascading a rebuild here because some other layer's
 *        content just changed (see `PaintController::rebuildLayerContent()`'s
 *        own docs, Studio-side, for that cascade - this function has no
 *        cascade logic of its own, and doesn't need any: reading "whatever
 *        the resolver says right now" is already correct regardless of
 *        what prompted the call).
 * @param resolveMindWave Passed through to applyPaintOperation() for each
 *        `PaintOperation` replayed - see its own docs. Defaults to an empty
 *        resolver, meaning every `InstrumentConfiguration` stroke replayed
 *        plays without vibrato/tremolo unless the caller supplies one.
 * @return A fresh `StreamImage`: `base`, with every operation in
 *         `operations` applied on top, in order.
 */
[[nodiscard]] sound_mind::codec::StreamImage rebuildPaintedContent(const sound_mind::codec::StreamImage& base,
                                                                     const std::vector<const Operation*>& operations,
                                                                     double frequencyToTimeScale,
                                                                     const LayerContentResolver& resolveLayerContent = {},
                                                                     const MindWaveResolver& resolveMindWave = {});

}  // namespace sound_mind::core
