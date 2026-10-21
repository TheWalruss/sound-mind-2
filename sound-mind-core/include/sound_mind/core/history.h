#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/operation.h"

namespace sound_mind::core {

class Project;

/// @brief Identifies one History entry; never reused within a project.
using HistoryEntryId = std::uint64_t;

/**
 * @brief Everything about a Project that its History tracks, without any
 *        raster data (`v0.1.43.1`).
 *
 * Captured after every commit, and compared with the next capture to find
 * what an action changed - so every mutation is recorded, whatever code
 * made it. Not tracked: the project's settings (fixed at creation;
 * Remaster overrides them) and the layers' rendered content (derived from
 * their base and operations).
 */
struct ProjectState {
    /// One layer's tracked state.
    struct LayerState {
        /// The layer's id.
        LayerId id = 0;
        /// The layer's JSON (`to_json(Layer)`), base recipe included.
        nlohmann::json json;
        /// Its Layer::baseRevision().
        std::uint64_t baseRevision = 0;
        /// Whether it has any base content.
        bool hasContent = false;
    };

    /// Every layer, bottom to top.
    std::vector<LayerState> layers;
    /// `Project::nextLayerId()`.
    LayerId nextLayerId = 1;
    /// Every logged operation's id, in append order.
    std::vector<OperationId> operationIds;
    /// `OperationLog::nextId()`.
    OperationId nextOperationId = 1;
    /// `OperationLog::stackOrder()`.
    std::vector<OperationId> stackOrder;
    /// `OperationLog::bakedThresholds()`.
    std::map<LayerId, OperationId> baked;
    /// Every resource library's JSON array, keyed by its project-file name
    /// (`"mindWaves"`, `"mindShots"`, ...).
    std::map<std::string, nlohmann::json> resources;
};

/**
 * @brief Captures `project`'s tracked state.
 * @param project The project.
 * @return Its state.
 */
[[nodiscard]] ProjectState captureProjectState(const Project& project);

/**
 * @brief The names of the resource libraries a ProjectState tracks, in the
 *        project file's order.
 * @return `"mindWaves"`, `"mindShots"`, `"mindGrains"`, `"resonanceProfiles"`,
 *         `"convolutionKernels"`, `"toolPresets"`, `"filterPresets"`,
 *         `"gridPresets"`, `"midiProgramMappings"`.
 */
[[nodiscard]] const std::vector<std::string>& historyResourceCollections();

/// @brief How one History entry changed one layer.
struct HistoryLayerChange {
    /// The layer.
    LayerId id = 0;
    /// Its JSON before the entry; `null` if the entry added it.
    nlohmann::json before;
    /// Its JSON after the entry; `null` if the entry removed it.
    nlohmann::json after;
    /// Whether its base content changed (always when added or removed with
    /// content).
    bool baseChanged = false;
    /// Whether it had base content before (`v0.1.43.3`) - a side without
    /// any is restored as empty, with nothing to recover.
    bool beforeHasContent = false;
    /// Whether it has base content after - see beforeHasContent.
    bool afterHasContent = false;
    /// Session-only: the base before and after, while kept - see
    /// `History::setBaseBudgetBytes()`. Restored exactly on undo/redo.
    std::optional<LayerBase> baseBefore;
    /// See baseBefore.
    std::optional<LayerBase> baseAfter;
};

/// @brief How one History entry changed one resource library.
struct HistoryResourceChange {
    /// The library's name - see historyResourceCollections().
    std::string collection;
    /// The whole library's JSON before.
    nlohmann::json before;
    /// The whole library's JSON after.
    nlohmann::json after;
};

/**
 * @brief One recorded action: what it changed, before and after
 *        (`v0.1.43.1`, `docs/sound-mind-architecture.md`'s *History,
 *        Sources and Macros*).
 *
 * Applying the "after" side redoes it; applying the "before" side undoes
 * it. Every field but the session-only base images is saved with the
 * project.
 */
struct HistoryEntry {
    /// Stable id.
    HistoryEntryId id = 0;
    /// What the user did, for the History panel ("Painted stroke").
    std::string description;
    /// When, in seconds since the Unix epoch.
    double wallClockSeconds = 0.0;
    /// Free-form extra data about the action (e.g. which source an import
    /// used); `null` if none.
    nlohmann::json metadata;
    /// Layers added, removed or changed.
    std::vector<HistoryLayerChange> layers;
    /// The layer order before and after, when the entry changed it
    /// (including by adding or removing a layer).
    std::optional<std::vector<LayerId>> layerOrderBefore;
    /// See layerOrderBefore.
    std::optional<std::vector<LayerId>> layerOrderAfter;
    /// `Project::nextLayerId()` before.
    LayerId nextLayerIdBefore = 1;
    /// `Project::nextLayerId()` after.
    LayerId nextLayerIdAfter = 1;
    /// How many operations were logged before.
    std::size_t operationCountBefore = 0;
    /// The operations the entry appended (`OperationLog::operationToJson()`).
    std::vector<nlohmann::json> appendedOperations;
    /// `OperationLog::nextId()` before.
    OperationId nextOperationIdBefore = 1;
    /// `OperationLog::nextId()` after.
    OperationId nextOperationIdAfter = 1;
    /// The stack order before and after, when it changed other than by
    /// appending (Bring to Front and friends).
    std::optional<std::vector<OperationId>> stackOrderBefore;
    /// See stackOrderBefore.
    std::optional<std::vector<OperationId>> stackOrderAfter;
    /// The bake thresholds before and after, when the entry changed them.
    std::optional<std::map<LayerId, OperationId>> bakedBefore;
    /// See bakedBefore.
    std::optional<std::map<LayerId, OperationId>> bakedAfter;
    /// Resource libraries the entry changed.
    std::vector<HistoryResourceChange> resources;
};

/// @brief Serializes an entry (without its session-only base images).
void to_json(nlohmann::json& json, const HistoryEntry& entry);

/// @brief Parses an entry.
/// @throws nlohmann::json::exception on missing or malformed fields.
void from_json(const nlohmann::json& json, HistoryEntry& entry);

/**
 * @brief What undoing or redoing changed - what the Studio must refresh.
 */
struct HistoryChange {
    /// Layers added, removed, or with changed properties.
    std::set<LayerId> layers;
    /// Layers (still present) whose rendered content must be rebuilt: their
    /// base or their operations changed.
    std::set<LayerId> contentLayers;
    /// Whether layers were added, removed or reordered.
    bool layerListChanged = false;
    /// Whether the operation log changed.
    bool operationsChanged = false;
    /// Resource libraries that changed.
    std::set<std::string> resources;
};

/**
 * @brief Recovers a layer's base content when the history no longer holds
 *        it in memory (after reloading the project, or once evicted) - by
 *        replaying the history from the start (`v0.1.43.4`).
 *
 * Called with the history position (how many entries are applied) and the
 * layer; returns that layer's base at that position, or `std::nullopt` if
 * it can't.
 */
using HistoryBaseResolver = std::function<std::optional<LayerBase>(std::size_t position, LayerId layer)>;

/**
 * @brief A project's complete record of what was done to it (`v0.1.43.1`),
 *        the source of truth for undo/redo, the History panel, Macros and
 *        Remastering - see `docs/sound-mind-architecture.md`'s *History,
 *        Sources and Macros*.
 *
 * Code that changes a project changes it directly and then commit()s with
 * a description; the history works out what changed by comparing the
 * project with its state at the previous commit. Undo and redo apply an
 * entry's recorded before or after state.
 *
 * Linear: committing after an undo drops the undone entries.
 *
 * Not thread-safe; owned by its `Project` and used from whichever thread
 * owns that (the UI thread in the Studio).
 */
class History {
public:
    /**
     * @brief Records `project` as it stands as the state further changes are
     *        compared against, without adding an entry - after creating or
     *        loading it. Clears any displaced bases (see
     *        Layer::takeDisplacedBase()).
     * @param project The project this history belongs to.
     */
    void setBaseline(Project& project);

