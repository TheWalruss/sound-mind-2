#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/path.h"

namespace sound_mind::core {

// Forward-declared, not included: paint_application.h's own chain
// (paint_operation.h -> tool_configuration.h -> mind_shot.h/mind_grain.h)
// leads right back to paste_operation.h, which needs SelectionRegion
// itself - a real #include cycle. combine()'s own declaration only needs
// FrameBinRange's *name* here; its definition (needed to actually define
// combine(), in selection_region.cpp, and to call it, wherever a caller
// already has a real FrameBinRange on hand) lives entirely outside this
// header.
struct FrameBinRange;

/// @brief Which shape `SelectionRegion` currently holds - see the class's
///        own docs.
enum class SelectionRegionKind {
    Path,  ///< A closed curve - a Lasso selection (`v0.Y.35.1` Installment A).
    Mask,  ///< A raster bitmap - a Wand or boolean-combined selection (Installment B).
};

/**
 * @brief A non-rectangular selection's own precise shape, narrowing which
 *        cells within a `FillOperation`/`PasteOperation`'s plain bounding-
 *        box `bounds()` actually get touched - see
 *        `docs/sound-mind-design.md`'s "Selection".
 *
 * Two genuinely different shapes need two genuinely different
 * representations, confirmed with the user rather than forcing one onto
 * the other:
 *
 * - **A Lasso's own boundary is always a single closed curve** (drawn as
 *   one continuous freehand drag) - represented as a plain
 *   `sound_mind::core::Path`, reused wholesale from `v0.Y.35.1`
 *   Installment A (see `containsPoint()`'s own docs for how membership is
 *   tested against it). Resolution-independent - re-tested fresh in
 *   continuous time/frequency space against whatever grid is current.
 * - **A Wand's flood-filled blob (`v0.Y.35.1` Installment B) - and any
 *   boolean-combined result - generally can't be**: a flood fill can be
 *   disconnected or hole-riddled, and combining two selections (Add/
 *   Subtract/Intersect) can produce a shape no single polygon captures.
 *   Represented instead as a raster bitmap mask over an explicit
 *   `[frameLow, frameHigh] x [binLow, binHigh]` absolute cell range (the
 *   same row-major, bin-major indexing `cellIndex()` already establishes)
 *   - the flood fill's own natural output (a BFS/DFS over grid cells
 *     already works cell-by-cell), and boolean combination is then a
 *     plain per-cell AND/OR/AND-NOT once both operands are rasterized to
 *     a common grid.
 *
 * **A mask is frozen to the discretization (`StreamCodecConfig`/frame
 * count) it was captured against** - the same accepted-risk precedent
 * `Clip`'s own pixel data already establishes ("captured from, and pasted
 * back into, a layer using the current project's own config"). A cell
 * outside the mask's own captured range is simply not selected, the same
 * "falls outside range" convention `applyPasteOperation()`'s own
 * out-of-range clipping already uses - not a new fragility.
 *
 * `bounds()` (the operation's own plain bounding-box `TimeFrequencyRect`)
 * is deliberately *not* derived from a `SelectionRegion` - it's computed
 * once, independently, at commit time (`Path::bounds()` for a Lasso; the
 * mask's own frame/bin range converted back to time/frequency for a Wand/
 * combined selection) and stored directly on the owning `FillOperation`/
 * `PasteOperation`, so every existing bounding-box consumer (Pick hit-
 * testing, Show Bounding Boxes, the Mind Grain guardrails) keeps working
 * completely unchanged regardless of which kind this class holds.
 */
class SelectionRegion {
public:
    /// @brief The three ways two selections can combine - see
    ///        `combine()`'s own docs.
    enum class BooleanOp {
        Add,        ///< Union - either operand's own selected cells.
        Subtract,   ///< The first operand's own cells, minus the second's.
        Intersect,  ///< Only cells both operands select.
    };

    /// @brief Default-constructs an empty (no nodes) Path-kind region -
    ///        only reachable through JSON deserialization
    ///        (`from_json()` immediately overwrites every field), the
    ///        same "degenerate but valid" convention `Path{}`/
    ///        `TimeFrequencyRect{}` already establish.
    SelectionRegion() = default;

