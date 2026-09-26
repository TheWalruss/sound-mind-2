#pragma once

#include <cstddef>

#include <QDockWidget>
#include <QStringList>

class QListWidget;
class QListWidgetItem;

namespace sound_mind::studio {

/**
 * @brief A read-only, dockable panel listing the project's own undo/redo
 *        history - "Layers Panel & Editing Enhancements v2" (`v0.Y.46.1`
 *        Installment D, "History Panel").
 *
 * One row per `UndoStack` command (its own `description`), plus a leading
 * synthetic "(Start)" row for index `0` (before anything at all was
 * done) - `count() + 1` rows total, see `UndoStack::count()`'s own docs.
 * The row at the stack's own `currentIndex()` is highlighted as the
 * current position.
 *
 * Purely a display - this panel never mutates anything itself.
 * Double-clicking a row instead asks `MainWindow` (via jumpRequested())
 * to actually perform the jump through `UndoStack::jumpTo()`, then push a
 * fresh setHistory() call back down once it has, the same "panel emits a
 * request, MainWindow owns the actual mutation" division of responsibility
 * every other dockable panel here already follows.
 *
 * No branching or versioning, matching `UndoStack`'s own linear model -
 * this is a plain list, not a tree.
 */
class HistoryPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief Builds the panel showing only the "(Start)" row.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit HistoryPanel(QWidget* parent = nullptr);

    /**
     * @brief Replaces the displayed history list.
     * @param descriptions Each already-performed command's own
     *        `UndoStack::descriptionAt()` label, in order - row `0` is
     *        always the synthetic "(Start)" entry ahead of these, not
     *        part of this list (so `descriptions[i]` becomes row `i + 1`).
     * @param currentIndex Which row is highlighted as the current
     *        position - see `UndoStack::currentIndex()`'s own docs; `0`
     *        highlights "(Start)".
     */
    void setHistory(const QStringList& descriptions, std::size_t currentIndex);

signals:
    /// @brief A row was double-clicked, requesting a jump to it.
    /// @param index The requested target index - see `UndoStack::
    ///        jumpTo()`'s own docs. `0` means "(Start)".
    void jumpRequested(std::size_t index);

private:
    QListWidget* list_;
};

}  // namespace sound_mind::studio
