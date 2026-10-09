#include "sound_mind/studio/resource_browser_controller.h"

#include <algorithm>
#include <filesystem>

#include <QFileDialog>
#include <QImage>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QStringList>
#include <QWidget>

#include <nlohmann/json.hpp>

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/core/mind_shot_preview.h"
#include "sound_mind/core/resource_file.h"
#include "sound_mind/studio/qt_image_conversion.h"
#include "sound_mind/studio/resource_browser_panel.h"

namespace sound_mind::studio {

namespace {

using sound_mind::core::ConvolutionKernelId;
using sound_mind::core::MindGrainId;
using sound_mind::core::MindShotId;
using sound_mind::core::MindWaveId;
using sound_mind::core::Project;
using sound_mind::core::ResonanceProfileId;
using sound_mind::core::ToolPresetId;

QString toolTypeLabel(sound_mind::core::ToolType type) {
    switch (type) {
        case sound_mind::core::ToolType::Procedural:
            return QObject::tr("Procedural");
        case sound_mind::core::ToolType::Instrument:
            return QObject::tr("Instrument");
        case sound_mind::core::ToolType::Resonance:
            return QObject::tr("Resonance");
        case sound_mind::core::ToolType::MindShot:
            return QObject::tr("Mind Shot");
        case sound_mind::core::ToolType::MindGrain:
            return QObject::tr("Mind Grain");
        case sound_mind::core::ToolType::Smudge:
            return QObject::tr("Smudge");
        case sound_mind::core::ToolType::OrderChaos:
            return QObject::tr("Order/Chaos");
        case sound_mind::core::ToolType::Heal:
            return QObject::tr("Heal");
        case sound_mind::core::ToolType::Soften:
            return QObject::tr("Soften");
        case sound_mind::core::ToolType::Clone:
            return QObject::tr("Clone");
    }
    return QString();
}

/// @brief A small line-chart rendering of a Resonance Profile's own
/// spectrum - every value already in `[0, 1]` (see
/// `NamedResonanceProfile::spectrum`'s own docs), so no normalization is
/// needed before plotting. The nearest thing to "spectral distribution"
/// a 1-D spectrum has, distinct from a Mind Shot's own 2-D spectrogram
/// raster.
QImage renderSpectrumImage(const std::vector<float>& spectrum) {
    constexpr int kWidth = 240;
    constexpr int kHeight = 80;
    QImage image(kWidth, kHeight, QImage::Format_RGB888);
    image.fill(Qt::black);
    if (spectrum.empty()) {
        return image;
    }

    QPainter painter(&image);
    painter.setPen(Qt::green);
    const double stepX = static_cast<double>(kWidth) / static_cast<double>(spectrum.size());
    for (std::size_t i = 0; i < spectrum.size(); ++i) {
        const double x = static_cast<double>(i) * stepX;
        const double value = std::clamp(spectrum[i], 0.0f, 1.0f);
        const double barHeight = value * kHeight;
        painter.drawLine(QPointF(x, kHeight), QPointF(x, kHeight - barHeight));
    }
    return image;
}

/// @brief The portable-file extension filter string for a `QFileDialog`,
/// e.g. `"MindWave Files (*.smwave)"`.
QString fileFilterFor(sound_mind::core::PortableResourceType type) {
    const QString extension = QString::fromUtf8(sound_mind::core::portableResourceFileExtension(type).data());
    return QObject::tr("Sound Mind Files (*%1)").arg(extension);
}

std::optional<sound_mind::core::PortableResourceType> portableTypeFor(ResourceCategory category) {
    switch (category) {
        case ResourceCategory::MindWave:
            return sound_mind::core::PortableResourceType::MindWave;
        case ResourceCategory::ToolPreset:
            return sound_mind::core::PortableResourceType::ToolPreset;
        case ResourceCategory::MindShot:
            return sound_mind::core::PortableResourceType::MindShot;
        case ResourceCategory::ResonanceProfile:
            return sound_mind::core::PortableResourceType::ResonanceProfile;
        case ResourceCategory::ConvolutionKernel:
            return sound_mind::core::PortableResourceType::ConvolutionKernel;
        case ResourceCategory::MindGrain:
        case ResourceCategory::Layer:
            return std::nullopt;
    }
    return std::nullopt;
}

}  // namespace

ResourceBrowserController::ResourceBrowserController(ResourceBrowserPanel* panel, QObject* parent)
    : QObject(parent), panel_(panel) {
    connect(panel_, &ResourceBrowserPanel::categoryChanged, this, [this](ResourceCategory) { handleCategoryChanged(); });
    connect(panel_, &ResourceBrowserPanel::entrySelected, this, &ResourceBrowserController::handleEntrySelected);
    connect(panel_, &ResourceBrowserPanel::exportRequested, this, &ResourceBrowserController::handleExportRequested);
    connect(panel_, &ResourceBrowserPanel::importFromFileRequested, this,
            &ResourceBrowserController::handleImportFromFileRequested);
    connect(panel_, &ResourceBrowserPanel::importEntryRequested, this,
            &ResourceBrowserController::handleImportEntryRequested);
    connect(panel_, &ResourceBrowserPanel::browseOtherProjectRequested, this,
            &ResourceBrowserController::handleBrowseOtherProjectRequested);
    connect(panel_, &ResourceBrowserPanel::returnToCurrentProjectRequested, this,
            &ResourceBrowserController::handleReturnToCurrentProjectRequested);
    connect(panel_, &ResourceBrowserPanel::playRequested, this, &ResourceBrowserController::handlePlayRequested);
    connect(panel_, &ResourceBrowserPanel::stopRequested, this, &ResourceBrowserController::handleStopRequested);
    connect(panel_, &ResourceBrowserPanel::addToToolkitRequested, this,
            &ResourceBrowserController::handleAddToToolkitRequested);
    connect(panel_, &ResourceBrowserPanel::removeFromToolkitRequested, this,
            &ResourceBrowserController::handleRemoveFromToolkitRequested);
    connect(panel_, &ResourceBrowserPanel::exportToolkitRequested, this,
            &ResourceBrowserController::handleExportToolkitRequested);
    connect(panel_, &ResourceBrowserPanel::importToolkitRequested, this,
            &ResourceBrowserController::handleImportToolkitRequested);
}

void ResourceBrowserController::setProject(Project* project) {
    project_ = project;
    browsedProject_ = std::nullopt;
    browsedProjectPath_ = std::nullopt;
    previewEngine_.stop();
}

const Project* ResourceBrowserController::activeProject() const {
    return browsedProject_ ? &*browsedProject_ : project_;
}

void ResourceBrowserController::refreshPanel() {
    panel_->setBrowsingOtherProject(browsedProjectPath_.value_or(QString()));
    panel_->setImportFromFileEnabled(portableTypeFor(panel_->selectedCategory()).has_value());
    refreshEntries();
}

void ResourceBrowserController::refreshEntries() {
    std::vector<ResourceBrowserPanel::RowData> rows;
    const Project* project = activeProject();
    if (project != nullptr) {
        switch (panel_->selectedCategory()) {
            case ResourceCategory::MindWave:
                for (const auto& entry : project->mindWaves()) {
                    rows.push_back({entry.id, QString::fromStdString(entry.name)});
                }
                break;
            case ResourceCategory::ToolPreset:
                for (const auto& entry : project->toolPresets()) {
                    const QString typeLabel = entry.config ? toolTypeLabel(entry.config->type()) : QString();
                    rows.push_back(
                        {entry.id, QObject::tr("%1 (%2)").arg(QString::fromStdString(entry.name), typeLabel)});
                }
                break;
            case ResourceCategory::MindShot:
                for (const auto& entry : project->mindShots()) {
                    rows.push_back({entry.id, QString::fromStdString(entry.name)});
                }
                break;
            case ResourceCategory::ResonanceProfile:
                for (const auto& entry : project->resonanceProfiles()) {
                    rows.push_back({entry.id, QString::fromStdString(entry.name)});
                }
                break;
            case ResourceCategory::ConvolutionKernel:
                for (const auto& entry : project->convolutionKernels()) {
                    rows.push_back({entry.id, QString::fromStdString(entry.name)});
                }
                break;
            case ResourceCategory::MindGrain:
                for (const auto& entry : project->mindGrains()) {
                    rows.push_back({entry.id, QString::fromStdString(entry.name)});
                }
                break;
            case ResourceCategory::Layer:
                for (const auto& layer : project->layers()) {
                    rows.push_back({layer.id(), QString::fromStdString(layer.name())});
                }
                break;
        }
    }
    panel_->setEntries(rows);
}

void ResourceBrowserController::refreshInspector() {
    const Project* project = activeProject();
    const std::optional<std::uint64_t> id = panel_->selectedEntryId();
    panel_->setAddToToolkitEnabled(id.has_value() && portableTypeFor(panel_->selectedCategory()).has_value());
    if (project == nullptr || !id.has_value()) {
        panel_->clearInspector();
        return;
    }

    const bool browsingOther = browsedProject_.has_value();
    QString name;
    QString parameterText;
    QImage raster;
    bool canPlay = false;
    const bool canExport = portableTypeFor(panel_->selectedCategory()).has_value();
    const bool canImportEntry = browsingOther && panel_->selectedCategory() != ResourceCategory::MindGrain;

    switch (panel_->selectedCategory()) {
        case ResourceCategory::MindWave: {
            const auto* entry = project->mindWaveById(static_cast<MindWaveId>(*id));
            if (entry == nullptr) {
                panel_->clearInspector();
                return;
            }
            name = QString::fromStdString(entry->name);
            parameterText = QString::fromStdString(nlohmann::json(entry->wave).dump(2));
            break;
        }
        case ResourceCategory::ToolPreset: {
            const auto* entry = project->toolPresetById(static_cast<ToolPresetId>(*id));
            if (entry == nullptr || !entry->config) {
                panel_->clearInspector();
                return;
            }
            name = QString::fromStdString(entry->name);
            nlohmann::json json;
            sound_mind::core::to_json(json, *entry->config);
            parameterText = QString::fromStdString(json.dump(2));
            break;
        }
        case ResourceCategory::MindShot: {
            const auto* entry = project->mindShotById(static_cast<MindShotId>(*id));
            if (entry == nullptr) {
                panel_->clearInspector();
                return;
            }
            name = QString::fromStdString(entry->name);
            parameterText =
                QObject::tr("fundamentalFrequencyHz: %1\nstartTimeOffsetSeconds: %2\nframeCount: %3\nbinCount: %4")
                    .arg(entry->fundamentalFrequencyHz)
                    .arg(entry->startTimeOffsetSeconds)
                    .arg(entry->clip.frameCount)
                    .arg(entry->clip.binCount);

            const auto config = sound_mind::core::streamCodecConfigFor(project->settings());
            const auto image = sound_mind::core::streamImageFromClip(entry->clip, config);
            raster = toQImageView(sound_mind::codec::toRgbImage(image)).copy();
            currentPreviewAudio_ = sound_mind::codec::decode(image);
            canPlay = true;
            break;
        }
        case ResourceCategory::ResonanceProfile: {
            const auto* entry = project->resonanceProfileById(static_cast<ResonanceProfileId>(*id));
            if (entry == nullptr) {
                panel_->clearInspector();
                return;
            }
            name = QString::fromStdString(entry->name);
            parameterText = QObject::tr("spectrum: %1 values").arg(entry->spectrum.size());
            raster = renderSpectrumImage(entry->spectrum);
            break;
        }
        case ResourceCategory::ConvolutionKernel: {
            const auto* entry = project->convolutionKernelById(static_cast<ConvolutionKernelId>(*id));
            if (entry == nullptr) {
                panel_->clearInspector();
                return;
            }
            name = QString::fromStdString(entry->name);
            nlohmann::json json;
            sound_mind::core::to_json(json, *entry);
            parameterText = QString::fromStdString(json.dump(2));
            break;
        }
        case ResourceCategory::MindGrain: {
            const auto* entry = project->mindGrainById(static_cast<MindGrainId>(*id));
            if (entry == nullptr) {
                panel_->clearInspector();
                return;
            }
            name = QString::fromStdString(entry->name);
            nlohmann::json json;
            sound_mind::core::to_json(json, *entry);
            parameterText = QString::fromStdString(json.dump(2));
            break;
        }
        case ResourceCategory::Layer: {
            const auto* layer = project->layerById(static_cast<sound_mind::core::LayerId>(*id));
            if (layer == nullptr) {
                panel_->clearInspector();
                return;
            }
            name = QString::fromStdString(layer->name());
            nlohmann::json json;
            sound_mind::core::to_json(json, *layer);
            parameterText = QString::fromStdString(json.dump(2));
            break;
        }
    }

    panel_->setInspector(name, parameterText, raster, canPlay, canExport, canImportEntry);
}

void ResourceBrowserController::handleCategoryChanged() {
    previewEngine_.stop();
    panel_->setImportFromFileEnabled(portableTypeFor(panel_->selectedCategory()).has_value());
    refreshEntries();
}

void ResourceBrowserController::handleEntrySelected(std::optional<std::uint64_t>) {
    previewEngine_.stop();
    refreshInspector();
}

bool ResourceBrowserController::exportSelectedEntry(const std::filesystem::path& path, QString* errorMessage) {
    const Project* project = activeProject();
    const std::optional<std::uint64_t> id = panel_->selectedEntryId();
    const auto portableType = portableTypeFor(panel_->selectedCategory());
    if (project == nullptr || !id.has_value() || !portableType.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Nothing exportable is selected.");
        }
        return false;
    }