    /// @brief Whether setBaseline() has run.
    /// @return `true` once it has.
    [[nodiscard]] bool hasBaseline() const noexcept { return hasBaseline_; }

    /**
     * @brief Whether the entries start from the project's creation
     *        (`v0.1.43.3`) - so replaying them into
     *        `Project::createNew(settings)` rebuilds the project. `false` for
     *        a project from before History existed, whose history starts at
     *        the first time it was opened, or after markIncomplete().
     * @return `true` for a complete history.
     */
    [[nodiscard]] bool startsAtCreation() const noexcept { return startsAtCreation_; }

    /// @brief Records that the project was changed outside the history, so
    ///        its entries no longer rebuild it from creation.
    void markIncomplete() noexcept { startsAtCreation_ = false; }

    /// @brief Records that the entries start at the project's creation -
    ///        `Project::createNew()`.
    void markStartsAtCreation() noexcept { startsAtCreation_ = true; }

    /**
     * @brief Makes `entries` this history, all applied, and `project` (which
     *        they must already describe) its baseline - a project rebuilt by
     *        replaying them (Remaster, `v0.1.43.4`) keeps them as its own
     *        history, so it can be replayed again.
     * @param project The project the entries rebuilt.
     * @param entries The entries; their session-only base images are dropped.
     * @param startsAtCreation Whether they start at the project's creation.
     */
    void assignEntries(Project& project, std::vector<HistoryEntry> entries, bool startsAtCreation);

    /**
     * @brief Records everything that changed in `project` since the last
     *        commit, undo or redo as one entry.
     *
     * Drops any undone entries first. Without a baseline, sets one instead.
     *
     * @param project The project.
     * @param description What the user did.
     * @param metadata Extra data to keep with the entry.
     * @return The new entry's id, or `std::nullopt` if nothing changed.
     */
    std::optional<HistoryEntryId> commit(Project& project, std::string description,
                                         nlohmann::json metadata = nullptr);

