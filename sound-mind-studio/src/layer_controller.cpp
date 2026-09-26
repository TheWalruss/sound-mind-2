#include "sound_mind/studio/layer_controller.h"

#include <algorithm>

#include <QMessageBox>

#include "sound_mind/core/compositor.h"
#include "sound_mind/core/generator.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/mind_grain.h"
#include "sound_mind/core/phase_cleanup.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/filter_configuration_panel.h"
#include "sound_mind/studio/layers_panel.h"
#include "sound_mind/studio/playback_controller.h"
#include "sound_mind/studio/qt_image_conversion.h"
#include "sound_mind/studio/undo_stack.h"

namespace {

/// @brief The Layers Panel's own thumbnail size, in pixels
/// (`v0.Y.44.1`, Layers Panel Redesign) - wide enough to read as a
/// recognizable spectrogram shape at a glance once stretched across a
/// row's own width (`LayerRowWidget`'s own `QLabel::setScaledContents()`
/// does that stretch at display time), tall enough to be more than a
/// sliver at the row header's own fixed height.
constexpr std::uint32_t kThumbnailWidth = 160;
constexpr std::uint32_t kThumbnailHeight = 32;

}  // namespace

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

void LayerController::setProject(sound_mind::core::Project* project) {
    project_ = project;
    // See pendingFilterConfiguration()'s own docs - a fresh default for
    // whatever new/opened project this now is, not carried over from
    // whatever project (if any) was previously set.
    pendingFilterConfiguration_ = sound_mind::core::FilterConfiguration{};
    // Re-syncs filterConfigurationPanel_ against the *new* project_ -
    // MainWindow's own real caller already clears layersPanel_'s selection
    // before calling this, but whatever earlier handleLayerSelectionChanged()
    // call that triggered only ever saw the *old* project_ (or none at all,
    // for the very first project a session ever creates), so the panel's
    // enabled state needs re-checking here rather than being left stale.
    handleLayerSelectionChanged(std::nullopt);
}

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

QImage LayerController::thumbnailFor(const sound_mind::core::Layer& layer) {
    const auto cached = thumbnailCache_.find(layer.id());
    if (cached != thumbnailCache_.end()) {
        return cached->second;
    }
    const auto rendered = sound_mind::core::renderLayerThumbnail(layer, kThumbnailWidth, kThumbnailHeight);
    if (!rendered.has_value()) {
        return QImage();  // No content yet - never cached, see this method's own docs.
    }
    const QImage thumbnail = toQImageView(*rendered).copy();
    thumbnailCache_[layer.id()] = thumbnail;
    return thumbnail;
}

void LayerController::refreshLayersPanel(std::optional<sound_mind::core::LayerId> changedContentLayer) {
    if (changedContentLayer.has_value()) {
        thumbnailCache_.erase(*changedContentLayer);
    }

    std::vector<LayersPanel::RowData> rows;
    if (project_ != nullptr) {
        rows.reserve(project_->layers().size());
        for (const auto& layer : project_->layers()) {
            LayersPanel::RowData row;
            row.id = layer.id();
            row.name = QString::fromStdString(layer.name());
            row.type = layer.type();
            row.opacity = layer.opacity();
            row.balance = layer.balance();
            row.visible = layer.visible();
            row.muted = layer.muted();
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
            row.thumbnail = thumbnailFor(layer);
            rows.push_back(row);
        }
    }
    layersPanel_->setLayers(rows);
}

void LayerController::applyVisibilityAndMute(sound_mind::core::LayerId id, bool visible, bool muted) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    layer->setVisible(visible);
    layer->setMuted(muted);
    emit layersChanged();
    playbackController_->invalidate();  // "topmost layer with content" may have changed.
    canvas_->update();
    refreshLayersPanel();
}

void LayerController::cycleLayerVisibilityState(sound_mind::core::LayerId id) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    const bool oldVisible = layer->visible();
    const bool oldMuted = layer->muted();

    bool newVisible;
    bool newMuted;
    if (oldVisible && !oldMuted) {
        newVisible = true;
        newMuted = true;  // Visible -> Muted.
    } else if (oldVisible && oldMuted) {
        newVisible = false;
        newMuted = false;  // Muted -> Invisible.
    } else {
        newVisible = true;
        newMuted = false;  // Invisible -> Visible.
    }

    applyVisibilityAndMute(id, newVisible, newMuted);
    undoStack_->push(
        {/*undo=*/[this, id, oldVisible, oldMuted]() { applyVisibilityAndMute(id, oldVisible, oldMuted); },
         /*redo=*/[this, id, newVisible, newMuted]() { applyVisibilityAndMute(id, newVisible, newMuted); },
         /*description=*/tr("Changed layer visibility")});
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
                           /*redo=*/[this, id, opacity]() { applyOpacity(id, opacity); },
                           /*description=*/tr("Changed layer opacity")});
    }
}

