#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/gradient.h"
#include "sound_mind/core/operation.h"

namespace sound_mind::core {

/**
 * @brief A single point in a Path's own coordinate space: seconds along
 *        the project's timeline, Hz along its frequency axis - the same
 *        units `TimeFrequencyRect` already uses for `Operation::bounds()`,
 *        reused here rather than a second, pixel/column-based convention.
 */
struct TimeFrequencyPoint {
    /// @brief Position along the timeline, in seconds from the project's start.
    double timeSeconds = 0.0;
    /// @brief Position along the frequency axis, in Hz.
    double frequencyHz = 0.0;
};

/// @brief Serializes a point to its JSON representation.
void to_json(nlohmann::json& json, const TimeFrequencyPoint& point);

/// @brief Parses a point from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, TimeFrequencyPoint& point);

/**
 * @brief Which of the two node kinds `docs/sound-mind-design.md`'s
 *        "Placing and Editing" describes a `PathNode` is.
 */
enum class PathNodeType {
    Smooth,  ///< Carries a pair of Bézier handles; the path curves smoothly through it.
    Corner,  ///< No handles; the path meets at a sharp kink.
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(PathNodeType, {
    {PathNodeType::Smooth, "smooth"},
    {PathNodeType::Corner, "corner"},
})
// clang-format on

/**
 * @brief One node of a Path - see `docs/sound-mind-design.md`'s "Placing
 *        and Editing".
 *
 * A `Corner` node's `handleIn`/`handleOut` are always `std::nullopt`; a
 * `Smooth` node's are normally both present (even if collapsed onto
 * `anchor` itself, for a freshly placed node with no curve pulled out of
 * it yet) - nothing here enforces that pairing, since it's a Studio-level
 * editing invariant (per the design doc, converting a node's type is what
 * establishes or discards its handles), not a `PathNode`-level one.
 */
struct PathNode {
    /// @brief This node's own position.
    TimeFrequencyPoint anchor;

    /// @brief Which kind of node this is.
    PathNodeType type = PathNodeType::Corner;

    /// @brief The incoming Bézier handle (toward the previous node), if any.
    std::optional<TimeFrequencyPoint> handleIn;

    /// @brief The outgoing Bézier handle (toward the next node), if any.
    std::optional<TimeFrequencyPoint> handleOut;
};

/// @brief Serializes a node to its JSON representation.
void to_json(nlohmann::json& json, const PathNode& node);

/// @brief Parses a node from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, PathNode& node);

/**
 * @brief A segmented cubic Bézier curve that a brush stamps along its
 *        length - see `docs/sound-mind-design.md`'s "Paths".
 *
 * Every `PaintOperation` carries exactly one: a freehand stroke's raw
 * input is curve-fit into one automatically (see fitPathToPoints()), and
 * the Path tool builds one the same way but node-by-node, deliberately -
 * both end up exactly as editable as each other once painted (see the
 * design doc's "Pick").
 */
class Path {
public:
    /// @brief Constructs an empty path (no nodes) with a fresh, fully
    ///        transparent Gradient - a path becomes meaningful once nodes
    ///        are appended and/or the gradient is given real values.
    Path() = default;

    /// @brief This path's nodes, in order from start (t=0) to end (t=1).
    /// @return The nodes currently placed; may be empty.
    [[nodiscard]] const std::vector<PathNode>& nodes() const noexcept { return nodes_; }

    /**
     * @brief Appends a new node to the end of the path.
     * @param node The node to append.
     * @return The new node's index in nodes().
     */
    std::size_t addNode(PathNode node);

    /**
     * @brief Inserts a node at a specific position.
     * @param index Where to insert - nodes from this index onward shift
     *        up by one; `index == nodes().size()` appends, same as
     *        addNode().
     * @param node The node to insert.
     * @return `true` and inserts if `index <= nodes().size()`; `false`
     *         (no change) otherwise.
     */
    bool insertNode(std::size_t index, PathNode node);

    /**
     * @brief Removes a node.
     * @param index The node to remove, per nodes()'s own indexing.
     * @return `true` and removes it if `index` is valid; `false` (no
     *         change) otherwise.
     */
    bool removeNode(std::size_t index);

    /**
     * @brief Replaces a node in place - moving its anchor, converting its
     *        type, or dragging a handle are all this same operation, just
     *        with one field of the replacement changed.
     * @param index The node to replace, per nodes()'s own indexing.
     * @param node The new node to put there.
     * @return `true` and applies the replacement if `index` is valid;
     *         `false` (no change) otherwise.
     */
    bool setNode(std::size_t index, PathNode node);