    try {
        switch (panel_->selectedCategory()) {
            case ResourceCategory::MindWave:
                sound_mind::core::exportMindWave(*project->mindWaveById(static_cast<MindWaveId>(*id)), path);
                break;
            case ResourceCategory::ToolPreset:
                sound_mind::core::exportToolPreset(*project->toolPresetById(static_cast<ToolPresetId>(*id)), path);
                break;
            case ResourceCategory::MindShot:
                sound_mind::core::exportMindShot(*project->mindShotById(static_cast<MindShotId>(*id)), path);
                break;
            case ResourceCategory::ResonanceProfile:
                sound_mind::core::exportResonanceProfile(
                    *project->resonanceProfileById(static_cast<ResonanceProfileId>(*id)), path);
                break;
            case ResourceCategory::ConvolutionKernel:
                sound_mind::core::exportConvolutionKernel(
                    *project->convolutionKernelById(static_cast<ConvolutionKernelId>(*id)), path);
                break;
            case ResourceCategory::MindGrain:
            case ResourceCategory::Layer:
                return false;
        }
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
    return true;
}

bool ResourceBrowserController::importFromFile(const std::filesystem::path& path, QString* errorMessage) {
    if (project_ == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("No project is open.");
        }
        return false;
    }
    const auto portableType = portableTypeFor(panel_->selectedCategory());
    if (!portableType.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("This category has no portable file format.");
        }
        return false;
    }

    try {
        switch (panel_->selectedCategory()) {
            case ResourceCategory::MindWave: {
                const auto entry = sound_mind::core::importMindWave(path);
                project_->addMindWave(entry.name, entry.wave);
                break;
            }
            case ResourceCategory::ToolPreset: {
                const auto entry = sound_mind::core::importToolPreset(path);
                project_->addToolPreset(entry.name, *entry.config);
                break;
            }
            case ResourceCategory::MindShot: {
                const auto entry = sound_mind::core::importMindShot(path);
                const MindShotId newId = project_->addMindShot(entry.name, entry.clip);
                if (auto* added = project_->mindShotById(newId)) {
                    added->fundamentalFrequencyHz = entry.fundamentalFrequencyHz;
                    added->startTimeOffsetSeconds = entry.startTimeOffsetSeconds;
                }
                break;
            }
            case ResourceCategory::ResonanceProfile: {
                const auto entry = sound_mind::core::importResonanceProfile(path);
                project_->addResonanceProfile(entry.name, entry.spectrum);
                break;
            }
            case ResourceCategory::ConvolutionKernel: {
                const auto entry = sound_mind::core::importConvolutionKernel(path);
                project_->addConvolutionKernel(entry.name, entry.size, entry.coefficients, entry.normalize);
                break;
            }
            case ResourceCategory::MindGrain:
            case ResourceCategory::Layer:
                return false;
        }
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }

    refreshEntries();
    emit resourcesChanged();
    return true;
}

