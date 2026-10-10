#include "sound_mind/studio/resource_browser_controller.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <string_view>

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
#include "sound_mind/codec/rgb_image_resample.h"
#include "sound_mind/core/mind_shot_preview.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/resource_file.h"
#include "sound_mind/core/tool_configuration.h"
#include "sound_mind/core/tool_configuration_preview.h"
#include "sound_mind/studio/name_collision_dialog.h"
#include "sound_mind/studio/qt_image_conversion.h"
#include "sound_mind/studio/resource_browser_panel.h"

namespace sound_mind::studio {

namespace {

using sound_mind::core::ConvolutionKernelId;
using sound_mind::core::FilterPresetId;
using sound_mind::core::MindGrainId;
using sound_mind::core::MindShotId;
using sound_mind::core::MindWaveId;
using sound_mind::core::Project;
using sound_mind::core::ResonanceProfileId;
using sound_mind::core::ToolPresetId;

/// @brief The inspector raster area's own fixed size, shared by every
/// category that renders one (MindWave, Mind Shot, Resonance Profile).
constexpr int kRasterWidth = 240;
constexpr int kRasterHeight = 80;

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
    QImage image(kRasterWidth, kRasterHeight, QImage::Format_RGB888);
    image.fill(Qt::black);
    if (spectrum.empty()) {
        return image;
    }

    QPainter painter(&image);
    painter.setPen(Qt::green);
    const double stepX = static_cast<double>(kRasterWidth) / static_cast<double>(spectrum.size());
    for (std::size_t i = 0; i < spectrum.size(); ++i) {
        const double x = static_cast<double>(i) * stepX;
        const double value = std::clamp(spectrum[i], 0.0f, 1.0f);
        const double barHeight = value * kRasterHeight;
        painter.drawLine(QPointF(x, kRasterHeight), QPointF(x, kRasterHeight - barHeight));
    }
    return image;
}

/// @brief The preview stroke's own lower frequency bound for display -
/// confirmed with the user: the rendered raster is clipped vertically at
/// this frequency, so the (mostly empty, since the stroke itself never
/// goes below 3 kHz) low-frequency portion of the full axis doesn't waste
/// space in a preview this small.
constexpr float kPreviewMinFrequencyHz = 1000.0f;

/// @brief Crops `content` (as `sound_mind::core::
/// toolConfigurationPreviewStreamImage()` returns it) vertically at
/// `kPreviewMinFrequencyHz` and rasterizes it
/// to the inspector's own fixed raster size - confirmed with the user
/// as the Resource Browser's own Tool Preset preview, now shared by
/// every category that previews a synthesized stroke the same way.
QImage cropAndRasterizePreviewImage(const sound_mind::codec::StreamImage& content) {
    using namespace sound_mind::core;
    const auto& codecConfig = content.config;

    // Clip out every bin below kPreviewMinFrequencyHz before rendering -
    // frequencyToBinIndex() maps low Hz to low bin indices, so this keeps
    // bins [keepFromBin, binCount - 1] (the high end of the axis, where
    // the stroke itself actually is) and drops the rest.
    const float keepFromBinF = frequencyToBinIndex(kPreviewMinFrequencyHz, codecConfig);
    const auto keepFromBin = static_cast<std::uint32_t>(
        std::clamp(std::lround(keepFromBinF), 0L, static_cast<long>(codecConfig.binCount) - 1));
    const std::uint32_t keptBinCount = codecConfig.binCount - keepFromBin;
    const std::uint32_t frameCount = content.frameCount;

    sound_mind::codec::StreamImage cropped;
    cropped.config = codecConfig;
    cropped.config.binCount = keptBinCount;
    cropped.frameCount = frameCount;
    cropped.leftMagnitudeDb.resize(std::size_t{keptBinCount} * frameCount);
    cropped.rightMagnitudeDb.resize(std::size_t{keptBinCount} * frameCount);
    cropped.sharedPhaseRadians.resize(std::size_t{keptBinCount} * frameCount);
    for (std::uint32_t bin = 0; bin < keptBinCount; ++bin) {
        const std::uint32_t sourceBin = keepFromBin + bin;
        for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
            const std::size_t sourceIndex = std::size_t{sourceBin} * frameCount + frame;
            const std::size_t destIndex = std::size_t{bin} * frameCount + frame;
            cropped.leftMagnitudeDb[destIndex] = content.leftMagnitudeDb[sourceIndex];
            cropped.rightMagnitudeDb[destIndex] = content.rightMagnitudeDb[sourceIndex];
            cropped.sharedPhaseRadians[destIndex] = content.sharedPhaseRadians[sourceIndex];
        }
    }

