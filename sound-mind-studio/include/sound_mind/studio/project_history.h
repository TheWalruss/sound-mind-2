#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include <QObject>
#include <QString>

#include <nlohmann/json.hpp>

#include "sound_mind/core/history.h"
#include "sound_mind/core/project.h"

namespace sound_mind::studio {

/**
 * @brief The Studio's handle on the open project's `History` (`v0.1.43.1`)
 *        - what every controller commits its changes to, and what Undo,
 *        Redo, the History panel and Macro recording drive.
 *
 * Replaces the session-only `UndoStack` (closures over live controllers).
 * A controller changes the project directly and then calls commit() with a
 * description; the core history records what changed. Undo/redo restore
 * the recorded state, rebuild the content of every affected layer (via the
 * registered content rebuilders) and then emit changed(), which each
 * controller handles by refreshing its own view - so nothing about undo is
 * specific to the action being undone.
 *
 * UI-thread only.
 */
class ProjectHistory : public QObject {
    Q_OBJECT

public:
    /// @param parent Standard QObject parent.
    explicit ProjectHistory(QObject* parent = nullptr);

    /**
     * @brief Points this at the open project (or none), whose own
     *        `History` it drives. Emits historyChanged().
     *
     * Changes made to the project before this (and not committed) become
     * part of the history's baseline rather than of its next entry.
     * @param project Non-owning; may be `nullptr`.
     */
    void setProject(sound_mind::core::Project* project);

    /// @brief The project this drives.
    /// @return It, or `nullptr`.
    [[nodiscard]] sound_mind::core::Project* project() const noexcept { return project_; }

    /**
     * @brief Records everything that changed in `project` since the last
     *        commit as one History entry, then emits entryCommitted() and
     *        historyChanged().
     *
     * Also points this at `project` if it wasn't already - controllers
     * built without a MainWindow (tests) never call setProject().
     *
     * @param project The project that changed.
     * @param description What the user did, for the History panel.
     * @param metadata Extra data to keep with the entry.
     * @return The new entry's id, or `std::nullopt` if nothing changed.
     */
    std::optional<sound_mind::core::HistoryEntryId> commit(sound_mind::core::Project& project,
                                                           const QString& description,
                                                           nlohmann::json metadata = nullptr);

    /// @brief Whether undo() would do anything.
    /// @return `true` if an entry is applied.
    [[nodiscard]] bool canUndo() const noexcept;

    /// @brief Whether redo() would do anything.
    /// @return `true` if an undone entry exists.
    [[nodiscard]] bool canRedo() const noexcept;

    /**
     * @brief Undoes the latest entry (committing anything uncommitted
     *        first), rebuilds affected content, then emits changed().
     * @return `true` if something was undone; `false` if there was nothing
     *         to undo, or a base content it needed couldn't be recovered.
     */
    bool undo();

    /// @brief Redoes the next undone entry - see undo().
    /// @return `true` if something was redone.
    bool redo();

    /**
     * @brief Undoes or redoes until `index` entries are applied - the
     *        History panel's jump.
     * @param index The target position, clamped to `[0, count()]`.
     * @return `true` if the position was reached.
     */
    bool jumpTo(std::size_t index);

    /// @brief How many entries exist, undone ones included.
    /// @return The count; `0` without a project.
    [[nodiscard]] std::size_t count() const noexcept;

    /// @brief How many entries are applied.
    /// @return The position; `0` without a project.
    [[nodiscard]] std::size_t currentIndex() const noexcept;

    /// @brief One entry's description.
    /// @param index Which entry.
    /// @return Its description.
    [[nodiscard]] QString descriptionAt(std::size_t index) const;

    /**
     * @brief Registers a function that rebuilds the rendered content of the
     *        layers an undo/redo changed, run before changed() is emitted
     *        so every view refreshes from up-to-date content.
     * @param rebuilder Called with what changed.
     */
    void addContentRebuilder(std::function<void(const sound_mind::core::HistoryChange&)> rebuilder);

    /**
     * @brief Sets how a base content the history no longer holds is
     *        recovered (by replaying the history - `v0.1.43.4`).
     * @param resolver The resolver; empty means "can't".
     */
    void setBaseResolver(sound_mind::core::HistoryBaseResolver resolver);

signals:
    /// @brief An undo, redo or jump changed the project - refresh whatever
    ///        shows the parts named in `change`.
    void changed(const sound_mind::core::HistoryChange& change);

    /// @brief commit() recorded a new entry.
    void entryCommitted(sound_mind::core::HistoryEntryId id);

    /// @brief An entry was undone (by undo() or, one per entry, jumpTo()) -
    ///        what Macro recording follows (`v0.1.44.1`).
    void entryUndone(sound_mind::core::HistoryEntryId id);

    /// @brief An entry was redone (by redo() or, one per entry, jumpTo()).
    void entryRedone(sound_mind::core::HistoryEntryId id);

    /// @brief The entries or the position changed in any way (the History
    ///        panel and the Undo/Redo actions refresh on this).
    void historyChanged();

private:
    bool finish(const std::optional<sound_mind::core::HistoryChange>& change);
    void signalSteps(std::size_t from);

    sound_mind::core::Project* project_ = nullptr;
    std::vector<std::function<void(const sound_mind::core::HistoryChange&)>> contentRebuilders_;
    sound_mind::core::HistoryBaseResolver resolver_;
};

}  // namespace sound_mind::studio
