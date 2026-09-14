#pragma once

#include <QObject>
#include <QString>

#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/project.h"

namespace sound_mind::studio {

class LayersPanel;
class MindWavesPanel;

/**
 * @brief Owns the current project's MindWave library - add/remove/rename/
 *        edit - and keeps `MindWavesPanel`/`LayersPanel` in sync with it.
 *        `v0.Y.31.1` (MindWaves v1) Installment C2's own controller, the
 *        same "own presentation" treatment `LayerController`/
 *        `ToolPaletteController` already received.
 *
 * Constructed with non-owning pointers to `mindWavesPanel` and
 * `layersPanel` - both stay `MainWindow`-owned (dock widgets, like
 * `LayersPanel`/`FilterConfigurationPanel` already are for
 * `LayerController`) - and pushes the current library into both of them
 * after any mutation: `mindWavesPanel` gets the full library (name,
 * generator type, parameters); `layersPanel` gets only id/name pairs, for
 * its own per-row opacity-binding combo (`LayersPanel::
 * setAvailableMindWaves()`).
 *
 * **Deliberately does not own `Layer::opacityMindWave()`'s own mutation** -
 * binding a *layer* to a MindWave is a layer mutation, not a MindWave-
 * library one, so it stays `LayerController::setLayerOpacityMindWave()`'s
 * job; this class only keeps `layersPanel`'s own combo populated with
 * *which* MindWaves exist to bind to.
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
     * @param parent The owning object, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    MindWaveController(MindWavesPanel* mindWavesPanel, LayersPanel* layersPanel, QObject* parent = nullptr);

    /// @brief Sets which project this controller looks up/mutates
    ///        MindWaves in. Does *not* itself refresh the panels -
    ///        `MainWindow`'s own `setProject()` calls
    ///        `refreshMindWavesPanel()` separately, matching
    ///        `LayerController::setProject()`'s own docs.
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
    MindWavesPanel* mindWavesPanel_;
    LayersPanel* layersPanel_;
    sound_mind::core::Project* project_ = nullptr;
};

}  // namespace sound_mind::studio