    const auto rgb = sound_mind::codec::toRgbImage(cropped);
    const auto downsampled = sound_mind::codec::downsampleAveraged(rgb, kRasterWidth, kRasterHeight);
    return toQImageView(downsampled).copy();
}

/// @brief A default `InstrumentConfiguration` with `mindWaveId` bound as
/// its own vibrato source - exists only so a standalone MindWave library
/// entry (not yet attached to any real Tool Preset) can still be
/// previewed the same way a Tool Preset is: painted as a representative
/// stroke and decoded. Plain default harmonics/inharmonicity - nothing
/// about *this* preview is about the Instrument, only about making the
/// MindWave's own modulation audible.
std::unique_ptr<sound_mind::core::ToolConfiguration> syntheticInstrumentPreviewConfig(
    sound_mind::core::MindWaveId mindWaveId) {
    auto config = std::make_unique<sound_mind::core::InstrumentConfiguration>();
    config->setVibratoMindWave(mindWaveId);
    return config;
}

/// @brief A default `ResonanceConfiguration` carrying `spectrum` - exists
/// only so a standalone Resonance Profile library entry can be previewed
/// the same way a Tool Preset is, before it's ever been used to paint a
/// real stroke.
std::unique_ptr<sound_mind::core::ToolConfiguration> syntheticResonancePreviewConfig(
    sound_mind::core::ResonanceProfileId sourceId, std::vector<float> spectrum) {
    auto config = std::make_unique<sound_mind::core::ResonanceConfiguration>();
    config->setSpectrum(sourceId, std::move(spectrum));
    return config;
}

/// @brief Renders a `CurveGraph`'s own shape (every edge, as a straight
/// line between its two endpoints - exactly what a `CurveGraph` actually
/// represents, see that class's own docs) into a fixed-size image -
/// confirmed with the user as the Resonance Profile inspector's own
/// "also show the path/branching curve rendered, next to the spectrum
/// profile." A uniform scale (not a stretch-to-fit) keeps the curve's
/// own real proportions, since `CurvePoint::x`/`y` are already
/// comparable units (see that struct's own docs).
QImage renderCurveGraphImage(const sound_mind::core::CurveGraph& graph) {
    QImage image(kRasterWidth, kRasterHeight, QImage::Format_RGB888);
    image.fill(Qt::black);
    const auto& nodes = graph.nodes();
    if (nodes.empty()) {
        return image;
    }

    double minX = nodes.front().position.x;
    double maxX = minX;
    double minY = nodes.front().position.y;
    double maxY = minY;
    for (const auto& node : nodes) {
        minX = std::min(minX, node.position.x);
        maxX = std::max(maxX, node.position.x);
        minY = std::min(minY, node.position.y);
        maxY = std::max(maxY, node.position.y);
    }
    const double spanX = std::max(maxX - minX, 1e-6);
    const double spanY = std::max(maxY - minY, 1e-6);
    constexpr double kMargin = 8.0;
    const double scale =
        std::min((kRasterWidth - 2 * kMargin) / spanX, (kRasterHeight - 2 * kMargin) / spanY);
    const double offsetX = kMargin + (kRasterWidth - 2 * kMargin - spanX * scale) / 2.0;
    const double offsetY = kMargin + (kRasterHeight - 2 * kMargin - spanY * scale) / 2.0;
    const auto toPoint = [&](const sound_mind::core::CurvePoint& p) {
        return QPointF(offsetX + (p.x - minX) * scale, offsetY + (p.y - minY) * scale);
    };

    QPainter painter(&image);
    painter.setPen(Qt::cyan);
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        for (const std::size_t neighbor : nodes[i].neighbors) {
            if (neighbor > i) {
                painter.drawLine(toPoint(nodes[i].position), toPoint(nodes[neighbor].position));
            }
        }
    }
    return image;
}

