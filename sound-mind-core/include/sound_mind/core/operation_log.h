#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/operation.h"

namespace sound_mind::core {

/**
 * @brief A Project's single, project-wide, ordered sequence of operations.
 *
 * See `docs/sound-mind-architecture.md`'s Core Data Model and "Composer
 * Mode Fit": this belongs to the Project, not to any one Layer, since a
 * structural operation (reordering the layer stack) doesn't target a
 * single layer's content.
 *
 * Owns every logged `Operation` (non-copyable, matching `Operation`'s own
 * stance - `unique_ptr` is how it's actually held). Every logged operation
 * is active unless something later replaces it. Undo isn't this class's
 * concern (as of `v0.1.43.1`): the project's `History` undoes an append
 * by truncate()-ing the log back to its earlier size, and restores
 * reordering and baking from its own recorded state (see
 * `docs/sound-mind-architecture.md`'s *History, Sources and Macros*).
 * Two mechanisms make an operation inactive:
 *
 * - **supersedes()** (declared on `Operation` itself) is for a
 *   *deliberate* re-edit of one specific past operation (Composer Mode
 *   retiming it, or Pick modifying a picked object) - appending a new
 *   operation whose `supersedes()` points at the old one, which becomes
 *   permanently inactive.
 * - **Baking** (bakeLayer(), `v0.1.43.1`): pooling or phase-cleaning a
 *   layer turns everything painted on it so far into its new base content,
 *   so those operations must never be replayed on top of it again. Every
 *   operation that targets the layer, with an id up to the baked threshold,
 *   is skipped.
 *
 * A third, independent mechanism - **stack order** - decides *rendering*
 * order (which of two overlapping active operations on the same layer
 * paints on top), deliberately kept separate from both of the above:
 *
 * - `operations_`'s own append order is a strict, append-only history
 *   record - it exists so truncate() and `supersedes()` chains stay simple
 *   and correct, and it must never be reinterpreted as anything else.
 * - `stackOrder_` is a separate, independently-ordered list of every
 *   operation id ever appended, used *only* to decide the order
 *   `activeOperationsTargeting()` returns its own (already-filtered)
 *   results in. A fresh, non-superseding append places its own id at the
 *   end of `stackOrder_` (newest paints on top, same as before). A
 *   *superseding* append - Pick's own move/modify/delete - inserts its
 *   own id immediately next to the operation it supersedes' own entry,
 *   rather than at the end: the whole reason this exists is so editing an
 *   object preserves its position in the stack instead of always
 *   promoting it to the top (a real, reported bug in the very first
 *   version of this mechanism, which just used append order for both
 *   purposes at once - see `docs/sound-mind-architecture.md`'s Decisions
 *   Made). Entries for ids that are no longer active (superseded or
 *   baked) are simply skipped when read; truncate() removes the entries of
 *   the operations it drops. A user can
 *   also deliberately rearrange `stackOrder_` directly -
 *   bringToFront()/sendToBack()/bringForward()/sendBackward() - without
 *   appending anything: reordering isn't an edit to *what* an operation
 *   does, only to where it renders relative to others on the same layer,
 *   so it doesn't need (and doesn't get) a logged, `supersedes()`-based
 *   history entry of its own.
 */
class OperationLog {
public:
    /// @brief Reserves a fresh, log-unique id for an operation the caller
    ///        is about to construct - `Operation`'s own concrete subtypes
    ///        (`PaintOperation` and friends) all need one at construction
    ///        time, before they exist to be append()'d.
    /// @return An id no operation in this log has used before.
    [[nodiscard]] OperationId reserveId() noexcept { return nextId_++; }

    /**
     * @brief Appends a newly constructed operation as the new most-recent,
     *        active entry.
     *
     * Places the new operation's own id into `stackOrder_` (see the
     * class's own docs): at the very end if `operation->supersedes()` is
     * `std::nullopt` (a genuinely new object, painted on top of
     * everything else so far); immediately next to its own superseded
     * id's entry otherwise, so editing an object never changes where it
     * sits in the stack.
     *
     * @param operation The operation to append; must not be `nullptr`.
     */
    void append(std::unique_ptr<Operation> operation);

    /// @brief How many operations are currently logged, active or not -
    ///        superseded and baked ones included.
    /// @return The total number of logged operations.
    [[nodiscard]] std::size_t size() const noexcept { return operations_.size(); }

