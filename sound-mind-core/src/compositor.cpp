#include "sound_mind/core/compositor.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <exception>

#include "gpu_compute_access.h"
#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/codec/rgb_image_resample.h"
#include "sound_mind/core/blend_mode_application.h"
#include "sound_mind/core/filter_application.h"
#include "sound_mind/core/loudness_analysis.h"
#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::core {

namespace {

using sound_mind::codec::RgbImage;
using sound_mind::codec::StreamImage;

/// @brief Nearest-neighbor horizontal resample of `source` to `newWidth`
/// columns, keeping the same height - used to apply a layer's
/// rescaleFactor() to its own timeline before placeOnCanvas() below. A
/// no-op (returns `source` unchanged) when nothing would actually change,
/// to skip the allocation on the (common) unrescaled path.
RgbImage resampleHorizontally(const RgbImage& source, std::uint32_t newWidth) {
    if (newWidth == source.width || source.width == 0) {
        return source;
    }
    RgbImage result;
    result.width = newWidth;
    result.height = source.height;
    result.pixels.resize(std::size_t{newWidth} * source.height * 3);
    for (std::uint32_t x = 0; x < newWidth; ++x) {
        const auto sourceX = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(source.width - 1, (std::uint64_t{x} * source.width) / newWidth));
        for (std::uint32_t y = 0; y < source.height; ++y) {
            const std::size_t srcIndex = (std::size_t{y} * source.width + sourceX) * 3;
            const std::size_t dstIndex = (std::size_t{y} * newWidth + x) * 3;
            result.pixels[dstIndex] = source.pixels[srcIndex];
            result.pixels[dstIndex + 1] = source.pixels[srcIndex + 1];
            result.pixels[dstIndex + 2] = source.pixels[srcIndex + 2];
        }
    }
    return result;
}

/// @brief Places `source` onto a `canvasWidth`-wide black canvas, shifted
/// by `translationColumns` (positive = later/right) - see renderLayer()'s
/// own docs for the padding/cropping this produces.
RgbImage placeOnCanvas(const RgbImage& source, std::int64_t translationColumns, std::uint32_t canvasWidth) {
    RgbImage result;
    result.width = canvasWidth;
    result.height = source.height;
    result.pixels.assign(std::size_t{canvasWidth} * source.height * 3, 0);
    for (std::uint32_t x = 0; x < canvasWidth; ++x) {
        const std::int64_t sourceX = static_cast<std::int64_t>(x) - translationColumns;
        if (sourceX < 0 || sourceX >= static_cast<std::int64_t>(source.width)) {
            continue;  // stays black - outside the layer's (shifted) content.
        }
        for (std::uint32_t y = 0; y < source.height; ++y) {
            const std::size_t srcIndex = (std::size_t{y} * source.width + static_cast<std::uint32_t>(sourceX)) * 3;
            const std::size_t dstIndex = (std::size_t{y} * canvasWidth + x) * 3;
            result.pixels[dstIndex] = source.pixels[srcIndex];
            result.pixels[dstIndex + 1] = source.pixels[srcIndex + 1];
            result.pixels[dstIndex + 2] = source.pixels[srcIndex + 2];
        }
    }
    return result;
}

/// @brief The smallest linear amplitude linearAmplitudeToDb() below will
/// ever report as anything but this floor's own dB value - avoids
/// `log10(0)` for genuine silence, the same reasoning (and the same
/// value) `sound-mind-codec`'s own private per-frame STFT helpers already
/// use for exactly this.
constexpr float kMinLinearAmplitude = 1e-7f;

/// @brief Converts a stored dB amplitude to its linear equivalent -
/// compositeProject()'s own copy of the same one-line formula
/// `sound-mind-codec`'s private `stream_frame_codec.h` already has,
/// duplicated rather than shared across the module boundary: Core is
/// allowed to depend on Codec's public `include/` headers, but not on an
/// internal, unexported `src/` implementation file, and a two-line
/// formula isn't worth promoting to a shared public header over.
[[nodiscard]] float dbToLinearAmplitude(float db) noexcept { return std::pow(10.0f, db / 20.0f); }

/// @brief The inverse of dbToLinearAmplitude() - see its own docs.
[[nodiscard]] float linearAmplitudeToDb(float amplitude) noexcept {
    return 20.0f * std::log10(std::max(amplitude, kMinLinearAmplitude));
}

/// @brief The per-channel dB gain a layer's own `balance()` implies - see
/// `Layer::balance()`'s own docs for the exact linear pan law. Computed
/// once per layer (`balance()` isn't MindWave-bindable, unlike opacity -
/// deliberately out of scope for this installment, see
/// `docs/sound-mind-architecture.md`'s own Decision on this), then added
/// as a plain per-cell dB offset everywhere a layer's own placed content
/// gets read - the same "gain as dB addition" shortcut `compositeSingleLayer()`'s
/// own `gainDb` already uses for opacity.
struct BalanceGainsDb {
    float left;
    float right;
};

[[nodiscard]] BalanceGainsDb balanceGainsDbFor(float balance) noexcept {
    const float leftLinear = std::min(1.0f, 2.0f * (1.0f - balance));
    const float rightLinear = std::min(1.0f, 2.0f * balance);
    return {linearAmplitudeToDb(leftLinear), linearAmplitudeToDb(rightLinear)};
}

/// @brief `layer`'s own bound opacity MindWave, resolved against
/// `project`'s own library - `v0.Y.31.1` Installment C1's own opacity-
/// binding entry point. A `layer.opacityMindWave()` id that no longer
/// resolves (its `NamedMindWave` was removed from the project) returns
/// `nullptr`, the same as no binding at all - see `Layer::opacityMindWave()`'s
/// own docs on why this is graceful, not an error.
[[nodiscard]] const MindWave* resolveOpacityMindWave(const Layer& layer, const Project& project) noexcept {
    const auto& id = layer.opacityMindWave();
    if (!id.has_value()) {
        return nullptr;
    }
    const NamedMindWave* named = project.mindWaveById(*id);
    return named ? &named->wave : nullptr;
}