void ResourceBrowserController::handleImportEntryRequested() {
    if (project_ == nullptr || !browsedProject_.has_value()) {
        return;
    }
    const std::optional<std::uint64_t> id = panel_->selectedEntryId();
    if (!id.has_value()) {
        return;
    }

    switch (panel_->selectedCategory()) {
        case ResourceCategory::MindWave:
            if (const auto* entry = browsedProject_->mindWaveById(static_cast<MindWaveId>(*id))) {
                project_->addMindWave(entry->name, entry->wave);
            }
            break;
        case ResourceCategory::ToolPreset:
            if (const auto* entry = browsedProject_->toolPresetById(static_cast<ToolPresetId>(*id))) {
                if (entry->config) {
                    project_->addToolPreset(entry->name, *entry->config);
                }
            }
            break;
        case ResourceCategory::MindShot:
            if (const auto* entry = browsedProject_->mindShotById(static_cast<MindShotId>(*id))) {
                const MindShotId newId = project_->addMindShot(entry->name, entry->clip);
                if (auto* added = project_->mindShotById(newId)) {
                    added->fundamentalFrequencyHz = entry->fundamentalFrequencyHz;
                    added->startTimeOffsetSeconds = entry->startTimeOffsetSeconds;
                }
            }
            break;
        case ResourceCategory::ResonanceProfile:
            if (const auto* entry = browsedProject_->resonanceProfileById(static_cast<ResonanceProfileId>(*id))) {
                project_->addResonanceProfile(entry->name, entry->spectrum);
            }
            break;
        case ResourceCategory::ConvolutionKernel:
            if (const auto* entry = browsedProject_->convolutionKernelById(static_cast<ConvolutionKernelId>(*id))) {
                project_->addConvolutionKernel(entry->name, entry->size, entry->coefficients, entry->normalize);
            }
            break;
        case ResourceCategory::Layer:
            if (const auto* layer = browsedProject_->layerById(static_cast<sound_mind::core::LayerId>(*id))) {
                project_->addLayer(*layer);
            }
            break;
        case ResourceCategory::MindGrain:
            return;
    }

    refreshEntries();
    emit resourcesChanged();
}

