#pragma once

#include "sound_mind/core/blend_mode.h"

namespace sound_mind::core {

/// @brief One cell's own blended left/right loudness and phase - see
/// `applyBlendedCell()`'s own docs.
struct BlendedCell {
    /// @brief Left channel amplitude, in dB.
    float leftMagnitudeDb;

    /// @brief Right channel amplitude, in dB.
    float rightMagnitudeDb;

    /// @brief The shared phase channel, in radians.
    float phaseRadians;
};

/**
 * @brief Blends `overlay` (the incoming content - a Layer's own
 *        contribution, a Paste's own clip, a Mind Shot/Mind Grain stamp)
 *        onto `base` (whatever's already there) at one cell, per `mode` -
 *        the single shared implementation behind layer compositing
 *        (`compositor.cpp`), Paste (`paste_application.cpp`), and Mind
 *        Shot/Mind Grain stamping (`paint_application.cpp`), confirmed
 *        with the user as `v0.Y.37.1`'s own settled scope (all three
 *        together, not layer compositing alone).
 *
 * Confirmed with the user directly against the legacy Python Studio's own
 * blend-mode engine (`image_ops/blend.py`'s `_blend_onto()`) before any
 * code - a real, deliberate domain split exists between `Normal` and
 * every other mode, not an inconsistency:
 *
 * - **`Normal`** stays in this codebase's own already-shipped, *linear-
 *   amplitude* domain (`dbToLinearAmplitude()`/`linearAmplitudeToDb()`) -
 *   `base`'s own linear amplitude plus `overlay`'s own linear amplitude,
 *   scaled by `opacity` as a linear gain first - exactly
 *   `compositor.cpp`'s own pre-`v0.Y.37.1` `mixLayerInto()` formula,
 *   untouched by this milestone. This is deliberately **not** the same
 *   numeric domain the other modes use: `Normal` is audio-style mixing
 *   (summing real acoustic energy), not an image-editor-style pixel
 *   operation, per `docs/sound-mind-design.md`'s own explicit framing.
 * - **`Overwrite`** now respects `opacity` via a plain linear crossfade of
 *   the *raw* dB/phase values (`base * (1 - opacity) + overlay * opacity`)
 *   - **as of real-world testing pass finding #19** (previously, through
 *   `v0.Y.37.1`, `opacity` was ignored entirely here, matching Paste/Mind
 *   Shot's own pre-`v0.Y.37.1` verbatim-copy behavior; found inconsistent
 *   with every other mode in real use and confirmed with the user to fix -
 *   see `docs/sound-mind-architecture.md`'s own Decision for the exact
 *   reasoning). Deliberately its own branch, not routed through the
 *   `dbToUnit()`-normalized path the five modes below share: that clamps
 *   to the `[-96, 0]`dB display range, which would silently corrupt
 *   Paste's/`captureClip()`'s own established "exact reproduction"
 *   contract even at `opacity = 1.0` (both intentionally exercise values
 *   outside that range as opaque data in their own tests, not real
 *   loudness) - a raw linear crossfade reproduces `overlay` bit-for-bit at
 *   `opacity = 1.0` and is numerically identical to the `dbToUnit()` path
 *   for any in-range value either way (`dbToUnit()` is itself an affine
 *   transform of dB, which commutes with linear interpolation). Both Paste
 *   and Mind Shot/Mind Grain stamping always call this with `opacity =
 *   1.0` regardless of blend mode (see each one's own call site), so this
 *   change has no actual behavioral effect there - it only changes real,
 *   observable output for layer compositing, where `opacity` can
 *   genuinely be less than `1.0`.
 * - **`Multiply`/`Screen`/`Overlay`/`Difference`/`Add`** all operate in
 *   `dbToUnit()`-normalized space (matching legacy's own pixel-normalized
 *   domain, and every other "image-editor style" filter already built in
 *   this codebase - `Invert`, `Convolve`, `BitDepthCrush`,
 *   `SpectralWavefold`), confirmed with the user over forcing them into
 *   `Normal`'s own linear-amplitude domain: `Multiply` = `base * overlay`;
 *   `Screen` = `1 - (1 - base) * (1 - overlay)`; `Overlay` = `Multiply`
 *   where `base < 0.5`, `Screen` otherwise; `Difference` = `|base -
 *   overlay|`; `Add` = `base + overlay` (clamped to `[0, 1]`) - each
 *   computed independently per channel (left, then right), then blended
 *   back with `base` via a final linear crossfade weighted by `opacity`
 *   (`base * (1 - opacity) + blended * opacity`), matching legacy's own
 *   "opacity applied as a final crossfade after the mode's own math"
 *   structure exactly.
 *
 * **`Overwrite`'s own phase** crossfades `base`'s and `overlay`'s own unit-
 * circle points (magnitude `1`, not weighted by loudness the way the five
 * modes below are - there's no in-range "unit" magnitude available here
 * without the same clamp this mode deliberately avoids) by `opacity`,
 * falling back to `overlay`'s own phase if the result is negligibly small
 * (only possible at `opacity` near `0.5` with exactly opposite phases).
 *
 * **Phase**, for these same five modes, ports legacy's own per-mode
 * formula (confirmed with the user over keeping phase treatment uniform
 * across every mode): each mode computes a complex "blended" value from
 * `base`'s and `overlay`'s own phase, using `(left + right) / 2`'s own
 * unit-normalized magnitude as each side's complex weight - the same
 * average this codebase's own pre-existing `Normal`-mode phase derivation
 * already uses (`compositor.cpp`'s own `mid = (newLeft + newRight) / 2`),
 * not legacy's own "always the first amplitude channel" convention, which
 * doesn't fit this codebase's own single shared-phase-for-both-channels
 * data model as cleanly:
 * - `Multiply`: phases add (`argOut = basePhase + overlayPhase`).
 * - `Screen`: the amplitude-weighted circular mean of `base`'s and
 *   `overlay`'s own complex values (falling back to `base`'s own phase
 *   if that sum is negligibly small).
 * - `Overlay`: `Multiply`'s own additive phase where `base < 0.5`,
 *   `Screen`'s own circular-mean phase otherwise - the same branch point
 *   the amplitude formula itself uses.
 * - `Difference`: the complex *difference* of `base`'s and `overlay`'s
 *   own values (not re-scaled to the amplitude result above - matching
 *   legacy's own decoupled amplitude/phase treatment for this mode
 *   exactly).
 * - `Add`: the complex *sum*, same reasoning as `Difference`.
 * Every mode's own resulting complex value is then crossfaded against
 * `base`'s own original complex value by `opacity` (a second, phase-space
 * crossfade, independent of the amplitude crossfade above - also matching
 * legacy exactly), and the final phase extracted from that - falling back
 * to `base`'s own phase if the result's own magnitude is negligibly small
 * (both inputs silent).
 *
 * @param mode Which blend mode to apply.
 * @param base The existing content at this cell (a layer's own running
 *        composite so far, a Paste's own destination, a stamp's own
 *        target).
 * @param overlay The incoming content at this cell (a Layer's own placed
 *        contribution, a Paste's own clip, a Mind Shot/Mind Grain's own
 *        captured content).
 * @param opacity How strongly `overlay` contributes, in `[0, 1]` - not
 *        clamped here. Respected by every mode, `BlendMode::Overwrite`
 *        included (as of real-world testing pass finding #19).
 * @return The blended cell.
 */
[[nodiscard]] BlendedCell applyBlendedCell(BlendMode mode, BlendedCell base, BlendedCell overlay, float opacity);

}  // namespace sound_mind::core