/// @brief `mindWaveId`, resolved against `project`'s own library - the
/// single-id building block `resolveFilterParameterMindWaves()` below
/// calls once per bindable parameter. Same graceful-dangling-id contract
/// as `resolveOpacityMindWave()`.
[[nodiscard]] const MindWave* resolveMindWaveId(std::optional<MindWaveId> mindWaveId, const Project& project) noexcept {
    if (!mindWaveId.has_value()) {
        return nullptr;
    }
    const NamedMindWave* named = project.mindWaveById(*mindWaveId);
    return named ? &named->wave : nullptr;
}

/// @brief `config`'s own twenty-one bindable filter parameters, each
/// resolved against `project`'s own library - `v0.Y.31.1` Installment D's
/// own filter-parameter-binding entry point (the first five), extended by
/// `v0.Y.38.1` (Filter Parameter Binding Completion) to fifteen more, and
/// by real-world testing pass finding #18 to a twenty-first
/// (`downsampleBlockSize`) - the `FilterConfiguration`-level counterpart to
/// `resolveOpacityMindWave()`. A thin adapter over `filter_application.h`'s
/// own generic-resolver `resolveFilterParameterMindWaves()` (`v0.Y.46.1`
/// Installment E) - the field list itself lives there now, shared with
/// `FilterOperation`'s own replay path instead of duplicated across both.
[[nodiscard]] FilterParameterMindWaves resolveFilterParameterMindWaves(const FilterConfiguration& config,
                                                                        const Project& project) noexcept {
    return sound_mind::core::resolveFilterParameterMindWaves(
        config, [&project](MindWaveId id) -> const MindWave* { return resolveMindWaveId(id, project); });
}

/// @brief The per-cell opacity multiplier `opacityMindWave` (if any)
/// contributes at `(bin, outputColumn)` - canvas-space, per `docs/
/// sound-mind-roadmap.md`'s own confirmed `v0.Y.31.1` scope ("every
/// binding this milestone builds is implicitly canvas-space"). This
/// depends only on the *output* cell's own canvas position, not on
/// `layer`'s own placement/rescale - a MindWave-bound layer's field moves
/// with the canvas, not with the layer's own content.
/// @return `1.0` (no effect) when `opacityMindWave` is `nullptr`.
[[nodiscard]] float mindWaveGainAt(const MindWave* opacityMindWave, std::uint32_t bin, std::uint32_t outputColumn,
                                    const sound_mind::codec::StreamCodecConfig& config) {
    if (!opacityMindWave) {
        return 1.0f;
    }
    return mindWaveValueAt(*opacityMindWave, bin, outputColumn, config);
}

/// @brief Builds a full `canvasWidth x config.binCount` array of
/// `mindWaveGainAt()`'s own per-cell value, matching `AmplitudePhaseSignal`'s
/// own flat row-major layout - `mixLayerIntoGpuOrCpu()`'s own GPU path
/// needs a real array (the whole point of `ComputeDevice::
/// mixAmplitudePhaseSignal()`'s own new per-cell field buffer), unlike the
/// CPU path, which can call `mindWaveGainAt()` inline per cell with no
/// array at all. Returns all-`1.0` (no effect), without evaluating
/// anything, when `opacityMindWave` is `nullptr` - the common, unbound
/// case skips every `MindWave::evaluate()` call entirely. Delegates to
/// `evaluateMindWaveField()` for the bound case (`v0.Y.45.1` Refactor &
/// Clean Up, Installment B) - previously its own independent copy of the
/// same bin/frame loop.
[[nodiscard]] std::vector<float> buildMindWaveField(const MindWave* opacityMindWave,
                                                     const sound_mind::codec::StreamCodecConfig& config,
                                                     std::uint32_t canvasWidth) {
    if (!opacityMindWave) {
        return std::vector<float>(std::size_t{config.binCount} * canvasWidth, 1.0f);
    }
    return evaluateMindWaveField(*opacityMindWave, config, canvasWidth);
}

/// @brief Applies `layer`'s own filter to `composite`, then blends the
/// result back toward `composite` itself (the pre-filter value) by
/// `layer`'s own opacity (MindWave-modulated where bound) - the single
/// call both compositeProject()'s and compositeProjectCached()'s own
/// per-layer folds make for a Filter-type layer, so the fix for a real-
/// world testing pass finding (`v0.Y.62.1` Installment B) lives in
/// exactly one place: `applyFilter()` itself has no "layer opacity"
/// concept at all (it transforms the running composite in place), so
/// without this, a Filter layer's own Opacity slider/Opacity MindWave
/// binding - shown and settable for it, same as any other layer type -
/// silently had zero effect on the rendered output.
///
/// Skips the extra copy-and-blend pass entirely when opacity is `1.0`
/// and no MindWave is bound (the default, overwhelmingly common case) -
/// exactly `applyFilter()`'s own prior, unblended behavior, at no added
/// cost. Otherwise reuses `applyBlendedCell(BlendMode::Overwrite, ...)` -
/// a plain linear crossfade of the raw dB/phase values between the pre-
/// and post-filter cell - rather than inventing new blend math: this is
/// exactly "how much of the filtered result replaces the original,"
/// which is `Overwrite`'s own already-tested semantic.
///
/// @param composite The running composite immediately before this
///        layer's own filter - consumed (moved from where possible).
/// @param layer The Filter layer to apply.
/// @param project Resolves `layer.opacityMindWave()` against its own
///        library - see `resolveOpacityMindWave()`'s own docs.
/// @param settings Passed through to `applyFilter()` - see its own docs.
/// @param config Supplies `binCount` - see `mindWaveGainAt()`'s own docs.
/// @param canvasWidth The composite's own width, in columns.
/// @return The filtered (and, where applicable, opacity-blended)
///         composite.
[[nodiscard]] StreamImage applyFilterRespectingOpacity(StreamImage composite, const Layer& layer,
                                                          const Project& project, const ProjectSettings& settings,
                                                          const sound_mind::codec::StreamCodecConfig& config,
                                                          std::uint32_t canvasWidth) {
    const MindWave* opacityMindWave = resolveOpacityMindWave(layer, project);
    if (layer.opacity() == 1.0f && opacityMindWave == nullptr) {
        return applyFilter(composite, layer.filterConfiguration(), settings,
                            resolveFilterParameterMindWaves(layer.filterConfiguration(), project));
    }

    const StreamImage preFilter = composite;
    StreamImage filtered = applyFilter(composite, layer.filterConfiguration(), settings,
                                         resolveFilterParameterMindWaves(layer.filterConfiguration(), project));
    for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
        for (std::uint32_t x = 0; x < canvasWidth; ++x) {
            const std::size_t cell = cellIndex(bin, x, canvasWidth);
            const float gain = layer.opacity() * mindWaveGainAt(opacityMindWave, bin, x, config);
            const BlendedCell base{preFilter.leftMagnitudeDb[cell], preFilter.rightMagnitudeDb[cell],
                                     preFilter.sharedPhaseRadians[cell]};
            const BlendedCell overlay{filtered.leftMagnitudeDb[cell], filtered.rightMagnitudeDb[cell],
                                        filtered.sharedPhaseRadians[cell]};
            const BlendedCell blended = applyBlendedCell(BlendMode::Overwrite, base, overlay, gain);
            filtered.leftMagnitudeDb[cell] = blended.leftMagnitudeDb;
            filtered.rightMagnitudeDb[cell] = blended.rightMagnitudeDb;
            filtered.sharedPhaseRadians[cell] = blended.phaseRadians;
        }
    }
    return filtered;
}

