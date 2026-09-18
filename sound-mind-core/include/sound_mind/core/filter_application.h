#pragma once

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::core {

/**
 * @brief Every one of `FilterConfiguration`'s five bindable parameters'
 *        own resolved MindWave, if bound - `applyFilter()`'s own per-cell
 *        parameter-binding entry point (`v0.Y.31.1` Installment D).
 *
 * Deliberately plain pointers, not `FilterConfiguration`'s own
 * `std::optional<MindWaveId>` fields directly - resolving a `MindWaveId`
 * against a `Project`'s own library is `compositeProject()`'s own job
 * (mirroring `resolveOpacityMindWave()`'s exact precedent from Installment
 * C1), the same "applyFilter() knows nothing about Project/NamedMindWave"
 * boundary `FilterConfiguration` itself already keeps. All five default to
 * `nullptr` (no binding at all - every parameter behaves exactly as it did
 * before this installment).
 */
struct FilterParameterMindWaves {
    /// @brief Resolved `FilterConfiguration::blurSigmaMindWave()`.
    const MindWave* blurSigma = nullptr;
    /// @brief Resolved `FilterConfiguration::medianSizeMindWave()`.
    const MindWave* medianSize = nullptr;
    /// @brief Resolved `FilterConfiguration::directionalBlurLengthMindWave()`.
    const MindWave* directionalBlurLength = nullptr;
    /// @brief Resolved `FilterConfiguration::directionalBlurAngleMindWave()`.
    const MindWave* directionalBlurAngle = nullptr;
    /// @brief Resolved `FilterConfiguration::sharpenAmountMindWave()`.
    const MindWave* sharpenAmount = nullptr;
};

/**
 * @brief Applies a Filter layer's own filter to `composite` - the actual
 *        DSP behind `docs/sound-mind-design.md`'s "Filter Layer", called
 *        by `compositeProject()` (`compositor.h`) once per Filter-type
 *        layer, on the running composite of everything beneath it.
 *
 * Dispatches on `config.type()`:
 * - **`FrequencyAxisGradient`** (this milestone's first real
 *   implementation - see `docs/sound-mind-roadmap.md`'s `v0.Y.28.1`,
 *   Installment A): `config.frequencyGradient()` is evaluated once per
 *   bin, at `t = bin / (binCount - 1)` (`t=0` the lowest encoded
 *   frequency, `t=1` the highest) - `binIndexToFrequency()`'s own
 *   log-scaled bin-to-Hz mapping already makes this exactly "normalized
 *   position across the encoded frequency range", with no separate Hz
 *   conversion needed. Every column in that bin's own row blends toward
 *   the resulting stop's own `leftIntensity`/`rightIntensity`, by
 *   `leftOpacity`/`rightOpacity` - the identical `lerp(existing, target,
 *   opacity)` blend `paint_application.cpp`'s own Fill/Paint already use
 *   for a `Gradient` (see `docs/sound-mind-architecture.md`'s own
 *   Decision recording why this reuses `Gradient` unchanged: the
 *   Equalizer layer's own "Cut" editor is this same filter, with its own
 *   UI simply always writing `leftIntensity`/`rightIntensity` at the
 *   silence floor and exposing only opacity, as "how much to cut").
 *   Phase is left untouched - only amplitude is affected.
 * - **`UniformBlur`/`EdgePreservingBlur`/`DirectionalBlur`/`Sharpen`**
 *   (`v0.Y.28.1` Installment B): each operates on `leftMagnitudeDb` and
 *   `rightMagnitudeDb` independently, directly in dB space (not converted
 *   to linear amplitude first - the same domain `FrequencyAxisGradient`
 *   and the legacy Python reference implementation both already operate
 *   in, since legacy's own normalized `[0,1]` pixel domain is just a
 *   linear rescale of dB). Phase is left untouched, matching
 *   `FrequencyAxisGradient`'s own precedent - legacy's optional
 *   `apply_to_phase` toggle isn't represented here. Every one of these
 *   four uses clamp-to-edge boundary handling (the nearest in-bounds
 *   cell stands in for anything off the edge of the grid) - a
 *   deliberate simplification, not a port of legacy's own
 *   per-filter-inconsistent boundary modes (`scipy.ndimage`'s
 *   default `reflect` for its Gaussian/median filters, an explicit
 *   `nearest` for its convolve-based motion blur - see
 *   `../sound-mind/packages/sound_mind_studio/src/sound_mind_studio/
 *   filters/core.py`). Radii/sizes/lengths are all in raw bin/column
 *   units, per `FilterConfiguration`'s own docs.
 *   - `UniformBlur`: a separable 2D Gaussian blur (`blurSigma()`,
 *     floored at `0.1` matching legacy), kernel truncated at 4 standard
 *     deviations (matching `scipy.ndimage.gaussian_filter`'s own default
 *     `truncate`).
 *   - `EdgePreservingBlur`: a 2D median filter over a square window
 *     (`medianSize()`, forced odd and at least `3` the same way legacy
 *     forces it: `max(3, size | 1)`).
 *   - `DirectionalBlur`: a line-shaped kernel stepped along
 *     `directionalBlurAngleDegrees()` for `directionalBlurLength()`
 *     samples each side of center, normalized to sum to 1 - ported
 *     directly from legacy's own `motion_blur_filter` algorithm (`0`°
 *     blurs along the time axis/columns, `90`° along the frequency
 *     axis/bins).
 *   - `Sharpen`: an unsharp mask - `original + sharpenAmount() *
 *     (original - gaussianBlur(original, sigma=1.0))`, the same fixed
 *     internal sigma legacy's own `sharpen_filter` uses.
 * - **`ToneCurve`** (`v0.Y.28.1` Installment C): each of `leftMagnitudeDb`/
 *   `rightMagnitudeDb` is remapped independently, per cell - the cell's
 *   own dB value normalizes to `[0, 1]` (the same `-96..0` display range
 *   `color_mapping.cpp`'s own `dbToByte()`/`byteToDb()` establish,
 *   Core's own duplicated copy - see `dbToUnit()`/`unitToDb()` in
 *   `filter_application.cpp`), the normalized value is evaluated against
 *   `config.toneCurvePoints()` via `evaluateToneCurve()`
 *   (`tone_curve.h` - a monotone cubic Hermite spline; see its own docs
 *   for why monotone, and for how it differs from legacy's own
 *   `PchipInterpolator`-based curve), the result clamps back to
 *   `[0, 1]`, and converts back to dB. Phase is left untouched, matching
 *   every other filter's own precedent. The two mandatory endpoint
 *   control points (`{0, 0}`, `{1, 1}` by default - the identity curve)
 *   mean a fresh Filter layer of this type has no effect until a point
 *   is moved.
 *
 * - **`SpeckleAdd`/`SpeckleRemove`/`Denoise`/`BitDepthCrush`/
 *   `GranularNoise`/`DynamicSpeckle`/`FeedbackDistortion`/
 *   `SpectralWavefold`** (`v0.Y.36.1` Installment A, "Noise & distortion"):
 *   all eight operate on `leftMagnitudeDb`/`rightMagnitudeDb`
 *   independently, directly in dB space, phase left untouched - the same
 *   contract every prior filter type keeps. None of the eight bind to a
 *   MindWave yet (see `FilterConfiguration`'s own docs for why that's a
 *   deliberate deferral, not an oversight).
 *   - `SpeckleAdd`: randomly boosts `speckleDensity()`'s own fraction of
 *     cells toward `0`dB by `speckleIntensity()`, deterministically -
 *     which cells get hit is a hash of `noiseSeed()` and the cell's own
 *     `(bin, frame)` position, not a live RNG, so the same configuration
 *     always produces the same pattern (stable across every recomposite
 *     and reload).
 *   - `SpeckleRemove`: replaces a cell with its own local 3x3 median only
 *     where it differs by more than `speckleThresholdDb()` - narrower
 *     than `EdgePreservingBlur`'s own always-applied median, which also
 *     smooths real detail.
 *   - `Denoise`: a per-cell downward expander/spectral gate - attenuates
 *     anything at or below `noiseFloorDb()` by up to `reductionDb()`,
 *     with a 6dB soft knee straddling the floor to avoid a hard on/off
 *     click right at the threshold.
 *   - `BitDepthCrush`: quantizes the `dbToUnit()`-normalized loudness
 *     into `lerp(256, 2, crushAmount())` discrete steps - a true no-op at
 *     `crushAmount() <= 0`.
 *   - `GranularNoise`: like `SpeckleAdd`, deterministic per `noiseSeed()`,
 *     but hashed per `grainSize()` x `grainSize()` *block* (not per cell)
 *     - every cell in a block gets the exact same random dB offset within
 *     `[-grainAmountDb(), +grainAmountDb()]`.
 *   - `DynamicSpeckle`: `SpeckleAdd`'s live-noise sibling, sharing its
 *     `speckleDensity()`/`speckleIntensity()` fields - deliberately *not*
 *     `noiseSeed()`-deterministic, a genuinely fresh `thread_local` RNG
 *     roll every call (visible flicker across recomposites, confirmed
 *     with the user), computed over fixed 2x2 blocks rather than per cell
 *     to stay cheap on a large canvas.
 *   - `FeedbackDistortion`: a one-pole recursive filter along the time
 *     axis per bin (`y[frame] = (1 - amount) * x[frame] + amount *
 *     y[frame - 1]`, `amount` clamped to `[0, 0.99]` internally for
 *     stability) - a decaying resonant smear, not a static effect. Each
 *     bin's own first frame is always exactly unchanged (nothing to feed
 *     back from yet).
 *   - `SpectralWavefold`: a classic wavefolder - loudness beyond a
 *     threshold reflects back into range via a period-2 triangle wave
 *     rather than clipping, gained by `foldGain()` first (`1.0` is a true
 *     no-op; higher values produce progressively more folds).
 *
 * - **`ChannelBalance`/`Invert`/`Convolve`** (`v0.Y.36.1` Installment B,
 *   the rest of Tonal plus the rest of Spectral shaping): confirmed with
 *   the user directly against the legacy Python Studio's own
 *   implementations of all three. None of the three bind to a MindWave
 *   yet, the same deliberate deferral Installment A's eight Noise &
 *   distortion types already established.
 *   - `ChannelBalance`: the **only** filter type that genuinely mixes the
 *     two channels together rather than processing each independently -
 *     an energy-conserving pan law in *linear* amplitude (dB values can't
 *     be meaningfully summed directly): `total = left + right`, then
 *     `left = total * (1 - balance)`, `right = total * balance`.
 *     `balance = 0.5` only reproduces the input exactly on an
 *     already-balanced signal (`left == right` everywhere) - unlike every
 *     other filter's own input-independent "no-op" value.
 *   - `Invert`: `out = 1 - dbToUnit(in)` - an amplitude negative (quiet
 *     becomes loud, loud becomes quiet), matching legacy's own
 *     `invert_filter()` exactly. Not audio polarity/phase inversion
 *     (`out = -in` on a signed sample) - every filter in this codebase
 *     already operates in the normalized-amplitude/dB spectrogram domain,
 *     never on raw time-domain samples, so this is the operation that's
 *     actually consistent with every other filter type here. No
 *     parameters of its own.
 *   - `Convolve`: a plain 2D spatial convolution (`convolve2D()`) over the
 *     `binCount` x `frameCount` grid, clamp-to-edge boundary handling
 *     (matching every other spatial filter above), with an arbitrary,
 *     user-edited odd-sized square kernel (`convolveKernel()`/
 *     `convolveKernelSize()` - forced odd via `| 1`, matching
 *     `medianBlur2D()`'s own precedent), optional positive-coefficient-sum
 *     normalization (`convolveNormalize()` - otherwise a pure-positive
 *     kernel would brighten/darken the whole image by that sum), and a
 *     dry/wet mix (`convolveAmount()`, `0` = no-op regardless of the
 *     kernel). Ported directly from legacy's own `custom_convolve_filter()`
 *     (`scipy.ndimage.convolve(..., mode="nearest")`) - the design doc's
 *     own "custom spectral or temporal responses" phrasing is a single
 *     mechanism doing double duty by kernel shape/orientation (a
 *     horizontal-only kernel acts mostly across time, a vertical-only one
 *     mostly across frequency), not two separate implementations, matching
 *     legacy exactly. A malformed kernel (its own coefficient count not
 *     matching `kernelSize * kernelSize` - a corrupted/hand-edited project
 *     file) is treated as a no-op rather than indexed out of bounds.
 *
 * As of `v0.Y.31.1` (MindWaves v1) Installment D, any of the four
 * kernel-shape parameters (`blurSigma`/`medianSize`/
 * `directionalBlurLength`/`directionalBlurAngleDegrees`) named in
 * `mindWaves` is evaluated fresh at *every cell*, genuinely varying that
 * cell's own kernel - see `docs/sound-mind-design.md`'s "Filter
 * parameters" for why this is exact per-cell computation, not a blend of
 * two whole-image results. This is real additional work per bound
 * parameter (no longer separable/fixed-shape - each cell effectively gets
 * its own independently-sized kernel), unlike `sharpenAmount`, which
 * varies for free (it only scales an already-fixed difference term - see
 * `sharpen2D()`'s own docs). A bound parameter's own configured scalar
 * value (`blurSigma()` etc.) becomes the *ceiling* it reaches where the
 * MindWave is brightest, falling toward that parameter's own "no effect"
 * baseline (`0` for `blurSigma`/`directionalBlurLength`/`sharpenAmount`,
 * `1` for `medianSize`, `0`° for `directionalBlurAngleDegrees` - the last
 * has no true "no effect" angle, so `0`° is a natural default rather than
 * a claimed no-op) where it's dark. `ToneCurve`/`FrequencyAxisGradient`
 * have no bindable parameter at all - neither has a single number to bind.
 *
 * @param composite The running composite to filter - everything visible
 *        beneath the Filter layer this configuration belongs to, already
 *        composited (see `compositeProject()`'s own docs).
 * @param config Which filter to apply, and its own parameters.
 * @param settings The project settings `streamCodecConfigFor()` needs,
 *        for any filter whose own algorithm depends on the project's
 *        encoded frequency range/bin count.
 * @param mindWaves Each bindable parameter's own resolved MindWave, if
 *        any - see `FilterParameterMindWaves`'s own docs. Defaults to
 *        every parameter unbound, matching this function's own pre-
 *        Installment-D behavior exactly.
 * @return The filtered result, the same shape (`config`/`frameCount`) as
 *         `composite`.
 */
[[nodiscard]] sound_mind::codec::StreamImage applyFilter(const sound_mind::codec::StreamImage& composite,
                                                             const FilterConfiguration& config,
                                                             const ProjectSettings& settings,
                                                             const FilterParameterMindWaves& mindWaves = {});

}  // namespace sound_mind::core