/// @brief Composes a Resonance Profile's own spectrum plot and source
/// curve render side by side into one image - see
/// `renderSpectrumImage()`'s/`renderCurveGraphImage()`'s own docs for
/// each half. A blank (all-black) right half if `graph` has no nodes
/// (an entry saved before `sourceCurve` existed).
QImage renderResonanceInspectorImage(const std::vector<float>& spectrum, const sound_mind::core::CurveGraph& graph) {
    QImage combined(kRasterWidth * 2, kRasterHeight, QImage::Format_RGB888);
    QPainter painter(&combined);
    painter.drawImage(0, 0, renderSpectrumImage(spectrum));
    painter.drawImage(kRasterWidth, 0, renderCurveGraphImage(graph));
    return combined;
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
        case ResourceCategory::FilterPreset:
            return sound_mind::core::PortableResourceType::FilterPreset;
        case ResourceCategory::GridPreset:
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
            case ResourceCategory::FilterPreset:
                for (const auto& entry : project->filterPresets()) {
                    rows.push_back({entry.id, QString::fromStdString(entry.name)});
                }
                break;
            case ResourceCategory::GridPreset:
                for (const auto& entry : project->gridPresets()) {
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
    const bool canImportEntry = browsingOther && panel_->selectedCategory() != ResourceCategory::MindGrain &&
                                 panel_->selectedCategory() != ResourceCategory::GridPreset;

    switch (panel_->selectedCategory()) {
        case ResourceCategory::MindWave: {
            const auto* entry = project->mindWaveById(static_cast<MindWaveId>(*id));
            if (entry == nullptr) {
                panel_->clearInspector();
                return;
            }
            name = QString::fromStdString(entry->name);
            parameterText = QString::fromStdString(nlohmann::json(entry->wave).dump(2));

            // The same evaluate-then-grayscale pipeline MindWavesPanel's
            // own per-row mini-preview already uses
            // (MindWaveController::refreshMindWavesPanel()).
            const auto config = sound_mind::core::streamCodecConfigFor(project->settings());
            const auto field =
                sound_mind::core::evaluateMindWaveField(entry->wave, config, project->settings().canvasWidth);
            const auto grayscale =
                sound_mind::codec::toGrayscaleImage(field, project->settings().canvasWidth, config.binCount);
            const auto downsampled = sound_mind::codec::downsampleAveraged(grayscale, kRasterWidth, kRasterHeight);
            raster = toQImageView(downsampled).copy();

            // Audible preview: a default Instrument bound to this
            // MindWave as its own vibrato source, painted as the same
            // representative 3-second stroke a Tool Preset preview
            // uses, then decoded - see syntheticInstrumentPreviewConfig()'s
            // own docs. Doesn't change the raster above at all; this
            // only adds a Play button alongside the existing visual.
            {
                const auto previewConfig = syntheticInstrumentPreviewConfig(static_cast<MindWaveId>(*id));
                currentPreviewImage_ = sound_mind::core::toolConfigurationPreviewStreamImage(*previewConfig, *project);
                currentPreviewAudio_ = sound_mind::codec::AudioBuffer{};
                canPlay = true;
            }
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
            currentPreviewImage_ = sound_mind::core::toolConfigurationPreviewStreamImage(*entry->config, *project);
            raster = cropAndRasterizePreviewImage(*currentPreviewImage_);
            currentPreviewAudio_ = sound_mind::codec::AudioBuffer{};
            canPlay = true;
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
            // decode() (a full inverse STFT) is the expensive part of a
            // Mind Shot preview, not the raster render above - deferred to
            // handlePlayRequested()'s first actual Play click instead of
            // running on every mere selection.
            currentPreviewImage_ = image;
            currentPreviewAudio_ = sound_mind::codec::AudioBuffer{};
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
            parameterText = QObject::tr("spectrum: %1 values\nsourceCurve: %2 nodes")
                                .arg(entry->spectrum.size())
                                .arg(entry->sourceCurve.nodes().size());
            raster = renderResonanceInspectorImage(entry->spectrum, entry->sourceCurve);

            // Audible preview: a default Resonance brush carrying this
            // profile's own spectrum, painted as the same representative
            // stroke a Tool Preset preview uses, then decoded - see
            // syntheticResonancePreviewConfig()'s own docs. Doesn't
            // change the raster above (spectrum + source curve) at all.
            if (!entry->spectrum.empty()) {
                const auto previewConfig =
                    syntheticResonancePreviewConfig(static_cast<ResonanceProfileId>(*id), entry->spectrum);
                currentPreviewImage_ = sound_mind::core::toolConfigurationPreviewStreamImage(*previewConfig, *project);
                currentPreviewAudio_ = sound_mind::codec::AudioBuffer{};
                canPlay = true;
            }
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
        case ResourceCategory::FilterPreset: {
            const auto* entry = project->filterPresetById(static_cast<FilterPresetId>(*id));
            if (entry == nullptr) {
                panel_->clearInspector();
                return;
            }
            name = QString::fromStdString(entry->name);
            nlohmann::json json;
            sound_mind::core::to_json(json, entry->config);
            parameterText = QString::fromStdString(json.dump(2));
            break;
        }
        case ResourceCategory::GridPreset: {
            const auto* entry = project->gridPresetById(static_cast<sound_mind::core::GridPresetId>(*id));
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
            case ResourceCategory::FilterPreset:
                sound_mind::core::exportFilterPreset(*project->filterPresetById(static_cast<FilterPresetId>(*id)),
                                                      path);
                break;
            case ResourceCategory::GridPreset:
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
                const auto resolution = sound_mind::studio::resolveImportName(
                    panel_, categoryLabel(ResourceCategory::MindWave), entry.name,
                    [this](const std::string& candidate) { return project_->mindWaveNameExists(candidate); });
                if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                    return true;
                }
                if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                    auto& waves = project_->mindWaves();
                    const auto it = std::find_if(waves.begin(), waves.end(),
                                                  [&resolution](const auto& w) { return w.name == resolution.name; });
                    if (it != waves.end()) {
                        it->wave = entry.wave;
                    }
                } else {
                    project_->addMindWave(resolution.name, entry.wave);
                }
                break;
            }
            case ResourceCategory::ToolPreset: {
                const auto entry = sound_mind::core::importToolPreset(path);
                const auto resolution = sound_mind::studio::resolveImportName(
                    panel_, categoryLabel(ResourceCategory::ToolPreset), entry.name,
                    [this](const std::string& candidate) { return project_->toolPresetNameExists(candidate); });
                if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                    return true;
                }
                if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                    auto& presets = project_->toolPresets();
                    const auto it = std::find_if(
                        presets.begin(), presets.end(),
                        [&resolution](const auto& named) { return named.name == resolution.name; });
                    if (it != presets.end() && entry.config) {
                        it->config = entry.config->clone();
                    }
                } else if (entry.config) {
                    project_->addToolPreset(resolution.name, *entry.config);
                }
                break;
            }
            case ResourceCategory::MindShot: {
                const auto entry = sound_mind::core::importMindShot(path);
                const auto resolution = sound_mind::studio::resolveImportName(
                    panel_, categoryLabel(ResourceCategory::MindShot), entry.name,
                    [this](const std::string& candidate) { return project_->mindShotNameExists(candidate); });
                if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                    return true;
                }
                if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                    auto& shots = project_->mindShots();
                    const auto it = std::find_if(shots.begin(), shots.end(),
                                                  [&resolution](const auto& s) { return s.name == resolution.name; });
                    if (it != shots.end()) {
                        it->clip = entry.clip;
                        it->fundamentalFrequencyHz = entry.fundamentalFrequencyHz;
                        it->startTimeOffsetSeconds = entry.startTimeOffsetSeconds;
                    }
                } else {
                    const MindShotId newId = project_->addMindShot(resolution.name, entry.clip);
                    if (auto* added = project_->mindShotById(newId)) {
                        added->fundamentalFrequencyHz = entry.fundamentalFrequencyHz;
                        added->startTimeOffsetSeconds = entry.startTimeOffsetSeconds;
                    }
                }
                break;
            }
            case ResourceCategory::ResonanceProfile: {
                const auto entry = sound_mind::core::importResonanceProfile(path);
                const auto resolution = sound_mind::studio::resolveImportName(
                    panel_, categoryLabel(ResourceCategory::ResonanceProfile), entry.name,
                    [this](const std::string& candidate) { return project_->resonanceProfileNameExists(candidate); });
                if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                    return true;
                }
                if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                    auto& profiles = project_->resonanceProfiles();
                    const auto it = std::find_if(
                        profiles.begin(), profiles.end(),
                        [&resolution](const auto& p) { return p.name == resolution.name; });
                    if (it != profiles.end()) {
                        it->spectrum = entry.spectrum;
                        it->sourceCurve = entry.sourceCurve;
                    }
                } else {
                    project_->addResonanceProfile(resolution.name, entry.spectrum, entry.sourceCurve);
                }
                break;
            }
            case ResourceCategory::ConvolutionKernel: {
                const auto entry = sound_mind::core::importConvolutionKernel(path);
                const auto resolution = sound_mind::studio::resolveImportName(
                    panel_, categoryLabel(ResourceCategory::ConvolutionKernel), entry.name,
                    [this](const std::string& candidate) { return project_->convolutionKernelNameExists(candidate); });
                if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                    return true;
                }
                if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                    auto& kernels = project_->convolutionKernels();
                    const auto it = std::find_if(
                        kernels.begin(), kernels.end(),
                        [&resolution](const auto& k) { return k.name == resolution.name; });
                    if (it != kernels.end()) {
                        it->size = entry.size;
                        it->coefficients = entry.coefficients;
                        it->normalize = entry.normalize;
                    }
                } else {
                    project_->addConvolutionKernel(resolution.name, entry.size, entry.coefficients, entry.normalize);
                }
                break;
            }
            case ResourceCategory::FilterPreset: {
                const auto entry = sound_mind::core::importFilterPreset(path);
                const auto resolution = sound_mind::studio::resolveImportName(
                    panel_, categoryLabel(ResourceCategory::FilterPreset), entry.name,
                    [this](const std::string& candidate) { return project_->filterPresetNameExists(candidate); });
                if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                    return true;
                }
                if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                    auto& presets = project_->filterPresets();
                    const auto it = std::find_if(
                        presets.begin(), presets.end(),
                        [&resolution](const auto& named) { return named.name == resolution.name; });
                    if (it != presets.end()) {
                        it->config = entry.config;
                    }
                } else {
                    project_->addFilterPreset(resolution.name, entry.config);
                }
                break;
            }
            case ResourceCategory::GridPreset:
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
        case ResourceCategory::MindWave: {
            const auto* entry = browsedProject_->mindWaveById(static_cast<MindWaveId>(*id));
            if (entry == nullptr) {
                break;
            }
            const auto resolution = sound_mind::studio::resolveImportName(
                panel_, categoryLabel(ResourceCategory::MindWave), entry->name,
                [this](const std::string& candidate) { return project_->mindWaveNameExists(candidate); });
            if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                return;
            }
            if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                auto& waves = project_->mindWaves();
                const auto it = std::find_if(waves.begin(), waves.end(),
                                              [&resolution](const auto& w) { return w.name == resolution.name; });
                if (it != waves.end()) {
                    it->wave = entry->wave;
                }
            } else {
                project_->addMindWave(resolution.name, entry->wave);
            }
            break;
        }
        case ResourceCategory::ToolPreset: {
            const auto* entry = browsedProject_->toolPresetById(static_cast<ToolPresetId>(*id));
            if (entry == nullptr || !entry->config) {
                break;
            }
            const auto resolution = sound_mind::studio::resolveImportName(
                panel_, categoryLabel(ResourceCategory::ToolPreset), entry->name,
                [this](const std::string& candidate) { return project_->toolPresetNameExists(candidate); });
            if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                return;
            }
            if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                auto& presets = project_->toolPresets();
                const auto it = std::find_if(
                    presets.begin(), presets.end(),
                    [&resolution](const auto& named) { return named.name == resolution.name; });
                if (it != presets.end()) {
                    it->config = entry->config->clone();
                }
            } else {
                project_->addToolPreset(resolution.name, *entry->config);
            }
            break;
        }
        case ResourceCategory::MindShot: {
            const auto* entry = browsedProject_->mindShotById(static_cast<MindShotId>(*id));
            if (entry == nullptr) {
                break;
            }
            const auto resolution = sound_mind::studio::resolveImportName(
                panel_, categoryLabel(ResourceCategory::MindShot), entry->name,
                [this](const std::string& candidate) { return project_->mindShotNameExists(candidate); });
            if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                return;
            }
            if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                auto& shots = project_->mindShots();
                const auto it = std::find_if(shots.begin(), shots.end(),
                                              [&resolution](const auto& s) { return s.name == resolution.name; });
                if (it != shots.end()) {
                    it->clip = entry->clip;
                    it->fundamentalFrequencyHz = entry->fundamentalFrequencyHz;
                    it->startTimeOffsetSeconds = entry->startTimeOffsetSeconds;
                }
            } else {
                const MindShotId newId = project_->addMindShot(resolution.name, entry->clip);
                if (auto* added = project_->mindShotById(newId)) {
                    added->fundamentalFrequencyHz = entry->fundamentalFrequencyHz;
                    added->startTimeOffsetSeconds = entry->startTimeOffsetSeconds;
                }
            }
            break;
        }
        case ResourceCategory::ResonanceProfile: {
            const auto* entry = browsedProject_->resonanceProfileById(static_cast<ResonanceProfileId>(*id));
            if (entry == nullptr) {
                break;
            }
            const auto resolution = sound_mind::studio::resolveImportName(
                panel_, categoryLabel(ResourceCategory::ResonanceProfile), entry->name,
                [this](const std::string& candidate) { return project_->resonanceProfileNameExists(candidate); });
            if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                return;
            }
            if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                auto& profiles = project_->resonanceProfiles();
                const auto it = std::find_if(
                    profiles.begin(), profiles.end(),
                    [&resolution](const auto& p) { return p.name == resolution.name; });
                if (it != profiles.end()) {
                    it->spectrum = entry->spectrum;
                    it->sourceCurve = entry->sourceCurve;
                }
            } else {
                project_->addResonanceProfile(resolution.name, entry->spectrum, entry->sourceCurve);
            }
            break;
        }
        case ResourceCategory::ConvolutionKernel: {
            const auto* entry = browsedProject_->convolutionKernelById(static_cast<ConvolutionKernelId>(*id));
            if (entry == nullptr) {
                break;
            }
            const auto resolution = sound_mind::studio::resolveImportName(
                panel_, categoryLabel(ResourceCategory::ConvolutionKernel), entry->name,
                [this](const std::string& candidate) { return project_->convolutionKernelNameExists(candidate); });
            if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                return;
            }
            if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                auto& kernels = project_->convolutionKernels();
                const auto it = std::find_if(
                    kernels.begin(), kernels.end(),
                    [&resolution](const auto& k) { return k.name == resolution.name; });
                if (it != kernels.end()) {
                    it->size = entry->size;
                    it->coefficients = entry->coefficients;
                    it->normalize = entry->normalize;
                }
            } else {
                project_->addConvolutionKernel(resolution.name, entry->size, entry->coefficients, entry->normalize);
            }
            break;
        }
        case ResourceCategory::FilterPreset: {
            const auto* entry = browsedProject_->filterPresetById(static_cast<FilterPresetId>(*id));
            if (entry == nullptr) {
                break;
            }
            const auto resolution = sound_mind::studio::resolveImportName(
                panel_, categoryLabel(ResourceCategory::FilterPreset), entry->name,
                [this](const std::string& candidate) { return project_->filterPresetNameExists(candidate); });
            if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                return;
            }
            if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                auto& presets = project_->filterPresets();
                const auto it = std::find_if(
                    presets.begin(), presets.end(),
                    [&resolution](const auto& named) { return named.name == resolution.name; });
                if (it != presets.end()) {
                    it->config = entry->config;
                }
            } else {
                project_->addFilterPreset(resolution.name, entry->config);
            }
            break;
        }
        case ResourceCategory::Layer:
            if (const auto* layer = browsedProject_->layerById(static_cast<sound_mind::core::LayerId>(*id))) {
                project_->addLayer(*layer);
            }
            break;
        case ResourceCategory::GridPreset:
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
        if (!currentPreviewImage_.has_value()) {
            return;
        }
        // First Play on this selection: decode now and cache, so a second
        // Play on the same entry doesn't pay for decode() again.
        currentPreviewAudio_ = sound_mind::codec::decode(*currentPreviewImage_);
        if (currentPreviewAudio_.frameCount() == 0) {
            return;
        }
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
    const QStringList autoAddedNames = addSelectedEntryToToolkit();
    if (!autoAddedNames.isEmpty()) {
        QMessageBox::information(
            panel_, QObject::tr("Dependencies Added"),
            QObject::tr("Also added to the Toolkit draft, since the selected entry depends on it:\n\n%1")
                .arg(autoAddedNames.join(QStringLiteral("\n"))));
    }
}