/// @brief The width a `sourceWidth`-wide sequence rescales to under
/// `rescaleFactor` - shared by `sourceColumnFor()` (bin-space placement,
/// below) and `renderLayer()` (RGB-pixel-space `resampleHorizontally()`'s
/// own target width), so the two stay in exact agreement for the same
/// layer (Refactor & Clean Up, `v0.Y.29.1`).
[[nodiscard]] std::uint32_t rescaledWidthFor(std::uint32_t sourceWidth, double rescaleFactor) noexcept {
    if (rescaleFactor == 1.0) {
        return sourceWidth;
    }
    const double scaled = static_cast<double>(sourceWidth) * rescaleFactor;
    return static_cast<std::uint32_t>(std::max(1.0, std::round(scaled)));
}

/// @brief The column of a `sourceWidth`-wide layer that `outputColumn`
/// (on the project's own `canvasWidth`-wide timeline) maps to, after
/// applying `rescaleFactor` (stretches/compresses the layer's own
/// timeline, applied first) then `translationColumns` (shifts the result
/// earlier/later, applied second) - the same geometry renderLayer()'s own
/// (RGB-pixel-space) resampleHorizontally()/placeOnCanvas() apply,
/// expressed here as a single index lookup rather than two intermediate
/// image copies, since compositeProject() needs this same lookup for
/// every layer at every output cell rather than once per whole image.
///
/// @return The source column, or `std::nullopt` if `outputColumn` falls
///         outside the layer's own (rescaled, translated) content -
///         nothing to place there, matching renderLayer()'s own
///         black-padding for the exact same case.
[[nodiscard]] std::optional<std::uint32_t> sourceColumnFor(std::uint32_t outputColumn, std::uint32_t sourceWidth,
                                                              double rescaleFactor,
                                                              std::int64_t translationColumns) noexcept {
    if (sourceWidth == 0) {
        return std::nullopt;
    }
    const std::uint32_t rescaledWidth = rescaledWidthFor(sourceWidth, rescaleFactor);
    const std::int64_t rescaledIndex = static_cast<std::int64_t>(outputColumn) - translationColumns;
    if (rescaledIndex < 0 || rescaledIndex >= static_cast<std::int64_t>(rescaledWidth)) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(
        sourceWidth - 1, (static_cast<std::uint64_t>(rescaledIndex) * sourceWidth) / rescaledWidth));
}

/// @brief Calls `onPlaced(bin, outputColumn, sourceCell, outputCell)` for
/// every `(bin, outputColumn)` cell in `[0, config.binCount) x
/// [0, canvasWidth)` where `layer`'s own (rescaled, translated) content
/// actually has something to place there (`bin` within the layer's own
/// `content()->config.binCount`, and `sourceColumnFor()` finds a source
/// column), and `onUnplaced(bin, outputColumn, outputCell)` for every
/// other cell in that same range.
///
/// The shared placement-geometry loop behind `mixLayerInto()`,
/// `placeLayerForGpuMix()`, and `compositeSingleLayer()` below - all
/// three independently implemented this exact nested loop/branch
/// structure before being consolidated here (Refactor & Clean Up,
/// `v0.Y.29.1`), risking the three copies silently drifting apart from
/// each other.
///
/// @param layer The layer being placed - must have content (`layer.content()`
///        must have a value); every caller here already guarantees this.
/// @param config Supplies `binCount`, the output row count to iterate.
/// @param canvasWidth The output column count to iterate.
/// @param onPlaced Called for each cell with real source data - see above.
/// @param onUnplaced Called for each cell with no real source data - see
///        above.
template <typename OnPlaced, typename OnUnplaced>
void forEachPlacedCell(const Layer& layer, const sound_mind::codec::StreamCodecConfig& config,
                       std::uint32_t canvasWidth, OnPlaced onPlaced, OnUnplaced onUnplaced) {
    const StreamImage& content = *layer.content();
    for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
        if (bin >= content.config.binCount) {
            // Nothing to add at this bin - see compositeProject()'s own
            // docs - for every column.
            for (std::uint32_t x = 0; x < canvasWidth; ++x) {
                onUnplaced(bin, x, cellIndex(bin, x, canvasWidth));
            }
            continue;
        }
        for (std::uint32_t x = 0; x < canvasWidth; ++x) {
            const std::size_t outputCell = cellIndex(bin, x, canvasWidth);
            const auto sourceColumn =
                sourceColumnFor(x, content.frameCount, layer.rescaleFactor(), layer.translationColumns());
            if (!sourceColumn.has_value()) {
                onUnplaced(bin, x, outputCell);
                continue;
            }
            onPlaced(bin, x, cellIndex(bin, *sourceColumn, content.frameCount), outputCell);
        }
    }
}