    /// @brief Constructs a Lasso-shaped region from its own boundary curve.
    /// @param boundary The closed curve - see the class's own docs on how
    ///        membership is tested against it.
    explicit SelectionRegion(Path boundary) noexcept : kind_(SelectionRegionKind::Path), path_(std::move(boundary)) {}

    /**
     * @brief Constructs a raster-mask region.
     * @param frameLow The lowest (inclusive) absolute frame/column index
     *        this mask covers.
     * @param frameHigh The highest (inclusive) absolute frame/column
     *        index.
     * @param binLow The lowest (inclusive) absolute bin/row index.
     * @param binHigh The highest (inclusive) absolute bin/row index.
     * @param mask The cell membership, row-major/bin-major (`cellIndex()`'s
     *        own convention, offset by `binLow`/`frameLow`) - must be
     *        exactly `(frameHigh - frameLow + 1) * (binHigh - binLow + 1)`
     *        long; a shorter vector makes every index past its own end
     *        read as "not selected" (see `containsCell()`'s own docs)
     *        rather than undefined behavior.
     */
    SelectionRegion(int frameLow, int frameHigh, int binLow, int binHigh, std::vector<bool> mask)
        : kind_(SelectionRegionKind::Mask),
          frameLow_(frameLow),
          frameHigh_(frameHigh),
          binLow_(binLow),
          binHigh_(binHigh),
          mask_(std::move(mask)) {}

    /// @brief Which shape this region currently holds.
    /// @return `kind_`, as given at construction.
    [[nodiscard]] SelectionRegionKind kind() const noexcept { return kind_; }

    /// @brief This region's own boundary curve.
    /// @return The curve given at construction - only meaningful when
    ///         `kind() == SelectionRegionKind::Path`; a default-
    ///         constructed (empty) `Path` otherwise.
    [[nodiscard]] const Path& path() const noexcept { return path_; }

    /// @brief The mask's own lowest captured frame index - only
    ///        meaningful when `kind() == SelectionRegionKind::Mask`.
    /// @return `frameLow_`, as given at construction.
    [[nodiscard]] int maskFrameLow() const noexcept { return frameLow_; }
    /// @brief The mask's own highest captured frame index - only
    ///        meaningful when `kind() == SelectionRegionKind::Mask`.
    /// @return `frameHigh_`, as given at construction.
    [[nodiscard]] int maskFrameHigh() const noexcept { return frameHigh_; }
    /// @brief The mask's own lowest captured bin index - only meaningful
    ///        when `kind() == SelectionRegionKind::Mask`.
    /// @return `binLow_`, as given at construction.
    [[nodiscard]] int maskBinLow() const noexcept { return binLow_; }
    /// @brief The mask's own highest captured bin index - only
    ///        meaningful when `kind() == SelectionRegionKind::Mask`.
    /// @return `binHigh_`, as given at construction.
    [[nodiscard]] int maskBinHigh() const noexcept { return binHigh_; }
    /// @brief The mask's own raw per-cell membership - only meaningful
    ///        when `kind() == SelectionRegionKind::Mask`. Exposed for
    ///        JSON (de)serialization and tests; `containsCell()` is the
    ///        normal way to query membership.
    /// @return `mask_`, as given at construction.
    [[nodiscard]] const std::vector<bool>& maskCells() const noexcept { return mask_; }

    /**
     * @brief Whether a specific absolute `(bin, frame)` cell falls within
     *        this region - the direct, natural test for a mask; for a
     *        Path, converts the cell to a `TimeFrequencyPoint` first (via
     *        `frameIndexToTime()`/`binIndexToFrequency()`) and calls
     *        `containsPoint()`.
     * @param bin The cell's own absolute bin/row index.
     * @param frame The cell's own absolute frame/column index.
     * @param config Interprets `bin`/`frame` against a real time/
     *        frequency position (Path kind only - unused for Mask, whose
     *        own indices are already absolute).
     * @return `true` if the cell is selected; for a Mask, `false` for any
     *         cell outside `[maskFrameLow(), maskFrameHigh()] x
     *         [maskBinLow(), maskBinHigh()]`.
     */
    [[nodiscard]] bool containsCell(int bin, int frame, const sound_mind::codec::StreamCodecConfig& config) const;

