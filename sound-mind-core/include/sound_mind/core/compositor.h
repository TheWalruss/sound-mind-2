#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"

namespace sound_mind::core {

/**
 * @brief Thrown by compositeProject() when its own `shouldCancel` callback
 *        returns `true` - `docs/sound-mind-roadmap.md`'s finding #12
 *        (Installment H).
 *
 * A distinct type, not a plain `std::runtime_error`, specifically so a
 * caller can tell "cancelled on request" apart from a genuine compositing
 * failure. A separate type from `sound_mind::core::PoolCancelled`,
 * `sound_mind::codec::ExportCancelled`, and `sound_mind::studio::
 * ImportCancelled` - each names the operation it actually belongs to, and
 * "pool cancelled"/"export cancelled" both read backwards for a cancelled
 * playback composite.
 */
class CompositeCancelled : public std::runtime_error {
public:
    CompositeCancelled() : std::runtime_error("composite cancelled") {}
};

/**
 * @brief Renders a single layer's cached content as displayable pixels,
 *        placed onto a `canvasWidth`-wide time axis.
 *
 * A single-layer building block, used both directly (nothing else needs
 * an individual layer's own placed-but-unblended render right now) and as
 * the geometry compositeProject() below repeats per layer, in the
 * amplitude/phase domain instead of already-converted RGB pixels.
 *
 * Applies the layer's own horizontal transform - `layer.rescaleFactor()`
 * (stretches/compresses the content's own timeline, applied first) then
 * `layer.translationColumns()` (shifts the result earlier/later, applied
 * second) - and always returns an image exactly `canvasWidth` columns
 * wide: a gap left by translation, or a layer narrower than `canvasWidth`
 * even with no transform at all, is padded with black columns; anything
 * that would fall outside `[0, canvasWidth)` is cropped.
 *
 * @param layer The layer to render.
 * @param canvasWidth The project's canvas width, in pixels/columns - see
 *        `sound_mind::core::ProjectSettings::canvasWidth`.
 * @return The rendered RGB composite, exactly `canvasWidth` columns wide,
 *         or `std::nullopt` if the layer has no cached content yet
 *         (`layer.content()` is empty).
 */
[[nodiscard]] std::optional<sound_mind::codec::RgbImage> renderLayer(const Layer& layer,
                                                                      std::uint32_t canvasWidth);

/**
 * @brief Renders a small thumbnail of a single layer's own raw cached
 *        content - `docs/sound-mind-design.md`'s "Layer panel styling"
 *        ("a rescaled representation of their visual contents... diligent[ly]
 *        [rescaled] to minimize the effects of aliasing"), `v0.Y.44.1`
 *        (Layers Panel Redesign).
 *
 * Unlike renderLayer(), this ignores the layer's own `translationColumns()`/
 * `rescaleFactor()` entirely - a thumbnail is meant to help recognize a
 * layer's own visual content at a glance, not show where it sits on the
 * project's timeline (that's what the canvas itself already shows once
 * selected). `sound_mind::codec::downsampleAveraged()` does the actual
 * area-averaging shrink - not `renderLayer()`'s/`compositeProject()`'s own
 * nearest-neighbor `resampleHorizontally()`, which is correct for
 * *stretching/compressing a timeline* but would alias badly shrinking a
 * whole image down to icon size.
 *
 * @param layer The layer to render a thumbnail for.
 * @param width The thumbnail's own width, in pixels.
 * @param height The thumbnail's own height, in pixels.
 * @return The rendered thumbnail, exactly `width` x `height`, or
 *         `std::nullopt` if the layer has no cached content yet
 *         (`layer.content()` is empty) - the same condition renderLayer()
 *         itself returns `std::nullopt` for.
 */
[[nodiscard]] std::optional<sound_mind::codec::RgbImage> renderLayerThumbnail(const Layer& layer, std::uint32_t width,
                                                                               std::uint32_t height);

/**
 * @brief Renders a layer's own total-amplitude-across-all-frequencies
 *        summary, as a grayscale strip - Composer Mode's "Amplitude" track
 *        background (`docs/sound-mind-design.md`'s "Composer Mode",
 *        `v0.Y.48.1` Installment A).
 *
 * Each output column's own brightness reflects that column's own average
 * amplitude across every encoded bin, converted to linear before
 * averaging (dB values themselves aren't meaningfully additive) and back
 * to dB afterward. The result is a synthetic, single-bin-tall `StreamImage`
 * of those per-column averages, rendered through the exact same
 * `sound_mind::codec::toRgbImage()` every other spectrogram view already
 * uses - reusing its own established dB-to-brightness mapping rather than
 * inventing a second one - then stretched to `width` x `height` via
 * `sound_mind::codec::downsampleAveraged()`, the same "squash cleanly,
 * don't alias" technique `renderLayerThumbnail()` already uses.
 *
 * @param layer The layer to summarize.
 * @param width The summary's own width, in pixels.
 * @param height The summary's own height, in pixels.
 * @return The rendered summary, exactly `width` x `height`, or
 *         `std::nullopt` if the layer has no cached content yet, or that
 *         content has no bins/frames to summarize - the same conditions
 *         `renderLayerThumbnail()` itself returns `std::nullopt` for.
 */
[[nodiscard]] std::optional<sound_mind::codec::RgbImage> renderLayerAmplitudeSummary(const Layer& layer,
                                                                                       std::uint32_t width,
                                                                                       std::uint32_t height);

/**
 * @brief A layer's own loudness, in dB, at one instant of the project's
 *        own shared timeline - the Layers Panel's own per-layer loudness
 *        indicator's live value during Playback/Loop, or the value shown
 *        while paused (`docs/sound-mind-roadmap.md`'s `v0.Y.52.1`,
 *        Analysis Tools v1).
 *
 * `profile` is a `computeLoudnessProfile()` result over the layer's own
 * *raw* content (before translation/rescale) - a caller polling this every
 * playback frame is expected to compute it once (and cache it, the same
 * way `LayerController::thumbnailCache_` already caches thumbnails per
 * layer id) rather than recomputing it on every call.
 *
 * `playheadFraction`/`canvasWidth` locate the requested instant on the
 * project's own shared timeline, exactly as `CanvasWidget::
 * setPlayheadFraction()` already does for the visible playhead line;
 * `rescaleFactor`/`translationColumns` then map that project-space column
 * back into `profile`'s own raw column space - the same geometry
 * `compositeProject()` itself already applies per layer, reused here via
 * `sourceColumnFor()` rather than re-derived.
 *
 * @param profile The layer's own loudness profile.
 * @param playheadFraction The playback position, as a fraction of the
 *        total loaded duration, in `[0, 1]` (clamped if outside it).
 * @param canvasWidth The project's own canvas width, in columns - see
 *        `ProjectSettings::canvasWidth`.
 * @param rescaleFactor The layer's own `Layer::rescaleFactor()`.
 * @param translationColumns The layer's own `Layer::translationColumns()`.
 * @return That layer's own loudness at this instant, or `std::nullopt` if
 *         `profile` is empty, `canvasWidth` is `0`, or this instant falls
 *         outside the layer's own (rescaled, translated) content - nothing
 *         playing there right now, matching `sourceColumnFor()`'s/
 *         `renderLayer()`'s own "nothing there" case.
 */
[[nodiscard]] std::optional<float> loudnessAtProjectColumn(const std::vector<float>& profile,
                                                            double playheadFraction, std::uint32_t canvasWidth,
                                                            double rescaleFactor, std::int64_t translationColumns);

/**
 * @brief Composites every visible, contentful layer in `project` into one
 *        combined `StreamImage`, per `docs/sound-mind-design.md`'s new
 *        "Compositing" subsection and `docs/sound-mind-roadmap.md`'s
 *        `v0.Y.27.1` (Multi-layer Compositing) - this is the real
 *        multi-layer composite `renderLayer()`'s own docs, and several
 *        milestones before it, described as still-future work.
 *
 * **Normal compositing is audio-style mixing, not image-style
 * alpha-over**: each layer's own stored amplitude (dB) converts to
 * linear, is scaled by that layer's own `opacity()` acting as a linear
 * gain - further scaled, per cell, by that layer's own bound `MindWave`
 * (`Layer::opacityMindWave()`), if any, evaluated at that cell's own
 * canvas position (`v0.Y.31.1` Installment C1; see `Layer::
 * opacityMindWave()`'s own docs) - and sums with every other visible
 * layer's own contribution at
 * the same output bin/column - as a complex value, using each layer's
 * own `sharedPhaseRadians` to give its (per-channel) linear amplitude a
 * direction before summing. The summed left/right complex values'
 * magnitudes become the composite's own left/right amplitude (converted
 * back to dB); the composite's own single shared phase is the *angle* of
 * the summed mono/mid signal (`(left + right) / 2`) - the same
 * "approximate shared phase from a mono downmix" contract a single
 * layer's own encode() already establishes (see `StreamImage`'s own
 * docs), just applied to the sum of every layer instead of one. A fully
 * opaque layer never mutes what's beneath it this way, unlike image
 * alpha-over - summation has no such "coverage" concept, and is also
 * commutative, so unlike alpha-over, stack *order* doesn't affect the
 * result (only which layers participate does).
 *
 * Every layer is placed onto the project's own `canvasWidth`-wide time
 * axis exactly the way renderLayer() places a single layer's own RGB
 * render - the same rescale-then-translate geometry, applied here to the
 * layer's own raw amplitude/phase columns instead of already-converted
 * pixels, so both this function and renderLayer() agree on where a given
 * layer's own content actually sits. A hidden layer (`layer.visible()`
 * is `false`) or one with no cached content contributes nothing, the
 * same as `renderLayer()`'s own single-layer convention.
 *
 * The returned image's own `config` comes from
 * `streamCodecConfigFor(project.settings())` (so every layer's amplitude
 * lines up bin-for-bin against a shared frequency range) - **except its
 * own `binCount`**, which instead is the tallest `binCount` among the
 * contributing layers' own cached content. The two numbers are identical
 * for any layer actually encoded from these same settings (the normal
 * case), but nothing enforces that in general, so this keeps the
 * composite matching what its own real content actually is rather than
 * padding or truncating against a project-level number that isn't
 * guaranteed to match it. `frameCount` is `project.settings().canvasWidth`,
 * and `sampleCount` is `frameCount * config.hopLength` - the exact
 * sample count `sound_mind::codec::decode()` should reconstruct for the
 * project's own full canvas-width duration. A layer whose own cached
 * content has fewer bins than the composite's own (tallest-among-layers)
 * `binCount` simply contributes nothing past its own bin range, rather
 * than being read out of bounds.
 *
 * @note **Fast path for the overwhelmingly common case of exactly one
 *       contributing layer**: summing a single term is the identity, so
 *       when only one layer is visible and has content, this skips the
 *       complex-domain math entirely (no `sin`/`cos`/`abs`/`log10` per
 *       cell) in favor of one gain shift plus a placement copy - this
 *       function runs on every canvas repaint, and most projects (and
 *       most moments even within a genuinely multi-layer one) have
 *       exactly one contributing layer at any given cell.
 * @note CPU-only, and not real-time-safe as written (allocates
 *       throughout) - the same characterization `sound_mind::codec::
 *       encode()`/`decode()` already carry, and consistent with
 *       `docs/sound-mind-roadmap.md`'s own confirmed scope for this
 *       milestone (a DirectX 12 Compute path is deliberately deferred
 *       to a dedicated performance pass, not built speculatively here).
 *
 * @param project The project to composite.
 * @param shouldCancel Consulted once per layer, before that layer is
 *        mixed in or applied as a filter - `docs/sound-mind-roadmap.md`'s
 *        finding #12 (Installment H). Once it returns `true`, throws
 *        `CompositeCancelled` immediately rather than processing that
 *        layer or any layer after it. Not consulted at all on the
 *        single-layer fast path (see the `@note` above) - that path has
 *        no per-layer loop to check between, and is specifically the
 *        case cheap enough not to need cancelling. `nullptr` (the
 *        default) never cancels - the exact prior behavior, unchanged for
 *        every existing caller.
 * @param respectMute Whether a muted layer (`Layer::muted()`) is also
 *        skipped, on top of the existing visible-only skip - "Layers
 *        Panel & Editing Enhancements v2" (`v0.Y.46.1` Installment B).
 *        `false` (the default, and every prior caller's exact behavior)
 *        includes a muted layer exactly like any other visible one - the
 *        live canvas render passes `false`, since a muted layer still
 *        needs to be *seen*. `true` additionally excludes it - every
 *        composite that actually drives audio playback (not just the
 *        canvas) passes `true`, so a muted layer is heard by neither.
 * @return The combined `StreamImage`, or `std::nullopt` if no layer in
 *         `project` contributes at all (every layer is invisible, muted
 *         with `respectMute` set, or has no cached content).
 * @throws CompositeCancelled if `shouldCancel` returns `true` - see its
 *         own docs.
 */
[[nodiscard]] std::optional<sound_mind::codec::StreamImage> compositeProject(
    const Project& project, const std::function<bool()>& shouldCancel = nullptr, bool respectMute = false);

}  // namespace sound_mind::core
