#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/gradient.h"
#include "sound_mind/core/operation.h"
#include "sound_mind/core/paint_operation.h"

namespace sound_mind::core {

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
 *        "Procedural Brushes"/"Sound Mind Instruments" and "What Editing
 *        Does": painting amplitude pixels brighter/darker changes that
 *        frequency's loudness at that time). Any tool type past
 *        `Procedural`/`Instrument` paints nothing yet, matching
 *        `ToolConfiguration`'s own "groundwork, not yet functional" note
 *        for those tool types.
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
 *
 * Overlapping stamps (a slow-moving stroke, the stroke's own path
 * doubling back on itself, or - for an Instrument - two harmonics landing
 * on the same bin) compound naturally, the same way a real brush laid
 * down more heavily builds up more paint - not specially guarded against.
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
 */
void applyPaintOperation(const PaintOperation& operation, double frequencyToTimeScale,
                          sound_mind::codec::StreamImage& content);

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
 * @return A fresh `StreamImage`: `base`, with every operation in
 *         `operations` applied on top, in order.
 */
[[nodiscard]] sound_mind::codec::StreamImage rebuildPaintedContent(const sound_mind::codec::StreamImage& base,
                                                                     const std::vector<const Operation*>& operations,
                                                                     double frequencyToTimeScale);

}  // namespace sound_mind::core