    /// @brief Whether `project` differs from its state at the last commit.
    /// @param project The project.
    /// @return `true` if a commit() would record an entry.
    [[nodiscard]] bool hasUncommittedChanges(const Project& project) const;

    /// @brief How many entries exist, undone ones included.
    /// @return The count.
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

    /// @brief How many entries are applied - the current position.
    /// @return A value in `[0, size()]`.
    [[nodiscard]] std::size_t position() const noexcept { return position_; }

    /// @brief One entry.
    /// @param index Which, in order.
    /// @return The entry.
    [[nodiscard]] const HistoryEntry& entry(std::size_t index) const { return entries_.at(index); }

    /// @brief Whether undo() would do anything.
    /// @return `true` if an entry is applied (or changes are uncommitted).
    [[nodiscard]] bool canUndo() const noexcept { return position_ > 0; }

    /// @brief Whether redo() would do anything.
    /// @return `true` if an undone entry exists.
    [[nodiscard]] bool canRedo() const noexcept { return position_ < entries_.size(); }

    /**
     * @brief Undoes the latest applied entry, first committing any
     *        uncommitted changes (as "Edit") so they're what gets undone.
     * @param project The project.
     * @param resolver Recovers a base no longer held in memory.
     * @return What changed, or `std::nullopt` if nothing was undone (nothing
     *         to undo, or a needed base couldn't be recovered - then
     *         nothing changed).
     */
    std::optional<HistoryChange> undo(Project& project, const HistoryBaseResolver& resolver = {});

    /**
     * @brief Redoes the next undone entry.
     * @param project The project.
     * @param resolver See undo().
     * @return What changed, or `std::nullopt` as for undo().
     */
    std::optional<HistoryChange> redo(Project& project, const HistoryBaseResolver& resolver = {});

    /**
     * @brief Undoes or redoes until `position` entries are applied.
     * @param project The project.
     * @param position The target, clamped to `[0, size()]`.
     * @param resolver See undo().
     * @return Everything that changed, merged; `std::nullopt` if a step
     *         failed (the position then stays where that step left it).
     */
    std::optional<HistoryChange> jumpTo(Project& project, std::size_t position,
                                        const HistoryBaseResolver& resolver = {});

    /**
     * @brief Applies `entry`'s "after" side to `project` - replaying a
     *        history into a fresh project (Remaster, Macros). Doesn't
     *        record anything in this history.
     * @param project The project, in the entry's "before" state.
     * @param entry The entry.
     * @param base Supplies the base of each layer whose base the entry set,
     *        when the entry doesn't hold it.
     * @return What changed; `std::nullopt` if a base was missing.
     */
    static std::optional<HistoryChange> applyEntry(
        Project& project, HistoryEntry& entry,
        const std::function<std::optional<LayerBase>(LayerId)>& base = {});

    /**
     * @brief Applies `entry`'s "before" side - undoing it in a replay (a
     *        Macro that recorded an undo, `v0.1.44.2`). The mirror of
     *        applyEntry().
     * @param project The project, in the entry's "after" state.
     * @param entry The entry.
     * @param base Supplies the base of each layer whose base the entry
     *        changed, as it was before, when the entry doesn't hold it.
     * @return What changed; `std::nullopt` if a base was missing.
     */
    static std::optional<HistoryChange> revertEntry(
        Project& project, HistoryEntry& entry,
        const std::function<std::optional<LayerBase>(LayerId)>& base = {});

    /**
     * @brief Caps how much memory the entries' kept base images may take;
     *        the oldest are dropped first, and recovered with a resolver if
     *        needed later.
     * @param bytes The budget; 1 GiB by default.
     */
    void setBaseBudgetBytes(std::size_t bytes);

    /// @brief How much memory the kept base images take now.
    /// @return Bytes.
    [[nodiscard]] std::size_t keptBaseBytes() const;

    /// @brief The id the next entry will get.
    /// @return The id.
    [[nodiscard]] HistoryEntryId nextEntryId() const noexcept { return nextEntryId_; }

    friend void to_json(nlohmann::json& json, const History& history);
    friend void from_json(const nlohmann::json& json, History& history);

private:
    std::optional<HistoryChange> step(Project& project, bool forward, const HistoryBaseResolver& resolver);
    void enforceBaseBudget();

    std::vector<HistoryEntry> entries_;
    std::size_t position_ = 0;
    HistoryEntryId nextEntryId_ = 1;
    ProjectState committed_;
    bool hasBaseline_ = false;
    bool startsAtCreation_ = false;
    std::size_t baseBudgetBytes_ = std::size_t{1} << 30;
};

/// @brief Serializes the applied entries (undone ones aren't saved).
void to_json(nlohmann::json& json, const History& history);

/// @brief Parses a history; every entry is applied. Leaves it without a
///        baseline - the loading project sets one.
/// @throws nlohmann::json::exception on malformed data.
void from_json(const nlohmann::json& json, History& history);

}  // namespace sound_mind::core
