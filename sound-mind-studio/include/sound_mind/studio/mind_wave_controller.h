#pragma once

#include <QObject>
#include <QString>

#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/project.h"

namespace sound_mind::studio {

class CanvasWidget;
class FilterConfigurationPanel;
class LayersPanel;
class MindWavesPanel;

/**
 * @brief Owns the current project's MindWave library - add/remove/rename/
 *        edit - and keeps `MindWavesPanel`/`LayersPanel`/
 *        `FilterConfigurationPanel` in sync with it. `v0.Y.31.1`
 *        (MindWaves v1) Installment C2's own controller (extended in
 *        Installment D3 for `FilterConfigurationPanel`), the same "own
 *        presentation" treatment `LayerController`/`ToolPaletteController`
 *        already received.
 *
 * Constructed with non-owning pointers to `mindWavesPanel`, `layersPanel`,
 * and `filterConfigurationPanel` - all three stay `MainWindow`-owned (dock
 * widgets, like they already are for `LayerController`) - and pushes the
 * current library into all three after any mutation: `mindWavesPanel` gets
 * the full library (name, generator type, parameters); `layersPanel` and
 * `filterConfigurationPanel` each get only id/name pairs, for their own
 * respective binding combos (`LayersPanel::setAvailableMindWaves()`,
 * `FilterConfigurationPanel::setAvailableMindWaves()`).
 *
 * **Deliberately does not own `Layer::opacityMindWave()`'s or
 * `FilterConfiguration`'s own parameter-binding mutation** - binding a
 * *layer* or a *filter parameter* to a MindWave is a layer/filter
 * mutation, not a MindWave-library one, so those stay
 * `LayerController::setLayerOpacityMindWave()`'s and
 * `FilterConfigurationPanel`'s own edited-config-round-trip's job
 * respectively; this class only keeps each panel's own combo populated
 * with *which* MindWaves exist to bind to.
 *
 * **Preview**: also owns pushing the currently selected MindWave onto
 * `CanvasWidget::setMindWavePreview()` whenever `mindWavesPanel`'s own
 * Preview toggle, selection, or the selected entry's own edits change -
 * see `MindWavesPanel`'s own class docs for the feature, and
 * updateMindWavePreview()'s/handleMindWaveEditedWhilePreviewing()'s own
 * docs for why edits specifically need their own, separate handler.
 */
class MindWaveController : public QObject {
    Q_OBJECT

public:
    /**
     * @param mindWavesPanel Non-owning; refreshed after any mutation, and
     *        the source of every edit this controller applies. Must
     *        outlive this controller.
     * @param layersPanel Non-owning; kept in sync with the library's own
     *        id/name pairs. Must outlive this controller.
     * @param filterConfigurationPanel Non-owning; kept in sync with the
     *        library's own id/name pairs, the same as `layersPanel`. Must
     *        outlive this controller.
     * @param canvas Non-owning; receives the live Preview overlay (see the
     *        class's own docs). Must outlive this controller.
     * @param parent The owning object, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    MindWaveController(MindWavesPanel* mindWavesPanel, LayersPanel* layersPanel,
                        FilterConfigurationPanel* filterConfigurationPanel, CanvasWidget* canvas,
                        QObject* parent = nullptr);

    /// @brief Sets which project this controller looks up/mutates
    ///        MindWaves in. Does *not* itself refresh the panels -
    ///        `MainWindow`'s own `setProject()` calls
    ///        `refreshMindWavesPanel()` separately, matching
    ///        `LayerController::setProject()`'s own docs. Also turns
    ///        Preview back off (a previous project's own selection/
    ///        MindWave means nothing for a different one) - `canvas_`
    ///        itself independently clears its own preview state on a
    ///        project switch too (see `CanvasWidget::setProject()`'s own
    ///        docs), so this is belt-and-suspenders, not the only place
    ///        it happens.
    /// @param project The project to target; may be `nullptr`.
    void setProject(sound_mind::core::Project* project);

    /// @brief Pushes the current project's MindWave library into
    ///        `MindWavesPanel` and `LayersPanel` - an empty library (not a
    ///        no-op) when no project is set.
    void refreshMindWavesPanel();

    /// @brief Adds a new, default MindWave to the current project (see
    ///        `sound_mind::core::MindWave`'s own docs for what "default"
    ///        means), named "MindWave N" (the smallest N not already
    ///        used), and selects it immediately in `MindWavesPanel`. A
    ///        no-op if no project is set.
    void addMindWave();

    /// @brief Removes the MindWave with the given id from the current
    ///        project. A no-op if no project is set or no MindWave with
    ///        this id exists. Does **not** clear any layer's own
    ///        `opacityMindWave()` reference to it - see `sound_mind::core::
    ///        Project::removeMindWave()`'s own docs on why a dangling
    ///        reference is harmless.
    /// @param id The MindWave to remove.
    void removeMindWave(sound_mind::core::MindWaveId id);

    /// @brief Renames the MindWave with the given id, without prompting -
    ///        the non-prompting core behind `MainWindow`'s own rename
    ///        prompt, matching `LayerController::renameLayerTo()`'s exact
    ///        shape.
    /// @param id The MindWave to rename.
    /// @param newName The new name - an empty name is rejected.
    /// @return `true` on success; `false` if no project is set, no
    ///         MindWave with this id exists, or `newName` is empty.
    bool renameMindWaveTo(sound_mind::core::MindWaveId id, const QString& newName);

    /// @brief Applies `MindWavesPanel`'s own edited MindWave back onto the
    ///        library entry it belongs to. A no-op if no project is set or
    ///        no MindWave with this id exists.
    /// @param id The entry that changed.
    /// @param wave Its own new, complete MindWave.
    void updateMindWave(sound_mind::core::MindWaveId id, const sound_mind::core::MindWave& wave);

signals:
    /// @brief Emitted whenever a mutation above actually took effect -
    ///        `MainWindow`'s own cue to mark `hasUnsavedChanges()`.
    void mindWavesChanged();

private:
    /**
     * @brief Pushes (or clears) the currently selected library entry's own
     *        MindWave onto `canvas_->setMindWavePreview()`, looked up
     *        fresh from `project_` - correct whenever `project_` is
     *        already authoritative for it (the Preview toggle itself
     *        changing, or a plain selection change), but **not** for an
     *        edit to the selected entry's own MindWave - see
     *        handleMindWaveEditedWhilePreviewing()'s own docs for why
     *        that needs a separate handler instead of this one.
     *
     * Connected to `mindWavesPanel_->previewToggled()`/`selectionChanged()`.
     */
    void updateMindWavePreview();