/// @brief `forEachPlacedCell()` above, for a caller with nothing to do on
/// an unplaced cell (leaving it untouched, e.g. `mixLayerInto()`'s own
/// "nothing to add here" semantics).
/// @param layer See the five-argument overload's own docs.
/// @param config See the five-argument overload's own docs.
/// @param canvasWidth See the five-argument overload's own docs.
/// @param onPlaced See the five-argument overload's own docs.
template <typename OnPlaced>
void forEachPlacedCell(const Layer& layer, const sound_mind::codec::StreamCodecConfig& config,
                       std::uint32_t canvasWidth, OnPlaced onPlaced) {
    forEachPlacedCell(layer, config, canvasWidth, onPlaced,
                       [](std::uint32_t /*bin*/, std::uint32_t /*outputColumn*/, std::size_t /*outputCell*/) {});
}

/// @brief Mixes `layer`'s own placed content into `running`, in place -
/// compositeProject()'s own general-path building block, called once per
/// Normal/Background contributor. Reads `running`'s own current dB/phase
/// and `layer`'s own (placed) content as a `BlendedCell` pair and dispatches
/// through `applyBlendedCell()` per `layer.blendMode()`, writing the result
/// back - see compositeProject()'s own docs for why this per-layer
/// incremental approach (rather than one N-way sum) is what lets a Filter
/// layer transform an in-progress composite mid-stack. `layer.balance()`'s
/// own per-channel dB gain (see `balanceGainsDbFor()`'s own docs) is added
/// straight into the `overlay` cell before blending - `v0.Y.46.1`
/// Installment C (Per-layer balance).
///
/// As of `v0.Y.37.1` (Deferred Blend Modes), this is the single shared path
/// for every blend mode, `BlendMode::Normal` included - `applyBlendedCell()`'s
/// own `Normal` case is mathematically identical to this function's own
/// pre-`v0.Y.37.1` formula (summing linear amplitude, scaled by opacity as a
/// linear gain), so unifying the two changes no existing project's own
/// composited result.
///
/// @param opacityMindWave `layer`'s own bound opacity MindWave (already
///        resolved against the project - see `resolveOpacityMindWave()`),
///        or `nullptr` if unbound - evaluated inline per cell via
///        `mindWaveGainAt()` and multiplied alongside `layer.opacity()` to
///        form `applyBlendedCell()`'s own per-cell `opacity` argument,
///        `v0.Y.31.1` Installment C1's own opacity-binding entry point.
void mixLayerInto(StreamImage& running, const Layer& layer, const sound_mind::codec::StreamCodecConfig& config,
                    std::uint32_t canvasWidth, const MindWave* opacityMindWave) {
    const StreamImage& content = *layer.content();
    const BalanceGainsDb balanceGainsDb = balanceGainsDbFor(layer.balance());
    forEachPlacedCell(
        layer, config, canvasWidth,
        [&](std::uint32_t bin, std::uint32_t outputColumn, std::size_t sourceCell, std::size_t outputCell) {
            const float gain = layer.opacity() * mindWaveGainAt(opacityMindWave, bin, outputColumn, config);
            const BlendedCell base{running.leftMagnitudeDb[outputCell], running.rightMagnitudeDb[outputCell],
                                    running.sharedPhaseRadians[outputCell]};
            const BlendedCell overlay{content.leftMagnitudeDb[sourceCell] + balanceGainsDb.left,
                                       content.rightMagnitudeDb[sourceCell] + balanceGainsDb.right,
                                       content.sharedPhaseRadians[sourceCell]};
            const BlendedCell blended = applyBlendedCell(layer.blendMode(), base, overlay, gain);

            running.leftMagnitudeDb[outputCell] = blended.leftMagnitudeDb;
            running.rightMagnitudeDb[outputCell] = blended.rightMagnitudeDb;
            running.sharedPhaseRadians[outputCell] = blended.phaseRadians;
        });
}

/// @brief Builds a `sound_mind::gpu::AmplitudePhaseSignal` sized/aligned
/// exactly like `running` (`config.binCount` x `canvasWidth`) from
/// `layer`'s own content, applying the same placement geometry
/// `mixLayerInto()`'s own CPU loop uses (`sourceColumnFor()`) - satisfies
/// `ComputeDevice::mixAmplitudePhaseSignal()`'s own precondition that its
/// `layer` argument already be placed onto `running`'s own shape. Cells
/// outside the layer's own placed range, or beyond its own bin range, are
/// filled with `silenceFloorDb` (rather than left untouched, the way the
/// CPU loop's own `continue` does) - the closest equivalent this whole-
/// array GPU kernel has to "nothing to add here", since (unlike the CPU
/// loop) it has no way to skip a cell individually.
///
/// Opacity is deliberately *not* baked in here - `mixLayerIntoGpuOrCpu()`
/// passes it separately as `mixAmplitudePhaseSignal()`'s own `layerGain`,
/// matching the CPU path's own separation of placement from gain.
/// `balance()`'s own per-channel gain, by contrast, *is* baked in here
/// (added as a plain per-cell dB offset, same as `mixLayerInto()`'s own
/// CPU-path `overlay` construction) - `v0.Y.46.1` Installment C (Per-layer
/// balance), confirmed as the simpler choice over threading a second
/// gain pair through the GPU kernel itself, since balance isn't
/// MindWave-bindable (unlike opacity) and needs no per-cell evaluation
/// beyond this one placement pass.
sound_mind::gpu::AmplitudePhaseSignal placeLayerForGpuMix(const Layer& layer,
                                                           const sound_mind::codec::StreamCodecConfig& config,
                                                           std::uint32_t canvasWidth, float silenceFloorDb) {
    const StreamImage& content = *layer.content();
    const BalanceGainsDb balanceGainsDb = balanceGainsDbFor(layer.balance());
    sound_mind::gpu::AmplitudePhaseSignal placed;
    const std::size_t cellCount = std::size_t{config.binCount} * canvasWidth;
    placed.leftMagnitudeDb.assign(cellCount, silenceFloorDb);
    placed.rightMagnitudeDb.assign(cellCount, silenceFloorDb);
    placed.phaseRadians.assign(cellCount, 0.0f);
    forEachPlacedCell(layer, config, canvasWidth,
                       [&](std::uint32_t /*bin*/, std::uint32_t /*outputColumn*/, std::size_t sourceCell,
                           std::size_t outputCell) {
                           placed.leftMagnitudeDb[outputCell] = content.leftMagnitudeDb[sourceCell] + balanceGainsDb.left;
                           placed.rightMagnitudeDb[outputCell] =
                               content.rightMagnitudeDb[sourceCell] + balanceGainsDb.right;
                           placed.phaseRadians[outputCell] = content.sharedPhaseRadians[sourceCell];
                       });
    return placed;
}