void LayerController::applyBalance(sound_mind::core::LayerId id, float balance) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    layer->setBalance(balance);
    emit layersChanged();
    canvas_->update();
    refreshLayersPanel();
}

void LayerController::setLayerBalance(sound_mind::core::LayerId id, float balance) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    const float oldBalance = layer->balance();
    applyBalance(id, balance);
    if (oldBalance != balance) {
        undoStack_->push({/*undo=*/[this, id, oldBalance]() { applyBalance(id, oldBalance); },
                           /*redo=*/[this, id, balance]() { applyBalance(id, balance); },
                           /*description=*/tr("Changed layer balance")});
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
             /*redo=*/[this, id, mindWaveId]() { applyOpacityMindWave(id, mindWaveId); },
             /*description=*/tr("Changed layer opacity MindWave binding")});
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
             /*redo=*/[this, id, translationColumns]() { applyTranslation(id, translationColumns); },
             /*description=*/tr("Changed layer translation")});
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
                           /*redo=*/[this, id, rescaleFactor]() { applyRescale(id, rescaleFactor); },
                           /*description=*/tr("Changed layer rescale")});
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
                           /*redo=*/[this, id, mode]() { applyBlendMode(id, mode); },
                           /*description=*/tr("Changed layer blend mode")});
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
    layer->setName(project_->uniqueLayerName(newName.toStdString(), id));
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
        thumbnailCache_.erase(id);
        emit layersChanged();
        playbackController_->invalidate();
        canvas_->update();
        refreshLayersPanel();
    }
}

void LayerController::duplicateLayer(sound_mind::core::LayerId id) {
    if (project_ == nullptr) {
        return;
    }
    const sound_mind::core::Layer* source = layerById(id);
    if (source == nullptr) {
        return;
    }
    if (sound_mind::core::isLockedLayerType(source->type())) {
        // Defense in depth - LayersPanel doesn't even show a duplicate
        // button for these, but refuse here too regardless of caller.
        return;
    }

    sound_mind::core::Layer copy(*source);
    const sound_mind::core::LayerId newId = project_->addLayer(std::move(copy));
    emit layersChanged();
    playbackController_->invalidate();
    canvas_->update();
    refreshLayersPanel();
    layersPanel_->selectLayer(newId);
}

void LayerController::cleanUpLayerPhase(sound_mind::core::LayerId id) {
    if (project_ == nullptr) {
        return;
    }
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr || !layer->content().has_value()) {
        // Defense in depth - LayersPanel doesn't even show this button for
        // a layer with no content (see the header's own docs), but refuse
        // here too regardless of caller.
        return;
    }

    sound_mind::codec::StreamImage content = *layer->content();
    sound_mind::core::applyPhaseCleanup(content);
    layer->setContent(std::move(content));

    emit layersChanged();
    playbackController_->invalidate();
    canvas_->update();
    refreshLayersPanel(id);
}

std::optional<std::size_t> LayerController::selectedLayerIndex() const {
    if (project_ == nullptr) {
        return std::nullopt;
    }
    const auto selected = layersPanel_->selectedLayerId();
    if (!selected.has_value()) {
        return std::nullopt;
    }
    const auto& layers = project_->layers();
    const auto it = std::find_if(layers.begin(), layers.end(),
                                  [&](const sound_mind::core::Layer& layer) { return layer.id() == *selected; });
    if (it == layers.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(layers.begin(), it));
}

void LayerController::selectLayerAbove() {
    const auto index = selectedLayerIndex();
    if (!index.has_value()) {
        return;
    }
    const auto& layers = project_->layers();
    if (*index + 1 >= layers.size()) {
        return;  // Already topmost.
    }
    layersPanel_->selectLayer(layers[*index + 1].id());
}

void LayerController::selectLayerBelow() {
    const auto index = selectedLayerIndex();
    if (!index.has_value() || *index == 0) {
        return;  // Nothing selected, or already bottommost.
    }
    layersPanel_->selectLayer(project_->layers()[*index - 1].id());
}