    /**
     * @brief Raw, index-based access to a logged operation, regardless of
     *        whether it's currently active.
     * @param index Which operation to return, in append order.
     * @return The operation at that index.
     */
    [[nodiscard]] const Operation& at(std::size_t index) const { return *operations_.at(index); }

    /**
     * @brief Drops every operation past the first `count`, and their
     *        stack-order entries - how `History` undoes appends
     *        (`v0.1.43.1`).
     * @param count How many operations to keep; a no-op if it's not below
     *        size().
     */
    void truncate(std::size_t count);

    /// @brief The next id reserveId() will hand out.
    /// @return The id; one past the highest ever reserved.
    [[nodiscard]] OperationId nextId() const noexcept { return nextId_; }

    /// @brief Resets the next id reserveId() hands out - `History` restoring
    ///        an earlier state.
    /// @param id The id to hand out next.
    void setNextId(OperationId id) noexcept { nextId_ = id; }

    /// @brief The current rendering order of every logged operation's id -
    ///        see the class's own docs.
    /// @return Ids, back to front.
    [[nodiscard]] const std::vector<OperationId>& stackOrder() const noexcept { return stackOrder_; }

    /// @brief Replaces the rendering order wholesale - `History` restoring
    ///        an earlier reordering.
    /// @param order Ids, back to front; should name exactly the logged
    ///        operations.
    void setStackOrder(std::vector<OperationId> order) { stackOrder_ = std::move(order); }

    /**
     * @brief Where append() would place a newly appended operation's id in
     *        a stack order - the rule append() itself uses, shared so
     *        `History` can tell an append-only change from a reorder.
     * @param order The stack order to insert into.
     * @param id The appended operation's id.
     * @param supersedes What it supersedes, if anything.
     */
    static void insertIntoStackOrder(std::vector<OperationId>& order, OperationId id,
                                     std::optional<OperationId> supersedes);

    /**
     * @brief Bakes everything painted on `layer` so far into it: every
     *        operation targeting it with an id below nextId() becomes
     *        inactive (`v0.1.43.1`, see the class's own docs).
     * @param layer The layer whose base content now includes those
     *        operations' results.
     */
    void bakeLayer(LayerId layer) noexcept { bakedThrough_[layer] = nextId_ - 1; }

    /// @brief The highest operation id baked into `layer`, `0` if none.
    /// @param layer The layer.
    /// @return The threshold; operations on `layer` with an id at or below
    ///         it are inactive.
    [[nodiscard]] OperationId bakedThrough(LayerId layer) const noexcept;

    /// @brief Every layer's bake threshold - `History`'s view of the state.
    /// @return Layer id to threshold; layers never baked are absent.
    [[nodiscard]] const std::map<LayerId, OperationId>& bakedThresholds() const noexcept { return bakedThrough_; }

    /// @brief Replaces every bake threshold - `History` restoring an
    ///        earlier state.
    /// @param thresholds Layer id to threshold.
    void setBakedThresholds(std::map<LayerId, OperationId> thresholds) { bakedThrough_ = std::move(thresholds); }

    /**
     * @brief Serializes one operation, exactly as to_json() writes it
     *        inside the log - `History` records appended operations this way.
     * @param operation Any concrete operation type this log can hold.
     * @return Its JSON, `"kind"` included.
     * @throws std::invalid_argument for an operation type this log can't
     *         serialize.
     */
    [[nodiscard]] static nlohmann::json operationToJson(const Operation& operation);

    /**
     * @brief Parses one operation from operationToJson()'s shape.
     * @param json The operation's JSON.
     * @return The operation.
     * @throws std::invalid_argument if `"kind"` isn't a recognized type.
     * @throws nlohmann::json::exception on missing or malformed fields.
     */
    [[nodiscard]] static std::unique_ptr<Operation> operationFromJson(const nlohmann::json& json);