/// @brief `1.0` for every cell `placeLayerForGpuMix()` actually placed real
/// content into, `0.0` everywhere else - v0.1.6.2's own fix for a real
/// GPU-vs-CPU mismatch the new non-Normal blend-mode kernel work turned up
/// (caught by this milestone's own new compositor GPU-agreement test,
/// `v0.Y.60.1` Installment B): `placeLayerForGpuMix()`'s own "fill unplaced
/// cells with `silenceFloorDb`" shortcut was already an approximation for
/// `Normal` (negligible - adding a ~-96dB linear contribution is
/// imperceptibly different from adding nothing), but every other blend
/// mode's own formula operates in normalized `[0, 1]` unit space with an
/// explicit multiplicative/crossfade step, where a fake near-silent
/// `overlay` value is *not* a no-op (`Multiply(base, ~0)` pulls the result
/// toward black, not toward `base` unchanged). Multiplying this mask into
/// `mindWaveField` before the GPU call (rather than threading a fourth
/// buffer through the shader's own root signature) forces `cellGain` to
/// exactly `0` at every unplaced cell - which every mode's own formula
/// already reduces to "leave `base` unchanged" for, the same semantics the
/// CPU path's own `onUnplaced` no-op already has.
std::vector<float> buildPlacedMask(const Layer& layer, const sound_mind::codec::StreamCodecConfig& config,
                                     std::uint32_t canvasWidth) {
    std::vector<float> mask(std::size_t{config.binCount} * canvasWidth, 0.0f);
    forEachPlacedCell(
        layer, config, canvasWidth,
        [&](std::uint32_t /*bin*/, std::uint32_t /*outputColumn*/, std::size_t /*sourceCell*/, std::size_t outputCell) {
            mask[outputCell] = 1.0f;
        });
    return mask;
}

/// @brief `mixLayerInto()` above, preferring the GPU when available -
/// `compositeProject()`'s own general-path dispatch point, confirmed with
/// the user ahead of implementation. Falls back to the CPU
/// implementation above whenever `sound_mind::core::detail::
/// gpuComputeDeviceOrNull()` returns `nullptr` (no device at all, or
/// `setGpuComputeForcedOffForTesting(true)` is in effect), or if the GPU
/// call itself throws - a device lost mid-session (driver reset/removal)
/// is treated as a transient failure to degrade past, not a fatal error,
/// also confirmed with the user.
///
/// As of `v0.Y.37.1` (Deferred Blend Modes) through `v0.1.6.1`, the GPU
/// was only ever attempted for `BlendMode::Normal` - `ComputeDevice::
/// mixAmplitudePhaseSignal()`'s own HLSL kernel only ever implemented
/// Normal's own linear-amplitude-sum formula; every other blend mode took
/// the CPU path below, matching this codebase's established "CPU first,
/// GPU deferred" precedent for new filter/blend math (confirmed with the
/// user as that milestone's own scope). **As of `v0.1.6.2`** (the
/// benchmark suite's own finding that this left every non-Normal blend
/// mode un-accelerated - `docs/sound-mind-roadmap.md`'s `v0.Y.60.1`
/// Installment B), the kernel ports every mode's own formula from
/// `applyBlendedCell()`/`blend_mode_application.cpp` and GPU dispatch is
/// attempted for every `BlendMode` value, selected by
/// `mixAmplitudePhaseSignal()`'s own new `blendMode` parameter
/// (`static_cast<int>(layer.blendMode())`, using that enum's own ordinal
/// values - see that method's own docs).
///
/// @param opacityMindWave `layer`'s own bound opacity MindWave, or
///        `nullptr` if unbound - forwarded to `mixLayerInto()`'s own
///        per-cell evaluation on the CPU path, or built into a full
///        `buildMindWaveField()` array for `ComputeDevice::
///        mixAmplitudePhaseSignal()`'s own new per-cell field parameter on
///        the GPU path (confirmed with the user: a new field buffer
///        alongside the existing scalar gain, not a replacement for it -
///        `v0.Y.31.1` Installment C1's own answer to designing this
///        evaluation GPU-aware from the start). **As of `v0.1.6.2`**, this
///        field also gets `buildPlacedMask()`'s own mask multiplied into
///        it before the GPU call - see that function's own docs for why
///        every blend mode but `Normal` needed it.
void mixLayerIntoGpuOrCpu(StreamImage& running, const Layer& layer, const sound_mind::codec::StreamCodecConfig& config,
                           std::uint32_t canvasWidth, float silenceFloorDb, const MindWave* opacityMindWave) {
    if (auto* device = detail::gpuComputeDeviceOrNull()) {
        try {
            sound_mind::gpu::AmplitudePhaseSignal runningSignal;
            runningSignal.leftMagnitudeDb = running.leftMagnitudeDb;
            runningSignal.rightMagnitudeDb = running.rightMagnitudeDb;
            runningSignal.phaseRadians = running.sharedPhaseRadians;
            const auto layerSignal = placeLayerForGpuMix(layer, config, canvasWidth, silenceFloorDb);
            auto mindWaveField = buildMindWaveField(opacityMindWave, config, canvasWidth);
            const auto placedMask = buildPlacedMask(layer, config, canvasWidth);
            for (std::size_t i = 0; i < mindWaveField.size(); ++i) {
                mindWaveField[i] *= placedMask[i];
            }
            const auto mixed = device->mixAmplitudePhaseSignal(runningSignal, layerSignal, layer.opacity(),
                                                                 mindWaveField, static_cast<int>(layer.blendMode()));
            running.leftMagnitudeDb = mixed.leftMagnitudeDb;
            running.rightMagnitudeDb = mixed.rightMagnitudeDb;
            running.sharedPhaseRadians = mixed.phaseRadians;
            return;
        } catch (const std::exception&) {
            // Fall through to the CPU path below.
        }
    }
    mixLayerInto(running, layer, config, canvasWidth, opacityMindWave);
}

