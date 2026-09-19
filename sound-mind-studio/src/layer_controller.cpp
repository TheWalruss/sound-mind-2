#include "sound_mind/studio/layer_controller.h"

#include <QMessageBox>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/mind_grain.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/filter_configuration_panel.h"
#include "sound_mind/studio/layers_panel.h"
#include "sound_mind/studio/playback_controller.h"
#include "sound_mind/studio/undo_stack.h"

namespace sound_mind::studio {

LayerController::LayerController(CanvasWidget* canvas, PlaybackController* playbackController,
                                  LayersPanel* layersPanel, FilterConfigurationPanel* filterConfigurationPanel,
                                  UndoStack* undoStack, QObject* parent)
    : QObject(parent),
      canvas_(canvas),
      playbackController_(playbackController),
      layersPanel_(layersPanel),
      filterConfigurationPanel_(filterConfigurationPanel),
      undoStack_(undoStack) {}

void LayerController::setProject(sound_mind::core::Project* project) { project_ = project; }

sound_mind::core::Layer* LayerController::topmostLayerWithContent() {
    if (project_ == nullptr) {
        return nullptr;
    }
    auto& layers = project_->layers();
    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        if (it->content().has_value() && it->visible()) {
            return &*it;
        }
    }
    return nullptr;
}

sound_mind::core::Layer* LayerController::layerById(sound_mind::core::LayerId id) {
    if (project_ == nullptr) {
        return nullptr;
    }
    return project_->layerById(id);
}

std::optional<sound_mind::core::LayerId> LayerController::paintTargetLayerId() const {
    if (project_ == nullptr || project_->layers().empty()) {
        return std::nullopt;
    }
    // A real row selection (LayersPanel's own "active layer" - see its
    // class docs) wins whenever there is one; layersPanel_->setLayers()
    // (called from refreshLayersPanel()) already drops a selection whose
    // id no longer exists in project_, so no extra validity check is
    // needed here. Falls back to the bottommost layer (Background, always
    // present) whenever nothing is selected - e.g. a project that was
    // just opened/created and never had a row clicked in it yet. Not
    // .back() - since the Equalizer milestone, that's always the
    // (locked, content-less) Equalizer layer, never a sensible paint
    // target - see this method's own docs.
    if (const auto selected = layersPanel_->selectedLayerId(); selected.has_value()) {
        return selected;
    }
    return project_->layers().front().id();
}

void LayerController::refreshLayersPanel() {
    std::vector<LayersPanel::RowData> rows;
    if (project_ != nullptr) {
        rows.reserve(project_->layers().size());
        for (const auto& layer : project_->layers()) {
            LayersPanel::RowData row;
            row.id = layer.id();
            row.name = QString::fromStdString(layer.name());
            row.type = layer.type();
            row.opacity = layer.opacity();
            row.visible = layer.visible();
            row.translationColumns = layer.translationColumns();
            row.rescaleFactor = layer.rescaleFactor();
            // Bug fix: this was previously left at RowData's own default
            // (std::nullopt) regardless of the layer's real binding, so a
            // MindWave selected in the opacity-bind combo took effect on
            // the canvas (setLayerOpacityMindWave() below did apply it)
            // but the combo itself reset to "None" on the very next
            // refresh - every mutation refreshes the panel, so this fired
            // immediately after every single bind/unbind.
            row.opacityMindWaveId = layer.opacityMindWave();
            row.blendMode = layer.blendMode();
            rows.push_back(row);
        }
    }
    layersPanel_->setLayers(rows);
}

void LayerController::applyVisibility(sound_mind::core::LayerId id, bool visible) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    layer->setVisible(visible);
    emit layersChanged();
    playbackController_->invalidate();  // "topmost layer with content" may have changed.
    canvas_->update();
    refreshLayersPanel();
}

void LayerController::toggleLayerVisibility(sound_mind::core::LayerId id, bool visible) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    const bool oldVisible = layer->visible();
    applyVisibility(id, visible);
    if (oldVisible != visible) {
        undoStack_->push({/*undo=*/[this, id, oldVisible]() { applyVisibility(id, oldVisible); },
                           /*redo=*/[this, id, visible]() { applyVisibility(id, visible); }});
    }
}

void LayerController::applyOpacity(sound_mind::core::LayerId id, float opacity) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    layer->setOpacity(opacity);
    emit layersChanged();
    canvas_->update();
    refreshLayersPanel();
}

void LayerController::setLayerOpacity(sound_mind::core::LayerId id, float opacity) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    const float oldOpacity = layer->opacity();
    applyOpacity(id, opacity);
    if (oldOpacity != opacity) {
        undoStack_->push({/*undo=*/[this, id, oldOpacity]() { applyOpacity(id, oldOpacity); },
                           /*redo=*/[this, id, opacity]() { applyOpacity(id, opacity); }});
    }
}

