#pragma once

#include <optional>
#include <vector>

#include <QDockWidget>
#include <QString>

#include "sound_mind/core/mind_wave.h"

class QComboBox;
class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace sound_mind::studio {

class MindWaveEditor;

/**
 * @brief A dockable panel managing the current project's MindWave library -
 *        `v0.Y.31.1` Installment C2's own "bare-bones MindWave management
 *        panel" (`docs/sound-mind-roadmap.md`'s own confirmed scope).
 *
 * Purely presentational, the same division of responsibility as
 * `LayersPanel`: every row action and edit is a signal `MindWaveController`
 * connects to its own handlers - no `Project`/`NamedMindWave` mutation
 * happens here.
 *
 * **Two `MindWaveEditor`s, not one**: the top one edits whichever library
 * entry is currently selected in the list above it; the bottom one edits
 * whichever entry of *that* MindWave's own `superpositionStack()` is
 * currently selected in the smaller list beside it (confirmed with the
 * user: a minimal nested editor, one level deep - a stack member's own
 * further-nested stack, if it somehow has one, isn't reachable through
 * this UI). Every edit anywhere - the top editor, the stack list itself
 * (add/remove), the stack member editor, or the blend mode combo -
 * reconstructs this panel's own current, complete `MindWave` (top-level
 * generator parameters from the top editor, `superpositionStack()`/
 * `superpositionBlendMode()` from this panel's own tracked stack state)
 * and emits exactly one mindWaveChanged() with it, the same "emit the
 * whole thing, not a per-field delta" convention `FilterConfigurationPanel`
 * already established.
 *
 * **Preview**: a checkable toggle previews the currently selected
 * entry's own complete field (top-level generator plus its full
 * superposition stack - never a lone stack member in isolation) as a
 * live grayscale overlay on the canvas, per
 * `docs/sound-mind-design.md`'s "Low Frequency Oscillations" - see
 * previewToggled()'s own docs. This panel only ever emits the toggle;
 * `MindWaveController` owns deciding *what* to preview and pushing it to
 * `CanvasWidget`.
 */
class MindWavesPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief One library entry's worth of display data - a Qt-friendly
    /// mirror of `sound_mind::core::NamedMindWave`.
    struct RowData {
        /// @brief Mirrors `sound_mind::core::NamedMindWave::id`.
        sound_mind::core::MindWaveId id = 0;

        /// @brief Mirrors `sound_mind::core::NamedMindWave::name`.
        QString name;

        /// @brief Mirrors `sound_mind::core::NamedMindWave::wave`.
        sound_mind::core::MindWave wave;
    };

    /// @brief Builds the panel with an initially-empty library.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit MindWavesPanel(QWidget* parent = nullptr);

    /**
     * @brief Replaces the displayed library entries.
     *
     * Preserves the current selection (and everything the two editors and
     * the stack list currently show) if the selected id is still present
     * among `entries` - the same "survives an unrelated refresh" rule
     * `LayersPanel::setLayers()`'s own docs establish - but always
     * redisplays from `entries`' own data when it is, so the panel never
     * silently drifts from whatever `MindWaveController` just wrote back
     * to the project.
     *
     * @param entries The project's current MindWave library.
     */
    void setMindWaves(const std::vector<RowData>& entries);

    /// @brief The currently selected library entry's id, if any.
    /// @return That entry's id, or `std::nullopt` if no row is selected.
    [[nodiscard]] std::optional<sound_mind::core::MindWaveId> selectedMindWaveId() const {
        return selectedMindWaveId_;
    }

    /// @brief Clears the current selection.
    void clearSelection();

    /// @brief Selects `id` as if its row had been clicked - a no-op if no
    ///        row currently has that id. Used by `MindWaveController` to
    ///        immediately select a MindWave it just added.
    /// @param id The entry to select.
    void selectMindWave(sound_mind::core::MindWaveId id);

    /// @brief Whether the Preview toggle is currently checked.
    /// @return `true` if Preview is on; `false` by default.
    [[nodiscard]] bool previewEnabled() const;