/// @brief `compositeProject()`'s own single-layer fast path: summing a
/// single term is the identity, so this produces exactly the same result
/// the general (`mixLayerIntoGpuOrCpu()`-based) path would - but skips
/// every transcendental call (sin/cos/abs/log10) it needs to actually sum
/// *multiple* complex values, replacing them with one gain shift
/// (computed once, not per cell) plus a placement copy. Worth its own
/// dedicated path specifically because it's the overwhelmingly common
/// case (most projects, and most moments even within a genuinely multi-
/// layer one, have exactly one contributing layer at any given cell) and
/// `compositeProject()` runs on every canvas repaint. Only called when
/// there's exactly one contributing layer and zero Filter layers in the
/// project at all - see `compositeProject()`'s own call site; even a
/// single Filter layer above this one contributor still needs the
/// general (sequential-fold) path, to actually apply it.
StreamImage compositeSingleLayer(const Layer& layer, const sound_mind::codec::StreamCodecConfig& config,
                                  std::uint32_t canvasWidth, float silenceFloorDb) {
    StreamImage result;
    result.config = config;
    result.frameCount = canvasWidth;
    result.sampleCount = static_cast<std::uint64_t>(canvasWidth) * config.hopLength;
    const std::size_t cellCount = std::size_t{config.binCount} * canvasWidth;
    result.leftMagnitudeDb.resize(cellCount);
    result.rightMagnitudeDb.resize(cellCount);
    result.sharedPhaseRadians.resize(cellCount);

    const StreamImage& content = *layer.content();
    const float gainDb = 20.0f * std::log10(std::max(layer.opacity(), kMinLinearAmplitude));
    const BalanceGainsDb balanceGainsDb = balanceGainsDbFor(layer.balance());
    forEachPlacedCell(
        layer, config, canvasWidth,
        [&](std::uint32_t /*bin*/, std::uint32_t /*outputColumn*/, std::size_t sourceCell, std::size_t outputCell) {
            // dB(linear * gain) == dB(linear) + dB(gain) - the same
            // identity dbToLinearAmplitude()/linearAmplitudeToDb() round-
            // trips exactly for any value clear of the silence floor,
            // letting opacity/balance apply as a plain addition instead of
            // a full linear round-trip.
            result.leftMagnitudeDb[outputCell] = content.leftMagnitudeDb[sourceCell] + gainDb + balanceGainsDb.left;
            result.rightMagnitudeDb[outputCell] =
                content.rightMagnitudeDb[sourceCell] + gainDb + balanceGainsDb.right;
            result.sharedPhaseRadians[outputCell] = content.sharedPhaseRadians[sourceCell];
        },
        [&](std::uint32_t /*bin*/, std::uint32_t /*outputColumn*/, std::size_t outputCell) {
            // Out of the layer's own bin range, or outside its own placed
            // column range - nothing to place there, same as a column
            // outside the layer's own placed range.
            result.leftMagnitudeDb[outputCell] = silenceFloorDb;
            result.rightMagnitudeDb[outputCell] = silenceFloorDb;
            result.sharedPhaseRadians[outputCell] = 0.0f;
        });
    return result;
}

}  // namespace

std::optional<sound_mind::codec::RgbImage> renderLayer(const Layer& layer, std::uint32_t canvasWidth) {
    if (!layer.content().has_value()) {
        return std::nullopt;
    }
    const RgbImage base = sound_mind::codec::toRgbImage(*layer.content());

    const std::uint32_t rescaledWidth = rescaledWidthFor(base.width, layer.rescaleFactor());
    const RgbImage rescaled = resampleHorizontally(base, rescaledWidth);

    return placeOnCanvas(rescaled, layer.translationColumns(), canvasWidth);
}

std::optional<sound_mind::codec::RgbImage> renderLayerThumbnail(const Layer& layer, std::uint32_t width,
                                                                  std::uint32_t height) {
    if (!layer.content().has_value()) {
        return std::nullopt;
    }
    const RgbImage base = sound_mind::codec::toRgbImage(*layer.content());
    return sound_mind::codec::downsampleAveraged(base, width, height);
}

std::optional<sound_mind::codec::RgbImage> renderLayerAmplitudeSummary(const Layer& layer, std::uint32_t width,
                                                                        std::uint32_t height) {
    if (!layer.content().has_value()) {
        return std::nullopt;
    }
    const StreamImage& content = *layer.content();
    if (content.frameCount == 0 || content.config.binCount == 0) {
        return std::nullopt;
    }

    // The same per-column loudness computation the Layers Panel's own
    // per-layer loudness indicator uses (v0.Y.52.1, Analysis Tools v1) -
    // shared here rather than this function's own now-removed independent
    // copy of the identical per-column averaging math.
    const std::vector<float> profile = computeLoudnessProfile(content);

    StreamImage summary;
    summary.config = content.config;
    summary.config.binCount = 1;
    summary.frameCount = content.frameCount;
    summary.leftMagnitudeDb = profile;
    summary.rightMagnitudeDb = profile;
    summary.sharedPhaseRadians.assign(content.frameCount, 0.0f);

    const RgbImage base = sound_mind::codec::toRgbImage(summary);
    return sound_mind::codec::downsampleAveraged(base, width, height);
}

std::optional<float> loudnessAtProjectColumn(const std::vector<float>& profile, double playheadFraction,
                                              std::uint32_t canvasWidth, double rescaleFactor,
                                              std::int64_t translationColumns) {
    if (profile.empty() || canvasWidth == 0) {
        return std::nullopt;
    }
    const double clampedFraction = std::clamp(playheadFraction, 0.0, 1.0);
    const auto outputColumn = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(canvasWidth - 1, static_cast<std::uint64_t>(clampedFraction * canvasWidth)));
    const auto sourceColumn =
        sourceColumnFor(outputColumn, static_cast<std::uint32_t>(profile.size()), rescaleFactor, translationColumns);
    if (!sourceColumn.has_value()) {
        return std::nullopt;
    }
    return profile[*sourceColumn];
}