void LayerController::moveSelectedLayerUp() {
    const auto index = selectedLayerIndex();
    if (!index.has_value()) {
        return;
    }
    const auto& layers = project_->layers();
    if (*index + 1 >= layers.size()) {
        return;  // Already topmost.
    }
    if (sound_mind::core::isLockedLayerType(layers[*index].type()) ||
        sound_mind::core::isLockedLayerType(layers[*index + 1].type())) {
        // Moving would displace a locked layer from its fixed position -
        // the same invariant LayersPanel's own drag-and-drop reorder
        // already enforces.
        return;
    }

    std::vector<sound_mind::core::LayerId> newOrder;
    newOrder.reserve(layers.size());
    for (const auto& layer : layers) {
        newOrder.push_back(layer.id());
    }
    std::swap(newOrder[*index], newOrder[*index + 1]);
    reorderLayers(newOrder);
}

void LayerController::moveSelectedLayerDown() {
    const auto index = selectedLayerIndex();
    if (!index.has_value() || *index == 0) {
        return;  // Nothing selected, or already bottommost.
    }
    const auto& layers = project_->layers();
    if (sound_mind::core::isLockedLayerType(layers[*index].type()) ||
        sound_mind::core::isLockedLayerType(layers[*index - 1].type())) {
        return;  // Same locked-neighbor guard as moveSelectedLayerUp().
    }

    std::vector<sound_mind::core::LayerId> newOrder;
    newOrder.reserve(layers.size());
    for (const auto& layer : layers) {
        newOrder.push_back(layer.id());
    }
    std::swap(newOrder[*index], newOrder[*index - 1]);
    reorderLayers(newOrder);
}

void LayerController::nudgeSelectedLayerOpacity(float delta) {
    const auto index = selectedLayerIndex();
    if (!index.has_value()) {
        return;
    }
    const sound_mind::core::Layer& layer = project_->layers()[*index];
    const float newOpacity = std::clamp(layer.opacity() + delta, 0.0f, 1.0f);
    setLayerOpacity(layer.id(), newOpacity);
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
    // Seeded from whatever's pending (finding #13) - a fresh default the
    // very first time, or whatever the user already dialed in beforehand.
    layer.setFilterConfiguration(pendingFilterConfiguration_);
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

void LayerController::addGeneratedLayer(const sound_mind::core::GeneratorConfiguration& config) {
    if (project_ == nullptr) {
        return;
    }
    sound_mind::core::Layer layer(0, tr("New Generated Layer").toStdString(), sound_mind::core::LayerType::Normal);
    layer.setContent(sound_mind::core::generateContent(config, project_->settings()));
    const sound_mind::core::LayerId id = project_->addLayer(std::move(layer));
    emit layersChanged();
    playbackController_->invalidate();
    canvas_->update();
    refreshLayersPanel();
    // Selected immediately - the same "ready to work with, no extra
    // click" reasoning addEmptyLayer()'s/addFilterLayer()'s own docs
    // give.
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
    // Finding #13: the panel always shows *something* editable, whether
    // that's a real Filter/Equalizer layer's own configuration, or (if the
    // selection isn't one) pendingFilterConfiguration_ - never disabled
    // purely for lack of a Filter layer being selected.
    filterConfigurationPanel_->setFilterConfiguration(isFilterLayer ? layer->filterConfiguration()
                                                                     : pendingFilterConfiguration_);
    filterConfigurationPanel_->setEnabled(project_ != nullptr);
}

void LayerController::applyFilterConfiguration(const sound_mind::core::FilterConfiguration& config) {
    if (project_ == nullptr) {
        return;
    }
    const auto id = layersPanel_->selectedLayerId();
    sound_mind::core::Layer* layer = id.has_value() ? layerById(*id) : nullptr;
    if (layer != nullptr && sound_mind::core::isFilterLayerType(layer->type())) {
        const sound_mind::core::FilterConfiguration oldConfig = layer->filterConfiguration();
        applyFilterConfigurationTo(*id, config);
        undoStack_->push(
            {/*undo=*/[this, layerId = *id, oldConfig]() { applyFilterConfigurationTo(layerId, oldConfig); },
             /*redo=*/[this, layerId = *id, config]() { applyFilterConfigurationTo(layerId, config); },
             /*description=*/tr("Changed filter configuration")});
        return;
    }
    // Finding #13: nothing (or a non-Filter layer) is selected - the edit
    // isn't discarded, it seeds whatever Filter layer addFilterLayer()
    // adds next. Not undoable, same reasoning as every other pending seed
    // value in this class - nothing real has changed yet.
    pendingFilterConfiguration_ = config;
}

void LayerController::applyFilterConfigurationTo(sound_mind::core::LayerId id,
                                                   const sound_mind::core::FilterConfiguration& config) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
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