    /**
     * @brief Sets the Preview toggle's own checked state, without emitting
     *        previewToggled() - used by `MindWaveController` to turn
     *        Preview back off on a project switch (see its own docs on
     *        why), where re-emitting and re-triggering a fresh preview
     *        render would be redundant/wrong (the new project's own
     *        canvas is what's about to be shown, with nothing selected
     *        yet to preview).
     * @param enabled The new checked state.
     */
    void setPreviewEnabled(bool enabled);

signals:
    /// @brief The current selection changed.
    /// @param id The newly selected entry's id, or `std::nullopt` if the
    ///        selection was cleared.
    void selectionChanged(std::optional<sound_mind::core::MindWaveId> id);

    /// @brief The "+ Add MindWave" button was clicked.
    void addRequested();

    /// @brief A row's name was double-clicked.
    void renameRequested(sound_mind::core::MindWaveId id);

    /// @brief A row's delete button was clicked.
    void deleteRequested(sound_mind::core::MindWaveId id);

    /// @brief The currently selected entry's own MindWave changed - via
    ///        the top editor, the stack list, the stack member editor, or
    ///        the blend mode combo. Never fires with no selection.
    /// @param id The entry that changed.
    /// @param wave Its own new, complete MindWave.
    void mindWaveChanged(sound_mind::core::MindWaveId id, const sound_mind::core::MindWave& wave);

    /**
     * @brief The Preview toggle was clicked by the user (see the class's
     *        own docs) - not emitted by setPreviewEnabled()'s own
     *        programmatic changes.
     *
     * `MindWaveController` reacts by pushing (or clearing) the currently
     * selected entry's own complete MindWave onto `CanvasWidget::
     * setMindWavePreview()` - this panel has no `CanvasWidget` of its own
     * to push to, and no opinion on *what* gets previewed beyond "the
     * current selection," so it only ever reports the raw toggle.
     *
     * @param enabled The toggle's own new checked state.
     */
    void previewToggled(bool enabled);

private:
    /// @brief Rebuilds `stackList_` from `currentStack_`.
    void refreshStackList();

    /// @brief Reconstructs this panel's own current, complete MindWave
    ///        (top editor + currentStack_/currentBlendMode_) and emits
    ///        mindWaveChanged() with it - a no-op if nothing is selected.
    void emitCurrentMindWaveChanged();

    /// @brief Loads `wave`'s own superposition stack/blend mode into
    ///        currentStack_/currentBlendMode_/the blend combo, refreshes
    ///        stackList_, and clears the stack member selection - the
    ///        shared setup a fresh library selection and a mindWaveChanged
    ///        refresh both need.
    void loadStackState(const sound_mind::core::MindWave& wave);

    QPushButton* addButton_ = nullptr;
    QPushButton* previewButton_ = nullptr;
    QListWidget* list_ = nullptr;
    MindWaveEditor* mindWaveEditor_ = nullptr;

    QListWidget* stackList_ = nullptr;
    QPushButton* addMemberButton_ = nullptr;
    QPushButton* removeMemberButton_ = nullptr;
    QComboBox* blendModeCombo_ = nullptr;
    MindWaveEditor* stackMemberEditor_ = nullptr;

    /// @brief The rows as of the last setMindWaves() call - used to look
    /// up the currently selected entry's own data.
    std::vector<RowData> currentRows_;

    /// @brief See selectedMindWaveId()'s own docs.
    std::optional<sound_mind::core::MindWaveId> selectedMindWaveId_;

    /// @brief The selected library entry's own superposition stack, kept
    /// here (not read back from mindWaveEditor_, which has no controls
    /// for it) so it survives independently of the top editor's own
    /// generator-parameter edits.
    std::vector<sound_mind::core::MindWave> currentStack_;

    /// @brief The selected library entry's own blend mode - see
    /// currentStack_'s own docs for why this lives here, not on
    /// mindWaveEditor_.
    sound_mind::core::SuperpositionBlendMode currentBlendMode_ = sound_mind::core::SuperpositionBlendMode::Multiply;

    /// @brief Which currentStack_ index the bottom editor currently shows,
    /// if any.
    std::optional<int> selectedStackMemberIndex_;
};

}  // namespace sound_mind::studio