std::optional<StreamImage> compositeProject(const Project& project, const std::function<bool()>& shouldCancel,
                                             bool respectMute) {
    const auto& layers = project.layers();

    // Pre-pass: which Normal/Background layers actually contribute their
    // own content, the composite's own output binCount (the tallest among
    // them - see the loop below for why), and whether any Filter-type
    // layer exists at all (Filter/Equalizer layers never have their own
    // content() - see Layer's own docs - so they never appear in
    // normalContributors, but their mere presence forces the general path
    // below instead of the single-layer fast path).
    std::uint32_t binCount = 0;
    std::vector<const Layer*> normalContributors;
    bool anyFilterLayer = false;
    for (const Layer& layer : layers) {
        if (!layer.visible() || (respectMute && layer.muted())) {
            continue;
        }
        if (isFilterLayerType(layer.type())) {
            anyFilterLayer = true;
            continue;
        }
        if (layer.content().has_value()) {
            normalContributors.push_back(&layer);
            // The project's own settings.binCount is authoritative for a
            // project whose layers were actually encoded from it (the
            // normal case - every real layer's own content already comes
            // from streamCodecConfigFor() against these same settings, so
            // the two numbers are identical in practice). But nothing
            // enforces that in general, and it's routine for a hand-built
            // StreamImage (an in-memory test fixture, in particular) to
            // declare its own, different binCount - using every
            // contributing layer's own tallest bin range instead keeps
            // the composite matching whatever its own real content
            // actually is, rather than silently padding it to (or
            // truncating it against) a project-level number nothing here
            // actually guarantees matches.
            binCount = std::max(binCount, layer.content()->config.binCount);
        }
    }
    if (normalContributors.empty()) {
        // Nothing to composite at all - a Filter layer with nothing
        // beneath it (or beneath everything hidden/contentless) has
        // nothing to filter either, per docs/sound-mind-design.md's
        // "Filter Layer" ("composits the layers beneath it").
        return std::nullopt;
    }

    const auto& settings = project.settings();
    auto config = streamCodecConfigFor(settings);
    if (binCount > 0) {
        config.binCount = binCount;
    }
    const std::uint32_t canvasWidth = settings.canvasWidth;
    const float silenceFloorDb = linearAmplitudeToDb(0.0f);

    if (!anyFilterLayer && normalContributors.size() == 1 &&
        !resolveOpacityMindWave(*normalContributors.front(), project) &&
        normalContributors.front()->blendMode() == BlendMode::Normal) {
        // Fast path - see compositeSingleLayer()'s own docs for why this
        // is worth a dedicated path, and what it's specifically reachable
        // for. Excluded once the sole contributor's own opacity is
        // MindWave-bound (`v0.Y.31.1` Installment C1) - the fast path's
        // whole premise is a single *scalar* gain shift, computed once,
        // not per cell; a bound layer needs the general path's own
        // per-cell evaluation instead. Excluded, as of `v0.Y.37.1`
        // (Deferred Blend Modes), for any blend mode other than `Normal`
        // too - compositeSingleLayer()'s own "single-layer reduces to
        // itself" shortcut is a `Normal`-specific algebraic identity
        // (summing one term onto silence); every other mode's own actual
        // identity behavior against a silent base is different (e.g.
        // `Multiply` against silence is silence, not the layer's own raw
        // content) and needs the general path's own real
        // `applyBlendedCell()` call instead.
        return compositeSingleLayer(*normalContributors.front(), config, canvasWidth, silenceFloorDb);
    }

    // General path: a sequential fold over the whole stack, bottom to
    // top - see docs/sound-mind-design.md's "Filter Layer" ("composits
    // the layers beneath it... applies a filter... and renders the
    // result"). Normal/Background layers mix into the running composite
    // (result), initialized to silence; a Filter-type layer transforms
    // the running composite in place instead. This alone implements "each
    // Filter layer only composites down to the next-lower Filter layer"
    // (docs/sound-mind-architecture.md's own Decision recording why no
    // separate segment bookkeeping is needed): by the time a Filter layer
    // is reached, `result` already *is* exactly "everything beneath it
    // since the last Filter layer" - an earlier (lower) Filter layer
    // already collapsed whatever was below *it* into one transformed
    // contribution, so this one only ever sees what came after that.
    StreamImage result;
    result.config = config;
    result.frameCount = canvasWidth;
    result.sampleCount = static_cast<std::uint64_t>(canvasWidth) * config.hopLength;
    const std::size_t cellCount = std::size_t{config.binCount} * canvasWidth;
    result.leftMagnitudeDb.assign(cellCount, silenceFloorDb);
    result.rightMagnitudeDb.assign(cellCount, silenceFloorDb);
    result.sharedPhaseRadians.assign(cellCount, 0.0f);
    bool anyMixedIn = false;
    for (const Layer& layer : layers) {
        if (shouldCancel && shouldCancel()) {
            throw CompositeCancelled{};
        }
        if (!layer.visible() || (respectMute && layer.muted())) {
            continue;
        }
        if (isFilterLayerType(layer.type())) {
            if (anyMixedIn) {
                result = applyFilterRespectingOpacity(std::move(result), layer, project, settings, config,
                                                        canvasWidth);
            }
            continue;
        }
        if (!layer.content().has_value()) {
            continue;
        }
        mixLayerIntoGpuOrCpu(result, layer, config, canvasWidth, silenceFloorDb,
                             resolveOpacityMindWave(layer, project));
        anyMixedIn = true;
    }

    return result;
}

void CompositePrefixCache::invalidateFrom(std::size_t layerIndex) noexcept {
    for (std::size_t i = layerIndex; i < prefixes_.size(); ++i) {
        prefixes_[i].reset();
    }
}

void CompositePrefixCache::invalidateAll() noexcept {
    prefixes_.clear();
    lastLayerOrder_.clear();
}

namespace {

/// @brief The byte budget `compositeProjectCached()` won't knowingly
/// cache past - see `CompositePrefixCache`'s own docs on why exceeding
/// it just means "skip caching this call", never a correctness problem.
///
/// **Corrected from an initial `512 MiB`, found too tight by this
/// feature's own first benchmark run**: at a "medium" benchmark canvas
/// (512 bins x 2000 frames, ~11.7 MiB per cached layer), `512 MiB` runs
/// out at ~44 layers - but `Project::createNew()` always carries a
/// Background and an Equalizer layer in addition to whatever a caller
/// adds, so a benchmark/user scenario asking for "50 layers" is actually
/// 52 `Project::layers()` entries, just over that line - silently
/// disabling caching for exactly the headline "many layers" case this
/// feature exists for (confirmed: the benchmark's own `layerCount=50`
/// cached-edit case measured no speedup at all over the uncached path
/// before this fix). `4 GiB` instead fits ~350 layers at this same medium
/// canvas, and ~44 layers even at the benchmark's own "large" canvas
/// size (1024 bins x 8000 frames, ~93.75 MiB/layer) - a generous, but
/// still bounded, fraction of a modern desktop's RAM; past either limit,
/// this still only ever degrades to "not cached," never incorrect.
constexpr std::size_t kCompositeCacheMaxBytes = 4ull * 1024 * 1024 * 1024;

}  // namespace