QStringList ResourceBrowserController::addSelectedEntryToToolkit() {
    const Project* project = activeProject();
    const std::optional<std::uint64_t> id = panel_->selectedEntryId();
    const auto portableType = portableTypeFor(panel_->selectedCategory());
    QStringList autoAddedNames;
    if (project == nullptr || !id.has_value() || !portableType.has_value()) {
        return autoAddedNames;
    }

    if (addToolkitEntryWithDependencies(*portableType, *id, *project, autoAddedNames)) {
        refreshToolkitEntries();
    }
    return autoAddedNames;
}

bool ResourceBrowserController::resolveResourceForToolkit(sound_mind::core::PortableResourceType type,
                                                           std::uint64_t id, const Project& project,
                                                           nlohmann::json& outResource, QString& outDisplayName) {
    switch (type) {
        case sound_mind::core::PortableResourceType::MindWave: {
            const auto* entry = project.mindWaveById(static_cast<MindWaveId>(id));
            if (entry == nullptr) {
                return false;
            }
            outResource = *entry;
            outDisplayName = QObject::tr("%1 (MindWave)").arg(QString::fromStdString(entry->name));
            return true;
        }
        case sound_mind::core::PortableResourceType::ToolPreset: {
            const auto* entry = project.toolPresetById(static_cast<ToolPresetId>(id));
            if (entry == nullptr || !entry->config) {
                return false;
            }
            outResource = *entry;
            outDisplayName =
                QObject::tr("%1 (%2)").arg(QString::fromStdString(entry->name), toolTypeLabel(entry->config->type()));
            return true;
        }
        case sound_mind::core::PortableResourceType::MindShot: {
            const auto* entry = project.mindShotById(static_cast<MindShotId>(id));
            if (entry == nullptr) {
                return false;
            }
            outResource = *entry;
            outDisplayName = QObject::tr("%1 (Mind Shot)").arg(QString::fromStdString(entry->name));
            return true;
        }
        case sound_mind::core::PortableResourceType::ResonanceProfile: {
            const auto* entry = project.resonanceProfileById(static_cast<ResonanceProfileId>(id));
            if (entry == nullptr) {
                return false;
            }
            outResource = *entry;
            outDisplayName = QObject::tr("%1 (Resonance Profile)").arg(QString::fromStdString(entry->name));
            return true;
        }
        case sound_mind::core::PortableResourceType::ConvolutionKernel: {
            const auto* entry = project.convolutionKernelById(static_cast<ConvolutionKernelId>(id));
            if (entry == nullptr) {
                return false;
            }
            outResource = *entry;
            outDisplayName = QObject::tr("%1 (Convolution Kernel)").arg(QString::fromStdString(entry->name));
            return true;
        }
        case sound_mind::core::PortableResourceType::FilterPreset: {
            const auto* entry = project.filterPresetById(static_cast<FilterPresetId>(id));
            if (entry == nullptr) {
                return false;
            }
            outResource = *entry;
            outDisplayName = QObject::tr("%1 (Filter Preset)").arg(QString::fromStdString(entry->name));
            return true;
        }
    }
    return false;
}