    /**
     * @brief Pushes `wave` straight onto `canvas_->setMindWavePreview()`
     *        when Preview is on and `id` is the currently selected entry -
     *        connected to `mindWavesPanel_->mindWaveChanged()`, fired
     *        *before* `MainWindow`'s own separate connection to the same
     *        signal has written `wave` back into `project_` (both are
     *        plain, independently-ordered connections to one Qt signal -
     *        this controller's own happens to run first, since it's
     *        connected first, in this class's own constructor, but relying
     *        on that ordering at all would be fragile). Using `wave`
     *        directly, rather than reading `project_->mindWaveById(id)`
     *        the way updateMindWavePreview() does, means this handler
     *        never depends on that ordering, and always previews the
     *        genuinely current edit rather than the one just before it.
     *
     * @param id The entry that changed.
     * @param wave Its own new, complete MindWave.
     */
    void handleMindWaveEditedWhilePreviewing(sound_mind::core::MindWaveId id, const sound_mind::core::MindWave& wave);

    MindWavesPanel* mindWavesPanel_;
    LayersPanel* layersPanel_;
    FilterConfigurationPanel* filterConfigurationPanel_;
    CanvasWidget* canvas_;
    sound_mind::core::Project* project_ = nullptr;
};

}  // namespace sound_mind::studio