bool ResourceBrowserController::browseOtherProjectAt(const std::filesystem::path& path, QString* errorMessage) {
    try {
        browsedProject_ = Project::load(path);
        browsedProjectPath_ = QString::fromStdString(path.string());
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }

    previewEngine_.stop();
    refreshPanel();
    return true;
}

void ResourceBrowserController::handleExportRequested() {
    const auto portableType = portableTypeFor(panel_->selectedCategory());
    if (!portableType.has_value() || !panel_->selectedEntryId().has_value()) {
        return;
    }
    const QString fileName =
        QFileDialog::getSaveFileName(panel_, QObject::tr("Export Resource"), QString(), fileFilterFor(*portableType));
    if (fileName.isEmpty()) {
        return;
    }
    QString errorMessage;
    if (!exportSelectedEntry(std::filesystem::path(fileName.toStdString()), &errorMessage)) {
        QMessageBox::critical(panel_, QObject::tr("Export Resource Failed"), errorMessage);
    }
}

void ResourceBrowserController::handleImportFromFileRequested() {
    const auto portableType = portableTypeFor(panel_->selectedCategory());
    if (!portableType.has_value()) {
        return;
    }
    const QString fileName =
        QFileDialog::getOpenFileName(panel_, QObject::tr("Import Resource"), QString(), fileFilterFor(*portableType));
    if (fileName.isEmpty()) {
        return;
    }
    QString errorMessage;
    if (!importFromFile(std::filesystem::path(fileName.toStdString()), &errorMessage)) {
        QMessageBox::critical(panel_, QObject::tr("Import Resource Failed"), errorMessage);
    }
}