std::optional<StreamImage> compositeProjectCached(const Project& project, CompositePrefixCache& cache,
                                                     const std::function<bool()>& shouldCancel, bool respectMute) {
    const auto& layers = project.layers();

    // Pre-pass - see compositeProject()'s own identical reasoning for
    // binCount. anyFilterLayer/single-contributor-fast-path don't matter
    // here - see compositeProjectCached()'s own docs on why this always
    // uses the general fold.
    std::uint32_t binCount = 0;
    bool anyContributor = false;
    for (const Layer& layer : layers) {
        if (!layer.visible() || (respectMute && layer.muted())) {
            continue;
        }
        if (isFilterLayerType(layer.type())) {
            continue;
        }
        if (layer.content().has_value()) {
            anyContributor = true;
            binCount = std::max(binCount, layer.content()->config.binCount);
        }
    }
    if (!anyContributor) {
        cache.invalidateAll();
        return std::nullopt;
    }

    const auto& settings = project.settings();
    auto config = streamCodecConfigFor(settings);
    if (binCount > 0) {
        config.binCount = binCount;
    }
    const std::uint32_t canvasWidth = settings.canvasWidth;
    const float silenceFloorDb = linearAmplitudeToDb(0.0f);

    // Structural signature check - see CompositePrefixCache's own docs
    // on why a reorder/add/remove/respectMute/output-shape change needs
    // no explicit invalidateAll() call from the caller: any of them
    // changes one of these four values, caught here instead.
    std::vector<LayerId> currentOrder;
    currentOrder.reserve(layers.size());
    for (const Layer& layer : layers) {
        currentOrder.push_back(layer.id());
    }
    const bool structureChanged = currentOrder != cache.lastLayerOrder_ || respectMute != cache.lastRespectMute_ ||
                                    config.binCount != cache.lastBinCount_ || canvasWidth != cache.lastCanvasWidth_;
    if (structureChanged) {
        cache.prefixes_.assign(layers.size(), std::nullopt);
        cache.lastLayerOrder_ = currentOrder;
        cache.lastRespectMute_ = respectMute;
        cache.lastBinCount_ = config.binCount;
        cache.lastCanvasWidth_ = canvasWidth;
    } else if (cache.prefixes_.size() != layers.size()) {
        // Defensive only - the order-vector comparison above should
        // already have caught any size change (a different layer count
        // always means a different id list too); kept in case that
        // invariant is ever violated by a future change here.
        cache.prefixes_.assign(layers.size(), std::nullopt);
    }

    // Memory budget - see CompositePrefixCache's own docs. Skips caching
    // entirely for this call rather than partially (storing only the
    // first few layers' own prefixes and not the rest would make
    // "is this cached" depend on index, a correctness trap no simpler
    // than just not caching at all this call).
    const std::size_t cellCount = std::size_t{config.binCount} * canvasWidth;
    const std::size_t bytesPerImage = cellCount * 3 * sizeof(float);
    const bool cachingAffordable = layers.empty() || bytesPerImage == 0 ||
                                     (kCompositeCacheMaxBytes / bytesPerImage) >= layers.size();
    if (!cachingAffordable) {
        cache.prefixes_.assign(layers.size(), std::nullopt);
    }

    // Resume from the highest still-valid cached prefix, if any.
    std::size_t resumeIndex = 0;
    StreamImage result;
    bool anyMixedIn = false;
    bool haveResumePoint = false;
    for (std::size_t i = cache.prefixes_.size(); i-- > 0;) {
        if (cache.prefixes_[i].has_value()) {
            result = cache.prefixes_[i]->image;
            anyMixedIn = cache.prefixes_[i]->anyMixedIn;
            resumeIndex = i + 1;
            haveResumePoint = true;
            break;
        }
    }
    if (!haveResumePoint) {
        result.config = config;
        result.frameCount = canvasWidth;
        result.sampleCount = static_cast<std::uint64_t>(canvasWidth) * config.hopLength;
        result.leftMagnitudeDb.assign(cellCount, silenceFloorDb);
        result.rightMagnitudeDb.assign(cellCount, silenceFloorDb);
        result.sharedPhaseRadians.assign(cellCount, 0.0f);
    }

    // A DynamicSpeckle Filter layer's own fresh-every-call randomness
    // (see CompositePrefixCache's own docs) means nothing at or above
    // its own stack position may ever be cached. haveResumePoint can
    // only be true here at an index strictly below any DynamicSpeckle
    // layer's own position in the first place - this function never
    // stores a prefix at or past one (see the loop below), so one could
    // never have been found above.
    bool seenDynamicSpeckle = false;

    for (std::size_t i = resumeIndex; i < layers.size(); ++i) {
        const Layer& layer = layers[i];
        if (shouldCancel && shouldCancel()) {
            throw CompositeCancelled{};
        }
        if (layer.visible() && !(respectMute && layer.muted())) {
            if (isFilterLayerType(layer.type())) {
                if (layer.filterConfiguration().type() == FilterType::DynamicSpeckle) {
                    seenDynamicSpeckle = true;
                }
                if (anyMixedIn) {
                    result = applyFilterRespectingOpacity(std::move(result), layer, project, settings, config,
                                                            canvasWidth);
                }
            } else if (layer.content().has_value()) {
                mixLayerIntoGpuOrCpu(result, layer, config, canvasWidth, silenceFloorDb,
                                      resolveOpacityMindWave(layer, project));
                anyMixedIn = true;
            }
        }

        if (!seenDynamicSpeckle) {
            cache.prefixes_[i] = CompositePrefixCache::Entry{result, anyMixedIn};
        }
    }

    return result;
}

}  // namespace sound_mind::core