    /**
     * @brief The operations relevant to rebuilding one layer's cache, in
     *        stack order (back to front) - the exact sequence a real
     *        replay needs to consume, and the same order Pick's own
     *        hit-testing reads (most-recent-in-the-stack-first, via
     *        reverse iteration) to decide which overlapping object a
     *        click actually reaches first.
     *
     * Only operations that are neither baked into the layer (bakeLayer())
     * nor superseded by a later operation are included - matching
     * "Composer Mode Fit"'s own replay rule: "Replay honours only the
     * non-superseded operation at each point in a layer's history." The
     * *order* they're returned in is `stackOrder_`'s own (see the class's
     * own docs) - deliberately not raw append order, which would put a
     * just-edited object back at the very end (the top) regardless of
     * where it actually sits in the stack.
     *
     * @param layer Which layer to filter to, via each operation's own
     *        Operation::targetLayer().
     * @return Every matching operation, back of the stack first.
     */
    [[nodiscard]] std::vector<const Operation*> activeOperationsTargeting(LayerId layer) const;

    /**
     * @brief Moves `id` to the very top (front) of its own layer's stack -
     *        "Bring to Front".
     *
     * Only ever reorders *among* `id`'s own layer's currently active
     * operations - other layers' own entries, and any of this layer's own
     * entries that aren't currently active, are entirely undisturbed.
     * Every other reorder method here follows the same scoping.
     *
     * @param id The operation to move; must currently be active (see
     *        activeOperationsTargeting()) and target a real layer.
     * @return `true` and reorders if `id` is active and not already
     *         topmost; `false` (no change) otherwise.
     */
    bool bringToFront(OperationId id);

    /// @brief Moves `id` to the very bottom (back) of its own layer's
    ///        stack - "Send to Back". See bringToFront()'s own docs for
    ///        the shared scoping/preconditions.
    /// @param id The operation to move.
    /// @return `true` and reorders if `id` is active and not already at
    ///         the back; `false` (no change) otherwise.
    bool sendToBack(OperationId id);

    /// @brief Swaps `id` with whichever active operation on its own layer
    ///        sits immediately above it - "Bring Forward". See
    ///        bringToFront()'s own docs for the shared scoping/
    ///        preconditions.
    /// @param id The operation to move.
    /// @return `true` and reorders if `id` is active and not already
    ///         topmost; `false` (no change) otherwise.
    bool bringForward(OperationId id);

    /// @brief Swaps `id` with whichever active operation on its own layer
    ///        sits immediately below it - "Send Backward". See
    ///        bringToFront()'s own docs for the shared scoping/
    ///        preconditions.
    /// @param id The operation to move.
    /// @return `true` and reorders if `id` is active and not already at
    ///         the back; `false` (no change) otherwise.
    bool sendBackward(OperationId id);

    friend void to_json(nlohmann::json& json, const OperationLog& log);
    friend void from_json(const nlohmann::json& json, OperationLog& log);

private:
    /// @brief Which direction reorderActiveOperation() should move an
    ///        operation in - the shared implementation behind
    ///        bringToFront()/sendToBack()/bringForward()/sendBackward().
    enum class ReorderDirection { ToFront, ToBack, Forward, Backward };

    /// @brief The shared implementation behind bringToFront()/
    ///        sendToBack()/bringForward()/sendBackward() - see their own
    ///        docs for the exact per-direction behavior.
    bool reorderActiveOperation(OperationId id, ReorderDirection direction);

    std::vector<std::unique_ptr<Operation>> operations_;
    OperationId nextId_ = 1;
    std::map<LayerId, OperationId> bakedThrough_;

    /// @brief The current rendering (stack) order of every operation ever
    ///        appended - see the class's own docs for why this is kept
    ///        entirely separate from `operations_`'s own append order.
    std::vector<OperationId> stackOrder_;
};

/// @brief Serializes the log to its JSON representation.
void to_json(nlohmann::json& json, const OperationLog& log);

/// @brief Parses the log from its JSON representation - either the
///        current shape (an object with an `"operations"` array plus
///        `"nextId"`, `"stackOrder"` and an optional `"bakedThrough"`) or
///        the pre-`v0.0.24.1` shape (no `"operations"` key at all, before
///        any concrete Operation subtype existed - always an empty log).
/// @throws std::invalid_argument if an entry's own `"kind"` isn't a
///         recognized Operation subtype.
/// @throws nlohmann::json::exception if a required field is missing or
///         the wrong type (e.g. an entry with no `"id"`).
void from_json(const nlohmann::json& json, OperationLog& log);

}  // namespace sound_mind::core