void LayerController::applyOpacityMindWave(sound_mind::core::LayerId id,
                                             std::optional<sound_mind::core::MindWaveId> mindWaveId) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    layer->setOpacityMindWave(mindWaveId);
    emit layersChanged();
    canvas_->update();
    refreshLayersPanel();
}

void LayerController::setLayerOpacityMindWave(sound_mind::core::LayerId id,
                                                std::optional<sound_mind::core::MindWaveId> mindWaveId) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    const std::optional<sound_mind::core::MindWaveId> oldMindWaveId = layer->opacityMindWave();
    applyOpacityMindWave(id, mindWaveId);
    if (oldMindWaveId != mindWaveId) {
        undoStack_->push(
            {/*undo=*/[this, id, oldMindWaveId]() { applyOpacityMindWave(id, oldMindWaveId); },
             /*redo=*/[this, id, mindWaveId]() { applyOpacityMindWave(id, mindWaveId); }});
    }
}

void LayerController::applyTranslation(sound_mind::core::LayerId id, std::int64_t translationColumns) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    layer->setTranslationColumns(translationColumns);
    emit layersChanged();
    canvas_->update();
    refreshLayersPanel();
}

void LayerController::setLayerTranslation(sound_mind::core::LayerId id, std::int64_t translationColumns) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    const std::int64_t oldTranslationColumns = layer->translationColumns();
    applyTranslation(id, translationColumns);
    if (oldTranslationColumns != translationColumns) {
        undoStack_->push(
            {/*undo=*/[this, id, oldTranslationColumns]() { applyTranslation(id, oldTranslationColumns); },
             /*redo=*/[this, id, translationColumns]() { applyTranslation(id, translationColumns); }});
    }
}

void LayerController::applyRescale(sound_mind::core::LayerId id, double rescaleFactor) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    layer->setRescaleFactor(rescaleFactor);
    emit layersChanged();
    canvas_->update();
    refreshLayersPanel();
}

void LayerController::setLayerRescale(sound_mind::core::LayerId id, double rescaleFactor) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    const double oldRescaleFactor = layer->rescaleFactor();
    applyRescale(id, rescaleFactor);
    if (oldRescaleFactor != rescaleFactor) {
        undoStack_->push({/*undo=*/[this, id, oldRescaleFactor]() { applyRescale(id, oldRescaleFactor); },
                           /*redo=*/[this, id, rescaleFactor]() { applyRescale(id, rescaleFactor); }});
    }
}

void LayerController::applyBlendMode(sound_mind::core::LayerId id, sound_mind::core::BlendMode mode) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    layer->setBlendMode(mode);
    emit layersChanged();
    canvas_->update();
    refreshLayersPanel();
}

void LayerController::setLayerBlendMode(sound_mind::core::LayerId id, sound_mind::core::BlendMode mode) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    const sound_mind::core::BlendMode oldMode = layer->blendMode();
    applyBlendMode(id, mode);
    if (oldMode != mode) {
        undoStack_->push({/*undo=*/[this, id, oldMode]() { applyBlendMode(id, oldMode); },
                           /*redo=*/[this, id, mode]() { applyBlendMode(id, mode); }});
    }
}

bool LayerController::renameLayerTo(sound_mind::core::LayerId id, const QString& newName) {
    if (newName.isEmpty()) {
        return false;
    }
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return false;
    }
    layer->setName(newName.toStdString());
    emit layersChanged();
    refreshLayersPanel();
    return true;
}

void LayerController::deleteLayer(sound_mind::core::LayerId id) {
    if (project_ == nullptr) {
        return;
    }
    const sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    if (sound_mind::core::isLockedLayerType(layer->type())) {
        // Defense in depth - LayersPanel doesn't even show a delete
        // button for these, but refuse here too regardless of caller.
        return;
    }

    // Refuse (with an explanatory modal) rather than silently deleting a
    // layer any active Mind Grain stroke elsewhere in the project reads
    // its own content from live - see docs/sound-mind-design.md's "Mind
    // Grains" ordering rule, and mindGrainOperationsBrokenByRemovingLayer()'s
    // own docs.
    const auto broken = sound_mind::core::mindGrainOperationsBrokenByRemovingLayer(*project_, id);
    if (!broken.empty()) {
        QMessageBox::warning(
            canvas_, tr("Can't Delete Layer"),
            tr("Deleting \"%1\" would break %2 Mind Grain stroke(s) painted elsewhere in this project - each one "
               "reads its own painted content live from this layer, and would lose its source. Remove or repaint "
               "those strokes first, or choose a different layer to delete.")
                .arg(QString::fromStdString(layer->name()))
                .arg(static_cast<int>(broken.size())));
        return;
    }

    if (project_->removeLayer(id)) {
        emit layersChanged();
        playbackController_->invalidate();
        canvas_->update();
        refreshLayersPanel();
    }
}