std::vector<ResourceBrowserController::ResourceDependency> ResourceBrowserController::findDependencies(
    const nlohmann::json& resourceJson) {
    std::vector<ResourceDependency> dependencies;
    std::function<void(const nlohmann::json&)> scan = [&](const nlohmann::json& node) {
        if (node.is_object()) {
            for (auto it = node.begin(); it != node.end(); ++it) {
                const std::string& key = it.key();
                if (it->is_number_integer()) {
                    constexpr std::string_view kMindWaveIdSuffix = "MindWaveId";
                    if (key.size() > kMindWaveIdSuffix.size() &&
                        key.compare(key.size() - kMindWaveIdSuffix.size(), kMindWaveIdSuffix.size(),
                                    kMindWaveIdSuffix) == 0) {
                        dependencies.push_back(
                            {sound_mind::core::PortableResourceType::MindWave, it->get<std::uint64_t>()});
                    } else if (key == "sourceMindShotId") {
                        dependencies.push_back(
                            {sound_mind::core::PortableResourceType::MindShot, it->get<std::uint64_t>()});
                    } else if (key == "sourceResonanceProfileId") {
                        dependencies.push_back(
                            {sound_mind::core::PortableResourceType::ResonanceProfile, it->get<std::uint64_t>()});
                    }
                    // "sourceMindGrainId" deliberately not tracked - Mind
                    // Grain has no portable form to bundle (see
                    // docs/sound-mind-architecture.md's "Portable Resource
                    // Files" table), so there's nothing to add for it.
                }
                scan(*it);
            }
        } else if (node.is_array()) {
            for (const auto& item : node) {
                scan(item);
            }
        }
    };
    scan(resourceJson);
    return dependencies;
}