void ResourceBrowserController::handleBrowseOtherProjectRequested() {
    const QString fileName = QFileDialog::getOpenFileName(panel_, QObject::tr("Browse Other Project"), QString(),
                                                            QObject::tr("Sound Mind Projects (*.smproj)"));
    if (fileName.isEmpty()) {
        return;
    }
    QString errorMessage;
    if (!browseOtherProjectAt(std::filesystem::path(fileName.toStdString()), &errorMessage)) {
        QMessageBox::critical(panel_, QObject::tr("Browse Other Project Failed"), errorMessage);
    }
}

void ResourceBrowserController::handleReturnToCurrentProjectRequested() {
    browsedProject_ = std::nullopt;
    browsedProjectPath_ = std::nullopt;
    previewEngine_.stop();
    refreshPanel();
}

void ResourceBrowserController::handlePlayRequested() {
    if (currentPreviewAudio_.frameCount() == 0) {
        return;
    }
    previewEngine_.loadAudio(currentPreviewAudio_);
    previewEngine_.play();
    panel_->setPlaying(true);
}

void ResourceBrowserController::handleStopRequested() {
    previewEngine_.stop();
    panel_->setPlaying(false);
}

void ResourceBrowserController::refreshToolkitEntries() {
    QStringList names;
    for (const ToolkitDraftEntry& entry : toolkitDraft_) {
        names.push_back(entry.displayName);
    }
    panel_->setToolkitEntries(names);
}