void LayerController::addEmptyLayer(sound_mind::codec::StreamImage placeholderContent) {
    if (project_ == nullptr) {
        return;
    }
    sound_mind::core::Layer layer(0, tr("New Layer").toStdString(), sound_mind::core::LayerType::Normal);
    // A silent, correctly-dimensioned placeholder - the same one a fresh
    // Loop Input layer gets - so there's real content to paint onto (and
    // to render/play, like any other layer) immediately, rather than
    // nothing at all until the first stroke.
    layer.setContent(std::move(placeholderContent));
    const sound_mind::core::LayerId id = project_->addLayer(std::move(layer));
    emit layersChanged();
    playbackController_->invalidate();
    canvas_->update();
    refreshLayersPanel();
    // Selected immediately - ready to paint into without an extra click,
    // the whole point of adding it in the first place.
    layersPanel_->selectLayer(id);
}

void LayerController::addFilterLayer() {
    if (project_ == nullptr) {
        return;
    }
    sound_mind::core::Layer layer(0, tr("New Filter").toStdString(), sound_mind::core::LayerType::Filter);
    const sound_mind::core::LayerId id = project_->addLayer(std::move(layer));
    emit layersChanged();
    playbackController_->invalidate();
    canvas_->update();
    refreshLayersPanel();
    // Selected immediately - ready to configure in FilterConfigurationPanel
    // without an extra click, the same reasoning addEmptyLayer()'s own
    // docs give for painting.
    layersPanel_->selectLayer(id);
}

void LayerController::handleLayerSelectionChanged(std::optional<sound_mind::core::LayerId> id) {
    const sound_mind::core::Layer* layer = id.has_value() ? layerById(*id) : nullptr;
    const bool isEqualizer = layer != nullptr && layer->type() == sound_mind::core::LayerType::Equalizer;
    const bool isFilterLayer = layer != nullptr && sound_mind::core::isFilterLayerType(layer->type());
    // Set before setFilterConfiguration() - see FilterConfigurationPanel::
    // setEqualizerMode()'s own docs (both are display-mode toggles, not
    // user edits, and updateVisibleGroup() reads isEqualizerMode_ as part
    // of loading a fresh configuration's own visible group).
    filterConfigurationPanel_->setEqualizerMode(isEqualizer);
    if (isFilterLayer) {
        filterConfigurationPanel_->setFilterConfiguration(layer->filterConfiguration());
    }
    filterConfigurationPanel_->setEnabled(isFilterLayer);
}

void LayerController::applyFilterConfiguration(const sound_mind::core::FilterConfiguration& config) {
    const auto id = layersPanel_->selectedLayerId();
    if (project_ == nullptr || !id.has_value()) {
        return;
    }
    sound_mind::core::Layer* layer = layerById(*id);
    if (layer == nullptr || !sound_mind::core::isFilterLayerType(layer->type())) {
        return;
    }
    layer->setFilterConfiguration(config);
    emit layersChanged();
    playbackController_->invalidate();
    canvas_->update();
}

void LayerController::reorderLayers(const std::vector<sound_mind::core::LayerId>& newOrderBottomToTop) {
    if (project_ == nullptr) {
        return;
    }

    // Refuse (with an explanatory modal), and cancel outright, rather than
    // applying a reorder that would put an active Mind Grain stroke's own
    // target layer at-or-below its own source - see
    // docs/sound-mind-design.md's "Mind Grains" ordering rule, and
    // mindGrainOperationsBrokenByReorder()'s own docs. The panel is still
    // refreshed below (snapping its own optimistic drag-and-drop display
    // back to the authoritative, unchanged order), the same as any other
    // rejected reorder.
    const auto broken = sound_mind::core::mindGrainOperationsBrokenByReorder(*project_, newOrderBottomToTop);
    if (!broken.empty()) {
        QMessageBox::warning(
            canvas_, tr("Can't Reorder Layers"),
            tr("This reorder would break %1 Mind Grain stroke(s) painted elsewhere in this project - each one must "
               "stay above its own source layer. Cancelled; nothing was moved.")
                .arg(static_cast<int>(broken.size())));
        refreshLayersPanel();
        return;
    }

    if (project_->reorderLayers(newOrderBottomToTop)) {
        emit layersChanged();
        canvas_->update();
    }
    // Refreshed either way - even a rejected reorder needs the panel
    // snapped back to the authoritative order (see LayersPanel::
    // reorderRequested()'s docs).
    refreshLayersPanel();
}

}  // namespace sound_mind::studio