    /// @brief Whether a continuous time/frequency point falls within this
    ///        region - for a Mask, rounds to the nearest cell first (via
    ///        `timeToFrameIndex()`/`frequencyToBinIndex()`) and calls
    ///        `containsCell()`; for a Path, calls `containsPoint()`
    ///        directly (no rounding).
    /// @param point The point to test.
    /// @param config Interprets `point` against a real cell grid (Mask
    ///        kind only).
    /// @return `true` if `point` falls within this region.
    [[nodiscard]] bool contains(TimeFrequencyPoint point, const sound_mind::codec::StreamCodecConfig& config) const;

    /// @brief A copy of this region, shifted by a fixed offset - a Path
    ///        shifts via `Path::translated()` (exact, continuous); a mask
    ///        shifts by rounding `deltaTimeSeconds`/`deltaFrequencyBins`
    ///        to the nearest whole frame/bin and moving its own captured
    ///        range accordingly (the mask's own cell contents are
    ///        unchanged, just relocated) - moving a Picked Wand/combined-
    ///        shaped fill or paste is exactly this.
    /// @param deltaTimeSeconds How far to shift along the timeline.
    /// @param deltaFrequencyBins How far to shift along the frequency
    ///        axis, in bins - not Hz (same reasoning `Path::translated()`'s
    ///        own docs give).
    /// @param config Interprets the shift against a real cell grid (Mask
    ///        kind only).
    /// @return The translated region.
    [[nodiscard]] SelectionRegion translated(double deltaTimeSeconds, double deltaFrequencyBins,
                                               const sound_mind::codec::StreamCodecConfig& config) const;

    /**
     * @brief Combines two selections (either of which may be absent,
     *        meaning "a plain rectangle over its own given range") into a
     *        single new mask-based region - the actual work behind Add/
     *        Subtract/Intersect (`docs/sound-mind-design.md`'s "boolean
     *        combination").
     *
     * Always produces a `Mask`-kind result, even when both operands
     * happen to be simple rectangles or a single Lasso - a boolean
     * combination's own result generally isn't expressible as a single
     * closed curve, so there is no "stay Path-kind when possible" special
     * case to preserve resolution-independence for.
     *
     * @param a The first operand (the selection being added to/subtracted
     *        from/intersected against) - `std::nullopt` for a plain
     *        rectangle, in which case every cell within `aRange` counts
     *        as selected.
     * @param aRange `a`'s own absolute cell range to rasterize against
     *        when `a` is `std::nullopt` or `Path`-kind - ignored when `a`
     *        is already `Mask`-kind (its own internal range is used
     *        instead, so a caller-supplied range can never desync from
     *        what a mask actually covers).
     * @param b The second operand, same convention as `a`.
     * @param bRange `b`'s own range, same convention as `aRange`.
     * @param resultRange The combined result's own absolute cell range to
     *        rasterize *over* - `Add` needs the union of `aRange`/`bRange`;
     *        `Subtract`/`Intersect` never exceed `aRange` itself. Left to
     *        the caller (`SelectionController`, which already has
     *        `rangeFor()` on hand for this) rather than computed here, to
     *        keep this class working purely in absolute cell coordinates.
     * @param op Which combination to perform.
     * @param config Interprets a `Path` operand's own cells (unused for
     *        an operand that's already `Mask`-kind or absent).
     * @return The combined result, as a new `Mask`-kind region spanning
     *         exactly `resultRange`.
     */
    [[nodiscard]] static SelectionRegion combine(const std::optional<SelectionRegion>& a, FrameBinRange aRange,
                                                   const std::optional<SelectionRegion>& b, FrameBinRange bRange,
                                                   FrameBinRange resultRange, BooleanOp op,
                                                   const sound_mind::codec::StreamCodecConfig& config);

private:
    SelectionRegionKind kind_ = SelectionRegionKind::Path;
    Path path_;

    int frameLow_ = 0;
    int frameHigh_ = -1;
    int binLow_ = 0;
    int binHigh_ = -1;
    std::vector<bool> mask_;
};

/// @brief Serializes a region to its JSON representation - `{"kind":
///        "path", "path": ...}` or `{"kind": "mask", "frameLow": ...,
///        "frameHigh": ..., "binLow": ..., "binHigh": ..., "mask": [...]}`.
void to_json(nlohmann::json& json, const SelectionRegion& region);

/// @brief Parses a region from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required
///         data, or an unrecognized `"kind"`.
void from_json(const nlohmann::json& json, SelectionRegion& region);

}  // namespace sound_mind::core