bool ResourceBrowserController::addToolkitEntryWithDependencies(sound_mind::core::PortableResourceType type,
                                                                 std::uint64_t id, const Project& project,
                                                                 QStringList& autoAddedNames, bool isDependency) {
    for (const ToolkitDraftEntry& existing : toolkitDraft_) {
        if (existing.type == type && existing.sourceId == id) {
            return true;  // Already in the draft - nothing more to do.
        }
    }

    nlohmann::json resource;
    QString displayName;
    if (!resolveResourceForToolkit(type, id, project, resource, displayName)) {
        return false;
    }

    toolkitDraft_.push_back({type, id, resource, displayName});
    if (isDependency) {
        autoAddedNames.push_back(displayName);
    }

    for (const ResourceDependency& dependency : findDependencies(resource)) {
        addToolkitEntryWithDependencies(dependency.type, dependency.id, project, autoAddedNames,
                                         /*isDependency=*/true);
    }
    return true;
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
            const auto resolution = sound_mind::studio::resolveImportName(
                panel_, categoryLabel(ResourceCategory::MindWave), entry.name,
                [&target](const std::string& candidate) { return target.mindWaveNameExists(candidate); });
            if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                return;
            }
            if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                auto& waves = target.mindWaves();
                const auto it = std::find_if(waves.begin(), waves.end(),
                                              [&resolution](const auto& w) { return w.name == resolution.name; });
                if (it != waves.end()) {
                    it->wave = entry.wave;
                }
            } else {
                target.addMindWave(resolution.name, entry.wave);
            }
            return;
        }
        case sound_mind::core::PortableResourceType::ToolPreset: {
            const auto entry = resource.get<sound_mind::core::NamedToolPreset>();
            if (!entry.config) {
                return;
            }
            const auto resolution = sound_mind::studio::resolveImportName(
                panel_, categoryLabel(ResourceCategory::ToolPreset), entry.name,
                [&target](const std::string& candidate) { return target.toolPresetNameExists(candidate); });
            if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                return;
            }
            if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                auto& presets = target.toolPresets();
                const auto it = std::find_if(
                    presets.begin(), presets.end(),
                    [&resolution](const auto& named) { return named.name == resolution.name; });
                if (it != presets.end()) {
                    it->config = entry.config->clone();
                }
            } else {
                target.addToolPreset(resolution.name, *entry.config);
            }
            return;
        }
        case sound_mind::core::PortableResourceType::MindShot: {
            const auto entry = resource.get<sound_mind::core::NamedMindShot>();
            const auto resolution = sound_mind::studio::resolveImportName(
                panel_, categoryLabel(ResourceCategory::MindShot), entry.name,
                [&target](const std::string& candidate) { return target.mindShotNameExists(candidate); });
            if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                return;
            }
            if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                auto& shots = target.mindShots();
                const auto it = std::find_if(shots.begin(), shots.end(),
                                              [&resolution](const auto& s) { return s.name == resolution.name; });
                if (it != shots.end()) {
                    it->clip = entry.clip;
                    it->fundamentalFrequencyHz = entry.fundamentalFrequencyHz;
                    it->startTimeOffsetSeconds = entry.startTimeOffsetSeconds;
                }
            } else {
                const MindShotId newId = target.addMindShot(resolution.name, entry.clip);
                if (auto* added = target.mindShotById(newId)) {
                    added->fundamentalFrequencyHz = entry.fundamentalFrequencyHz;
                    added->startTimeOffsetSeconds = entry.startTimeOffsetSeconds;
                }
            }
            return;
        }
        case sound_mind::core::PortableResourceType::ResonanceProfile: {
            const auto entry = resource.get<sound_mind::core::NamedResonanceProfile>();
            const auto resolution = sound_mind::studio::resolveImportName(
                panel_, categoryLabel(ResourceCategory::ResonanceProfile), entry.name,
                [&target](const std::string& candidate) { return target.resonanceProfileNameExists(candidate); });
            if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                return;
            }
            if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                auto& profiles = target.resonanceProfiles();
                const auto it = std::find_if(
                    profiles.begin(), profiles.end(),
                    [&resolution](const auto& p) { return p.name == resolution.name; });
                if (it != profiles.end()) {
                    it->spectrum = entry.spectrum;
                    it->sourceCurve = entry.sourceCurve;
                }
            } else {
                target.addResonanceProfile(resolution.name, entry.spectrum, entry.sourceCurve);
            }
            return;
        }
        case sound_mind::core::PortableResourceType::ConvolutionKernel: {
            const auto entry = resource.get<sound_mind::core::NamedConvolutionKernel>();
            const auto resolution = sound_mind::studio::resolveImportName(
                panel_, categoryLabel(ResourceCategory::ConvolutionKernel), entry.name,
                [&target](const std::string& candidate) { return target.convolutionKernelNameExists(candidate); });
            if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                return;
            }
            if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                auto& kernels = target.convolutionKernels();
                const auto it = std::find_if(
                    kernels.begin(), kernels.end(),
                    [&resolution](const auto& k) { return k.name == resolution.name; });
                if (it != kernels.end()) {
                    it->size = entry.size;
                    it->coefficients = entry.coefficients;
                    it->normalize = entry.normalize;
                }
            } else {
                target.addConvolutionKernel(resolution.name, entry.size, entry.coefficients, entry.normalize);
            }
            return;
        }
        case sound_mind::core::PortableResourceType::FilterPreset: {
            const auto entry = resource.get<sound_mind::core::NamedFilterPreset>();
            const auto resolution = sound_mind::studio::resolveImportName(
                panel_, categoryLabel(ResourceCategory::FilterPreset), entry.name,
                [&target](const std::string& candidate) { return target.filterPresetNameExists(candidate); });
            if (resolution.action == sound_mind::studio::ImportNameAction::Skip) {
                return;
            }
            if (resolution.action == sound_mind::studio::ImportNameAction::OverwriteExisting) {
                auto& presets = target.filterPresets();
                const auto it = std::find_if(
                    presets.begin(), presets.end(),
                    [&resolution](const auto& named) { return named.name == resolution.name; });
                if (it != presets.end()) {
                    it->config = entry.config;
                }
            } else {
                target.addFilterPreset(resolution.name, entry.config);
            }
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
