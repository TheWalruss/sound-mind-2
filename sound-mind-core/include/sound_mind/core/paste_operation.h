#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/operation.h"
#include "sound_mind/core/selection_region.h"

namespace sound_mind::core {

/**
 * @brief A captured rectangular snapshot of a `StreamImage`'s amplitude/
 *        phase planes - what Copy/Cut actually places on the clipboard, and
 *        what a `PasteOperation` writes back out.
 *
 * Deliberately a plain Core struct of raw arrays, not an embedded
 * `sound_mind::codec::StreamImage`: Codec is a deliberately JSON-free module
 * (see `docs/sound-mind-architecture.md`'s Build & Module Layout - confirmed
 * via `sound-mind-codec/CMakeLists.txt` carrying no `nlohmann_json`
 * dependency at all), and every other `Operation` subtype so far
 * (`PaintOperation`, `FillOperation`) only ever stores plain Core types for
 * exactly that reason. A `Clip` needs no `StreamCodecConfig` of its own - it
 * is always captured from, and pasted back into, a layer using the current
 * project's own config, the same assumption `applyFillOperation()` already
 * makes about `bounds()`.
 */
struct Clip {
    /// @brief Number of time-axis columns this clip spans.
    std::uint32_t frameCount = 0;

    /// @brief Number of frequency-axis rows this clip spans.
    std::uint32_t binCount = 0;

    /// @brief Left channel amplitude, in dB, row-major `[bin][frame]` -
    ///        exactly `binCount * frameCount` values.
    std::vector<float> leftMagnitudeDb;

    /// @brief Right channel amplitude, in dB, row-major `[bin][frame]` -
    ///        exactly `binCount * frameCount` values.
    std::vector<float> rightMagnitudeDb;

    /// @brief The shared phase channel, in radians, row-major
    ///        `[bin][frame]` - exactly `binCount * frameCount` values.
    std::vector<float> sharedPhaseRadians;
};

/// @brief Serializes a clip to its JSON representation.
void to_json(nlohmann::json& json, const Clip& clip);

/// @brief Parses a clip from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, Clip& clip);

/**
 * @brief Pastes a previously captured `Clip` onto a layer's content, logged
 *        non-destructively - see `docs/sound-mind-design.md`'s "Cut / Copy /
 *        Paste" and `docs/sound-mind-architecture.md`'s Core Data Model.
 *
 * The third concrete `Operation` subtype (after `PaintOperation` and
 * `FillOperation`). Deliberately independent of both the selection it was
 * copied from *and* of "the currently active layer" at construction time:
 * `targetLayer()` names whichever layer this specific paste writes into,
 * which may differ from the layer the clip was originally captured on - a
 * copy from one layer can be pasted onto a completely different one. Unlike
 * `FillOperation`/`PaintOperation`, applying a paste is a direct overwrite
 * of `bounds()`'s own cells from the clip's own pixel grid, not a gradient-
 * blended edit - see `applyPasteOperation()`'s own docs.
 *
 * **Non-rectangular pastes** (Lasso, `v0.Y.35.1` Installment A; Wand and
 * boolean-combined selections, Installment B): the same bounding-box-vs-
 * narrowing-shape split `FillOperation` establishes - `bounds()` is
 * always the placement's own bounding box; `boundary()`, when present,
 * additionally narrows which of the clip's own cells actually get
 * written, leaving the destination's own prior content untouched
 * everywhere else within that bounding box - see `applyPasteOperation()`'s
 * own docs. The clip itself (`clip()`) is always a plain rectangular
 * raster regardless - a non-rectangular Copy still captures its own full
 * bounding box's worth of pixels, exactly like a Rectangle Copy always
 * has; only where those pixels actually get *written back* on paste is
 * narrowed.
 */
class PasteOperation : public LayerContentOperation {
public:
    /**
     * @param id Identity to give this operation within its OperationLog.
     * @param targetLayer Which layer this paste writes into - independent
     *        of whichever layer `clip` was originally captured from.
     * @param placement Where on `targetLayer` the clip lands - its own
     *        time/frequency extent, an owned snapshot (same "owned, not
     *        shared" reasoning `FillOperation::bounds` already
     *        established). Always this paste's own bounding box, even
     *        when `boundary` narrows it to a non-rectangular shape.
     * @param clip The captured pixel data to paste - an owned copy.
     * @param supersedes The prior operation this one replaces, if any -
     *        see Operation::supersedes()'s own docs.
     * @param boundary The selection `clip` was originally copied through,
     *        in the same coordinate space as `placement` - narrowing which
     *        of `clip`'s own cells actually get written - see
     *        `boundary()`'s own docs. `std::nullopt` (the default) for a
     *        plain Rectangle-shaped paste, unchanged from every paste this
     *        class supported before `v0.Y.35.1`.
     */
    PasteOperation(OperationId id, LayerId targetLayer, TimeFrequencyRect placement, Clip clip,
                    std::optional<OperationId> supersedes = std::nullopt,
                    std::optional<SelectionRegion> boundary = std::nullopt) noexcept
        : LayerContentOperation(id, targetLayer, supersedes),
          placement_(placement),
          clip_(std::move(clip)),
          boundary_(std::move(boundary)) {}

    /// @brief This operation's own time/frequency footprint - exactly
    ///        where the clip was placed. Always the *bounding box*, even
    ///        for a Lasso-shaped paste - see boundary()'s own docs and
    ///        this class's own docs on why.
    /// @return This operation's own bounds.
    [[nodiscard]] TimeFrequencyRect bounds() const override { return placement_; }

    /// @brief The captured pixel data this operation pastes.
    /// @return This operation's own clip.
    [[nodiscard]] const Clip& clip() const noexcept { return clip_; }

    /**
     * @brief The selection this paste's own clip was originally copied
     *        through, if any - narrows `applyPasteOperation()`'s own
     *        write to cells `boundary()->containsCell()` actually places
     *        inside it, leaving the destination's own prior content at
     *        every other cell in `bounds()` untouched.
     * @return The boundary, or `std::nullopt` for a plain Rectangle-shaped
     *         paste (every cell in `clip()` is written, exactly as before
     *         this field existed).
     */
    [[nodiscard]] const std::optional<SelectionRegion>& boundary() const noexcept { return boundary_; }

    /// @copydoc Operation::translatedCopy()
    [[nodiscard]] std::unique_ptr<Operation> translatedCopy(
        OperationId newId, double deltaTimeSeconds, double deltaFrequencyBins,
        const sound_mind::codec::StreamCodecConfig& config) const override {
        std::optional<SelectionRegion> translatedBoundary;
        if (boundary_) {
            translatedBoundary = boundary_->translated(deltaTimeSeconds, deltaFrequencyBins, config);
        }
        return std::make_unique<PasteOperation>(
            newId, targetLayer_, translated(placement_, deltaTimeSeconds, deltaFrequencyBins, config), clip_, id(),
            std::move(translatedBoundary));
    }

private:
    TimeFrequencyRect placement_;
    Clip clip_;
    std::optional<SelectionRegion> boundary_;
};

}  // namespace sound_mind::core
