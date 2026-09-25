#pragma once

#include <nlohmann/json.hpp>

namespace sound_mind::core {

/**
 * @brief How one piece of content combines with what's already there - a
 *        Layer's own contribution during compositing, a Paste's own clip
 *        onto its destination, or a Mind Shot/Mind Grain stamp onto the
 *        layer it's painted on - `docs/sound-mind-design.md`'s "Layers"
 *        ("Compositing") and `v0.Y.37.1` (Deferred Blend Modes)'s own
 *        confirmed scope covering all three together.
 *
 * Confirmed with the user directly against the legacy Python Studio's own
 * blend-mode engine (`image_ops/blend.py`) before any code - see
 * `applyBlendedCell()`'s own docs (`blend_mode_application.h`) for exactly
 * how each of these six is computed. `Normal` and `Overwrite` are **not**
 * new - they name this codebase's own two pre-existing behaviors precisely
 * (layer compositing's own already-shipped audio-style summing, and Paste/
 * Mind Shot's own already-shipped hard, unconditional overwrite,
 * respectively) - see each enumerator's own docs for why neither is a
 * departure from what already ships today. `Multiply`/`Screen`/`Overlay`/
 * `Difference`/`Add` are the "classic five" of the design doc's own
 * "usual image-editor catalogue", confirmed with the user as this
 * milestone's own scope over legacy's full 13-mode set (Color Burn/Dodge,
 * Min/Max, Reflect, Glow, Negation, XOR remain a documented future-work
 * item, not built here).
 */
enum class BlendMode {
    /// @brief Audio-style mixing, not image-style alpha-over - a layer's
    ///        amplitude converts to linear, scales by opacity as a linear
    ///        gain, and sums with everything else contributing at that
    ///        same cell (`docs/sound-mind-design.md`'s "Compositing", this
    ///        codebase's own long-since-shipped `v0.Y.27.1` behavior,
    ///        untouched by this milestone). The default for layers -
    ///        loading a project saved before this milestone existed always
    ///        gets this value, reproducing its own prior, only-ever-Normal
    ///        behavior exactly.
    Normal,
    /// @brief The incoming content replaces whatever's there, crossfaded
    ///        by opacity the same way every mode below is (`base * (1 -
    ///        opacity) + overlay * opacity`) - see `applyBlendedCell()`'s
    ///        own docs; real-world testing pass finding #19 fixed this to
    ///        respect opacity, having previously ignored it completely
    ///        through `v0.Y.37.1`. This was originally named after Paste's
    ///        and Mind Shot/Mind Grain stamping's own pre-existing, only-
    ///        ever behavior before that milestone
    ///        (`blitClipCentered()`/`applyPasteOperation()`'s own verbatim-
    ///        copy contract) - not a new mode being introduced so much as
    ///        an existing, already-shipped behavior finally being named and
    ///        made one selectable option among several; both of those call
    ///        sites always pass a fixed, full `opacity = 1.0` regardless of
    ///        blend mode, so finding #19's fix changes nothing observable
    ///        for either - only layer compositing, where opacity can
    ///        genuinely vary, sees a real difference. The default for
    ///        Paste/Mind Shot/Mind Grain, for the same old-project-
    ///        compatibility reason `Normal` is the default for layers.
    Overwrite,
    /// @brief `result = base * overlay` (in `dbToUnit()`-normalized space).
    Multiply,
    /// @brief `result = 1 - (1 - base) * (1 - overlay)` (in
    ///        `dbToUnit()`-normalized space) - `Multiply`'s own inverse.
    Screen,
    /// @brief `Multiply` where the base is dark, `Screen` where it's
    ///        light - see `applyBlendedCell()`'s own docs for the exact
    ///        branch point.
    Overlay,
    /// @brief `result = |base - overlay|` (in `dbToUnit()`-normalized space).
    Difference,
    /// @brief `result = base + overlay` (in `dbToUnit()`-normalized space,
    ///        clamped to `[0, 1]`) - not the same operation as `Normal`'s
    ///        own linear-amplitude sum, despite both being additive; see
    ///        `applyBlendedCell()`'s own docs for why the two live in
    ///        different numeric domains and are not interchangeable.
    Add,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(BlendMode, {
    {BlendMode::Normal, "normal"},
    {BlendMode::Overwrite, "overwrite"},
    {BlendMode::Multiply, "multiply"},
    {BlendMode::Screen, "screen"},
    {BlendMode::Overlay, "overlay"},
    {BlendMode::Difference, "difference"},
    {BlendMode::Add, "add"},
})
// clang-format on

}  // namespace sound_mind::core