void ResourceBrowserController::handleAddToToolkitRequested() {
    const Project* project = activeProject();
    const std::optional<std::uint64_t> id = panel_->selectedEntryId();
    const auto portableType = portableTypeFor(panel_->selectedCategory());
    if (project == nullptr || !id.has_value() || !portableType.has_value()) {
        return;
    }

    nlohmann::json resource;
    QString displayName;
    switch (panel_->selectedCategory()) {
        case ResourceCategory::MindWave: {
            const auto* entry = project->mindWaveById(static_cast<MindWaveId>(*id));
            if (entry == nullptr) {
                return;
            }
            resource = *entry;
            displayName = QObject::tr("%1 (MindWave)").arg(QString::fromStdString(entry->name));
            break;
        }
        case ResourceCategory::ToolPreset: {
            const auto* entry = project->toolPresetById(static_cast<ToolPresetId>(*id));
            if (entry == nullptr || !entry->config) {
                return;
            }
            resource = *entry;
            displayName =
                QObject::tr("%1 (%2)").arg(QString::fromStdString(entry->name), toolTypeLabel(entry->config->type()));
            break;
        }
        case ResourceCategory::MindShot: {
            const auto* entry = project->mindShotById(static_cast<MindShotId>(*id));
            if (entry == nullptr) {
                return;
            }
            resource = *entry;
            displayName = QObject::tr("%1 (Mind Shot)").arg(QString::fromStdString(entry->name));
            break;
        }
        case ResourceCategory::ResonanceProfile: {
            const auto* entry = project->resonanceProfileById(static_cast<ResonanceProfileId>(*id));
            if (entry == nullptr) {
                return;
            }
            resource = *entry;
            displayName = QObject::tr("%1 (Resonance Profile)").arg(QString::fromStdString(entry->name));
            break;
        }
        case ResourceCategory::ConvolutionKernel: {
            const auto* entry = project->convolutionKernelById(static_cast<ConvolutionKernelId>(*id));
            if (entry == nullptr) {
                return;
            }
            resource = *entry;
            displayName = QObject::tr("%1 (Filter)").arg(QString::fromStdString(entry->name));
            break;
        }
        case ResourceCategory::MindGrain:
        case ResourceCategory::Layer:
            return;
    }

    toolkitDraft_.push_back({*portableType, std::move(resource), displayName});
    refreshToolkitEntries();
}

void ResourceBrowserController::handleRemoveFromToolkitRequested(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= toolkitDraft_.size()) {
        return;
    }
    toolkitDraft_.erase(toolkitDraft_.begin() + index);
    refreshToolkitEntries();
}

bool ResourceBrowserController::exportToolkitAt(const QString& name, const std::filesystem::path& path,
                                                 QString* errorMessage) {
    if (toolkitDraft_.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("The Toolkit draft is empty - add at least one entry first.");
        }
        return false;
    }

    std::vector<sound_mind::core::ToolkitEntry> entries;
    entries.reserve(toolkitDraft_.size());
    for (const ToolkitDraftEntry& draft : toolkitDraft_) {
        entries.push_back({draft.type, draft.resource});
    }

    try {
        sound_mind::core::exportToolkit(name.toStdString(), entries, path);
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }

    toolkitDraft_.clear();
    refreshToolkitEntries();
    return true;
}

