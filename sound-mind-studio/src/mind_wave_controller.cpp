#include "sound_mind/studio/mind_wave_controller.h"

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <QImage>

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/codec/rgb_image_resample.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/filter_configuration_panel.h"
#include "sound_mind/studio/layers_panel.h"
#include "sound_mind/studio/mind_waves_panel.h"
#include "sound_mind/studio/qt_image_conversion.h"
#include "sound_mind/studio/tool_configuration_panel.h"

namespace {

/// @brief The Layers Panel's own MindWave child-row thumbnail size, in
/// pixels (`v0.Y.44.1`, Layers Panel Redesign) - smaller than a layer's own
/// thumbnail (`layer_controller.cpp`'s own `kThumbnailWidth`/
/// `kThumbnailHeight`), matching the child row's own smaller, "tabbed-in"
/// treatment.
constexpr std::uint32_t kMindWavePreviewWidth = 120;
constexpr std::uint32_t kMindWavePreviewHeight = 24;

}  // namespace

namespace sound_mind::studio {

namespace {

using sound_mind::core::MindWave;
using sound_mind::core::MindWaveId;
using sound_mind::core::NamedMindWave;

/// @brief The smallest N such that "MindWave N" isn't already a name in
/// `entries` - addMindWave()'s own default-naming scheme. Not the same
/// approach as `Project::addMindWave()`'s own id assignment (a simple
/// "one past the max"), since a name has to be *unique*, not just fresh -
/// a MindWave could have been renamed or deleted in a way that leaves
/// gaps, and reusing a low N once it's free again reads more naturally
/// than an ever-growing counter for a UI-facing default name.
QString nextDefaultName(const std::vector<NamedMindWave>& entries) {
    int n = 1;
    while (true) {
        const QString candidate = QObject::tr("MindWave %1").arg(n);
        bool taken = false;
        for (const NamedMindWave& entry : entries) {
            if (QString::fromStdString(entry.name) == candidate) {
                taken = true;
                break;
            }
        }
        if (!taken) {
            return candidate;
        }
        ++n;
    }
}

}  // namespace

MindWaveController::MindWaveController(MindWavesPanel* mindWavesPanel, LayersPanel* layersPanel,
                                        FilterConfigurationPanel* filterConfigurationPanel,
                                        ToolConfigurationPanel* toolConfigurationPanel, CanvasWidget* canvas,
                                        QObject* parent)
    : QObject(parent),
      mindWavesPanel_(mindWavesPanel),
      layersPanel_(layersPanel),
      filterConfigurationPanel_(filterConfigurationPanel),
      toolConfigurationPanel_(toolConfigurationPanel),
      canvas_(canvas) {
    connect(mindWavesPanel_, &MindWavesPanel::previewToggled, this, &MindWaveController::updateMindWavePreview);
    connect(mindWavesPanel_, &MindWavesPanel::selectionChanged, this, &MindWaveController::updateMindWavePreview);
    connect(mindWavesPanel_, &MindWavesPanel::mindWaveChanged, this,
            &MindWaveController::handleMindWaveEditedWhilePreviewing);
    connect(mindWavesPanel_, &MindWavesPanel::visibilityChanged, this,
            &MindWaveController::handleMindWavesPanelVisibilityChanged);
}

void MindWaveController::setProject(sound_mind::core::Project* project) {
    project_ = project;
    mindWavesPanel_->setPreviewEnabled(false);
    canvas_->setMindWavePreview(std::nullopt);
}

void MindWaveController::handleMindWavesPanelVisibilityChanged(bool visible) {
    if (visible) {
        return;
    }
    mindWavesPanel_->setPreviewEnabled(false);
    canvas_->setMindWavePreview(std::nullopt);
}

void MindWaveController::updateMindWavePreview() {
    if (!mindWavesPanel_->previewEnabled() || project_ == nullptr) {
        canvas_->setMindWavePreview(std::nullopt);
        return;
    }
    const auto selectedId = mindWavesPanel_->selectedMindWaveId();
    if (!selectedId.has_value()) {
        canvas_->setMindWavePreview(std::nullopt);
        return;
    }
    const NamedMindWave* entry = project_->mindWaveById(*selectedId);
    canvas_->setMindWavePreview(entry != nullptr ? std::optional<MindWave>(entry->wave) : std::nullopt);
}

void MindWaveController::handleMindWaveEditedWhilePreviewing(MindWaveId id, const MindWave& wave) {
    if (!mindWavesPanel_->previewEnabled()) {
        return;
    }
    if (mindWavesPanel_->selectedMindWaveId() != std::optional<MindWaveId>(id)) {
        return;  // Defensive - mindWaveChanged() never fires for a non-selected entry (see its own docs).
    }
    canvas_->setMindWavePreview(wave);
}

void MindWaveController::refreshMindWavesPanel() {
    std::vector<MindWavesPanel::RowData> rows;
    std::vector<std::pair<MindWaveId, QString>> availableForBinding;
    std::map<MindWaveId, QImage> previewImages;
    if (project_ != nullptr) {
        const auto config = sound_mind::core::streamCodecConfigFor(project_->settings());
        for (const NamedMindWave& entry : project_->mindWaves()) {
            const QString name = QString::fromStdString(entry.name);
            rows.push_back(MindWavesPanel::RowData{entry.id, name, entry.wave});
            availableForBinding.emplace_back(entry.id, name);

            // The Layers Panel's own MindWave child-row preview
            // (`v0.Y.44.1`, Layers Panel Redesign) - the same evaluate-
            // then-grayscale pipeline the canvas's own live MindWave
            // Preview overlay uses (see CanvasWidget::setMindWavePreview()),
            // shrunk to thumbnail size via the shared area-averaging
            // downsample (correct here for the same reason it's correct
            // for a layer's own thumbnail - see renderLayerThumbnail()'s
            // own docs).
            const auto field =
                sound_mind::core::evaluateMindWaveField(entry.wave, config, project_->settings().canvasWidth);
            const auto grayscale =
                sound_mind::codec::toGrayscaleImage(field, project_->settings().canvasWidth, config.binCount);
            const auto downsampled =
                sound_mind::codec::downsampleAveraged(grayscale, kMindWavePreviewWidth, kMindWavePreviewHeight);
            previewImages[entry.id] = toQImageView(downsampled).copy();
        }
    }
    mindWavesPanel_->setMindWaves(rows);
    layersPanel_->setAvailableMindWaves(availableForBinding);
    layersPanel_->setMindWavePreviewImages(previewImages);
    filterConfigurationPanel_->setAvailableMindWaves(availableForBinding);
    toolConfigurationPanel_->setAvailableMindWaves(availableForBinding);
}

void MindWaveController::addMindWave() {
    if (project_ == nullptr) {
        return;
    }
    const QString name = nextDefaultName(project_->mindWaves());
    const MindWaveId id = project_->addMindWave(name.toStdString(), MindWave{});
    emit mindWavesChanged();
    refreshMindWavesPanel();
    mindWavesPanel_->selectMindWave(id);
}

void MindWaveController::removeMindWave(MindWaveId id) {
    if (project_ == nullptr || !project_->removeMindWave(id)) {
        return;
    }
    emit mindWavesChanged();
    refreshMindWavesPanel();
}

bool MindWaveController::renameMindWaveTo(MindWaveId id, const QString& newName) {
    if (project_ == nullptr || newName.isEmpty()) {
        return false;
    }
    NamedMindWave* entry = project_->mindWaveById(id);
    if (entry == nullptr) {
        return false;
    }
    entry->name = newName.toStdString();
    emit mindWavesChanged();
    refreshMindWavesPanel();
    return true;
}

void MindWaveController::updateMindWave(MindWaveId id, const MindWave& wave) {
    if (project_ == nullptr) {
        return;
    }
    NamedMindWave* entry = project_->mindWaveById(id);
    if (entry == nullptr) {
        return;
    }
    entry->wave = wave;
    emit mindWavesChanged();
    refreshMindWavesPanel();
}

void MindWaveController::setDrawnPath(MindWaveId id, const sound_mind::core::Path& path) {
    if (project_ == nullptr) {
        return;
    }
    NamedMindWave* entry = project_->mindWaveById(id);
    if (entry == nullptr) {
        return;
    }
    entry->wave.setType(sound_mind::core::GeneratorType::Drawn);
    entry->wave.setDrawnPath(path);
    emit mindWavesChanged();
    refreshMindWavesPanel();
}

}  // namespace sound_mind::studio