    /// @brief This path's own gradient (see `docs/sound-mind-design.md`'s
    ///        "Path Gradient") - controls color and opacity from its start
    ///        (t=0) to its end (t=1).
    /// @return The gradient painted along this path.
    [[nodiscard]] const Gradient& gradient() const noexcept { return gradient_; }

    /// @brief Mutable access to this path's own gradient, for in-place edits.
    /// @return The gradient painted along this path.
    [[nodiscard]] Gradient& gradient() noexcept { return gradient_; }

    /**
     * @brief A copy of this path with every node's anchor and handles
     *        shifted by a fixed offset, gradient and node types/count
     *        unchanged - moving a Picked paint object (see
     *        `docs/sound-mind-design.md`'s "Pick") is exactly this, since
     *        a move never changes the object's own shape or coloring.
     *
     * `deltaFrequencyBins`, not a raw Hz offset - see the free
     * `sound_mind::core::translated(TimeFrequencyRect, ...)` function's
     * own docs for why: the frequency axis is log-scaled, so every
     * point's own frequency converts to its own bin position first, is
     * shifted there, then converts back - not a single Hz amount added to
     * every point alike, which would distort the path's own shape (worse
     * the wider a frequency range its own nodes/handles span) instead of
     * translating it.
     *
     * @param deltaTimeSeconds How far to shift every point along the
     *        timeline.
     * @param deltaFrequencyBins How far to shift every point along the
     *        frequency axis, in bins - not Hz.
     * @param config Interprets every point's own Hz value against
     *        `config`'s own frequency range/bin count.
     * @return The translated path.
     */
    [[nodiscard]] Path translated(double deltaTimeSeconds, double deltaFrequencyBins,
                                    const sound_mind::codec::StreamCodecConfig& config) const;

    /**
     * @brief This path's time/frequency extent, for hit-testing (see the
     *        design doc's "Pick") and Composer Mode's own track boxes.
     *
     * Deliberately coarse and cheap: the bounding box of every node's
     * anchor *and* handle points, not the tighter exact extent of the
     * curve itself (which bulges toward, but not necessarily to, its
     * handles) - computing that exactly needs a per-segment derivative
     * root-find that nothing here actually needs precision for.
     *
     * @return The bounding rectangle; all-zero for a path with no nodes.
     */
    [[nodiscard]] TimeFrequencyRect bounds() const noexcept;

    friend void to_json(nlohmann::json& json, const Path& path);
    friend void from_json(const nlohmann::json& json, Path& path);

private:
    std::vector<PathNode> nodes_;
    Gradient gradient_;
};

/// @brief Serializes a path to its JSON representation.
void to_json(nlohmann::json& json, const Path& path);

/// @brief Parses a path from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, Path& path);

/**
 * @brief Curve-fits raw, densely-sampled input (a freehand stroke's actual
 *        cursor motion) into a genuine Path with a sparse set of smooth
 *        nodes - see `docs/sound-mind-design.md`'s "Freehand Path
 *        Capture".
 *
 * Simplifies `rawPoints` down to a sparse subset via the Ramer-Douglas-
 * Peucker algorithm, then derives each interior anchor's Bézier handles
 * from a Catmull-Rom-style tangent through its now-sparse neighbors -
 * cheap enough to re-run on every new sample while a stroke is still being
 * drawn, for the design doc's "rendered live as it's captured" requirement.
 *
 * @param rawPoints The stroke's own sampled points, in drawing order.
 * @param frequencyToTimeScale How many Hz are "worth" the same simplify/
 *        smoothing weight as one second - since `timeSeconds` and
 *        `frequencyHz` are wildly different-scaled units, simplification
 *        needs *some* common yardstick between them, and this project
 *        doesn't have one built in (unlike, say, canvas pixels, where X
 *        and Y are already the same unit). A Studio caller derives this
 *        from its own canvas geometry (duration-in-seconds spanning the
 *        canvas width vs. frequency-range-in-Hz spanning its height);
 *        must be positive.
 * @param simplifyToleranceSeconds How far (in the `frequencyToTimeScale`-
 *        normalized space above, in second-equivalent units) a point may
 *        deviate from the simplified curve before it's kept as its own
 *        anchor rather than discarded - larger values produce sparser,
 *        smoother paths; must be non-negative.
 * @return The fitted path, with a fresh, fully transparent Gradient
 *         (see Path's own constructor) - painting it is a separate step.
 *         Empty (no nodes) if `rawPoints` has fewer than 2 points.
 */
[[nodiscard]] Path fitPathToPoints(const std::vector<TimeFrequencyPoint>& rawPoints, double frequencyToTimeScale,
                                    double simplifyToleranceSeconds);

}  // namespace sound_mind::core
