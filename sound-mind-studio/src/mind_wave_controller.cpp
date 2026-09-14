#include "sound_mind/studio/mind_wave_controller.h"

#include <utility>
#include <vector>

#include "sound_mind/studio/filter_configuration_panel.h"
#include "sound_mind/studio/layers_panel.h"
#include "sound_mind/studio/mind_waves_panel.h"

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
                                        FilterConfigurationPanel* filterConfigurationPanel, QObject* parent)
    : QObject(parent),
      mindWavesPanel_(mindWavesPanel),
      layersPanel_(layersPanel),
      filterConfigurationPanel_(filterConfigurationPanel) {}

void MindWaveController::setProject(sound_mind::core::Project* project) { project_ = project; }

void MindWaveController::refreshMindWavesPanel() {
    std::vector<MindWavesPanel::RowData> rows;
    std::vector<std::pair<MindWaveId, QString>> availableForBinding;
    if (project_ != nullptr) {
        for (const NamedMindWave& entry : project_->mindWaves()) {
            const QString name = QString::fromStdString(entry.name);
            rows.push_back(MindWavesPanel::RowData{entry.id, name, entry.wave});
            availableForBinding.emplace_back(entry.id, name);
        }
    }
    mindWavesPanel_->setMindWaves(rows);
    layersPanel_->setAvailableMindWaves(availableForBinding);
    filterConfigurationPanel_->setAvailableMindWaves(availableForBinding);
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

}  // namespace sound_mind::studio
