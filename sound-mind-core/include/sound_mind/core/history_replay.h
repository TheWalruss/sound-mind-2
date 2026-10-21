#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/history.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::core {

/// @brief A layer's rendered content during a replay - rendering it first if
///        it's stale.
using ReplayRenderedLookup = std::function<const sound_mind::codec::StreamImage*(LayerId)>;

/**
 * @brief Remakes a base from its recipe during a replay - the Studio's
 *        `runBaseRecipe()`, injected so Core stays free of Qt.
 *
 * Called with the recipe, the layer, the replayed project as it stands just
 * before the entry that made the base, and a lookup of rendered content.
 * Returns the base, or `std::nullopt` (with a reason) if it can't.
 */
using ReplayRecipeRunner = std::function<std::optional<LayerBase>(
    const nlohmann::json& recipe, LayerId layer, const Project& project, const ReplayRenderedLookup& rendered,
    std::string* error)>;

/// @brief How a history is replayed.
struct ReplayOptions {
    /// Remakes bases from recipes; required unless every base is kept.
    ReplayRecipeRunner runRecipe;
    /// Use the base images the history still holds in memory where it has
    /// them (exact, and fast) instead of running recipes. `false` for
    /// Remaster, whose settings differ.
    bool useKeptBases = true;
    /// Adapts each entry before it's applied (Remaster: new settings,
    /// replaced sources). Called on a copy.
    std::function<void(HistoryEntry&)> transformEntry;
    /// Polled between entries; `true` stops the replay.
    std::function<bool()> shouldCancel;
    /// Told how far along the replay is.
    std::function<void(std::size_t done, std::size_t total)> progress;
    /// Keep every stepped entry, with the bases its step set aside, so
    /// stepping it back (a Macro's undo and redo) needs no recipe -
    /// `v0.1.44.2`. Costs memory for each replaced base.
    bool keepSteppedEntries = false;
};

/**
 * @brief Replays a history, entry by entry, into a fresh project
 *        (`v0.1.43.3`) - the engine behind recovering a base no longer in
 *        memory, Remaster and Macro rendering (`docs/sound-mind-
 *        architecture.md`'s *History, Sources and Macros*).
 *
 * Starts from `Project::createNew(settings)` - the state every complete
 * history (`History::startsAtCreation()`) begins from - and applies each
 * entry's "after" side. Bases come from the entry (if kept and allowed) or
 * from the recipe runner. Layer content is rendered lazily: a layer whose
 * base or operations changed is only re-rendered when something reads it
 * (a recipe, a Mind Grain stroke on another layer, the caller), so a long
 * painting session doesn't re-render after every stroke.
 *
 * Not thread-safe; owns its project, so it can run on a background thread
 * while the original is edited.
 */
class HistoryReplayer {
public:
    /**
     * @param settings The project settings to replay at - the original's,
     *        or (Remaster) new ones.
     * @param options How to replay.
     */
    HistoryReplayer(const ProjectSettings& settings, ReplayOptions options);

    /**
     * @brief Applies one entry.
     * @param entry The entry (copied; transformed if a transform is set).
     * @param error Receives the reason on failure; may be `nullptr`.
     * @return `true` if applied; `false` if a base couldn't be made (the
     *         project is then unchanged by this entry).
     */
    bool apply(const HistoryEntry& entry, std::string* error = nullptr);

    /**
     * @brief Reverts one entry - the replay of an undo (`v0.1.44.2`).
     * @param entry The entry, as recorded (the project is in its "after"
     *        state).
     * @param error Receives the reason on failure; may be `nullptr`.
     * @return `true` if reverted.
     */
    bool revert(const HistoryEntry& entry, std::string* error = nullptr);

    /**
     * @brief Applies the first `count` entries of `history`, reporting
     *        progress and honoring cancellation.
     * @param history The history.
     * @param count How many entries.
     * @param error Receives the reason on failure or cancellation.
     * @return `true` if every entry was applied.
     */
    bool applyAll(const History& history, std::size_t count, std::string* error = nullptr);

    /// @brief The replayed project. Content of stale layers isn't rendered
    ///        until rendered() or renderAll().
    /// @return It.
    [[nodiscard]] Project& project() noexcept { return project_; }

    /**
     * @brief One layer's rendered content, rendering it first if stale.
     * @param layer The layer.
     * @return Its content, or `nullptr` if it has none.
     */
    const sound_mind::codec::StreamImage* rendered(LayerId layer);

    /// @brief Renders every stale layer - before handing the project over.
    void renderAll();

    /// @brief Hands the project over, every layer rendered.
    /// @return The replayed project.
    [[nodiscard]] Project takeProject();

private:
    void render(LayerId layer, std::set<LayerId>& visiting);
    bool step(const HistoryEntry& entry, bool forward, std::string* error);

    Project project_;
    ReplayOptions options_;
    std::set<LayerId> stale_;
    std::map<HistoryEntryId, HistoryEntry> stepped_;
};

}  // namespace sound_mind::core