void ResourceBrowserController::applyToolkitEntry(sound_mind::core::PortableResourceType type,
                                                    const nlohmann::json& resource, Project& target) {
    switch (type) {
        case sound_mind::core::PortableResourceType::MindWave: {
            const auto entry = resource.get<sound_mind::core::NamedMindWave>();
            target.addMindWave(entry.name, entry.wave);
            return;
        }
        case sound_mind::core::PortableResourceType::ToolPreset: {
            const auto entry = resource.get<sound_mind::core::NamedToolPreset>();
            if (entry.config) {
                target.addToolPreset(entry.name, *entry.config);
            }
            return;
        }
        case sound_mind::core::PortableResourceType::MindShot: {
            const auto entry = resource.get<sound_mind::core::NamedMindShot>();
            const MindShotId newId = target.addMindShot(entry.name, entry.clip);
            if (auto* added = target.mindShotById(newId)) {
                added->fundamentalFrequencyHz = entry.fundamentalFrequencyHz;
                added->startTimeOffsetSeconds = entry.startTimeOffsetSeconds;
            }
            return;
        }
        case sound_mind::core::PortableResourceType::ResonanceProfile: {
            const auto entry = resource.get<sound_mind::core::NamedResonanceProfile>();
            target.addResonanceProfile(entry.name, entry.spectrum);
            return;
        }
        case sound_mind::core::PortableResourceType::ConvolutionKernel: {
            const auto entry = resource.get<sound_mind::core::NamedConvolutionKernel>();
            target.addConvolutionKernel(entry.name, entry.size, entry.coefficients, entry.normalize);
            return;
        }
    }
}

bool ResourceBrowserController::importToolkitFrom(const std::filesystem::path& path, QString* errorMessage) {
    if (project_ == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("No project is open.");
        }
        return false;
    }

    try {
        const sound_mind::core::ImportedToolkit toolkit = sound_mind::core::importToolkit(path);
        // Applied to a trial copy first, swapped in only if every entry
        // succeeds - an all-or-nothing import, never a partially-applied
        // one (see this method's own header docs).
        Project trial = *project_;
        for (const sound_mind::core::ToolkitEntry& entry : toolkit.entries) {
            applyToolkitEntry(entry.type, entry.resource, trial);
        }
        *project_ = std::move(trial);
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }

    refreshEntries();
    emit resourcesChanged();
    return true;
}

void ResourceBrowserController::handleExportToolkitRequested() {
    if (toolkitDraft_.empty()) {
        QMessageBox::information(panel_, QObject::tr("Export Toolkit"),
                                  QObject::tr("Add at least one entry to the Toolkit draft first."));
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(panel_, QObject::tr("Export Toolkit"), QObject::tr("Toolkit name:"),
                                                 QLineEdit::Normal, QString(), &ok);
    if (!ok || name.isEmpty()) {
        return;
    }
    const QString fileName = QFileDialog::getSaveFileName(panel_, QObject::tr("Export Toolkit"), QString(),
                                                            QObject::tr("Sound Mind Toolkits (*.smtoolkit)"));
    if (fileName.isEmpty()) {
        return;
    }
    QString errorMessage;
    if (!exportToolkitAt(name, std::filesystem::path(fileName.toStdString()), &errorMessage)) {
        QMessageBox::critical(panel_, QObject::tr("Export Toolkit Failed"), errorMessage);
    }
}

void ResourceBrowserController::handleImportToolkitRequested() {
    const QString fileName = QFileDialog::getOpenFileName(panel_, QObject::tr("Import Toolkit"), QString(),
                                                            QObject::tr("Sound Mind Toolkits (*.smtoolkit)"));
    if (fileName.isEmpty()) {
        return;
    }
    QString errorMessage;
    if (!importToolkitFrom(std::filesystem::path(fileName.toStdString()), &errorMessage)) {
        QMessageBox::critical(panel_, QObject::tr("Import Toolkit Failed"), errorMessage);
    }
}

}  // namespace sound_mind::studio
