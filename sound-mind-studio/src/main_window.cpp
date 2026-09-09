#include "sound_mind/studio/main_window.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <exception>
#include <stdexcept>

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QFileDialog>
#include <QImage>
#include <QInputDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/wav_file.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/layer_export.h"
#include "sound_mind/core/pooling.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/create_project_wizard.h"
#include "sound_mind/studio/landing_page.h"
#include "sound_mind/studio/layers_panel.h"
#include "sound_mind/studio/theme.h"

#ifndef SOUND_MIND_VERSION
#define SOUND_MIND_VERSION "unknown"
#endif

namespace sound_mind::studio {

namespace {
const char* kProjectFileFilter = "Sound Mind Projects (*.smproj)";
const char* kAudioFileFilter = "WAV Audio (*.wav)";
const char* kImageFileFilter = "Images (*.png *.jpg *.jpeg *.bmp *.tga *.webp)";
const char* kExportAudioFileFilter = "FLAC Audio (*.flac);;Ogg Vorbis Audio (*.ogg);;MP3 Audio (*.mp3)";
const char* kExportVideoFileFilter = "MP4 Video (*.mp4)";

/// @brief Maps a destination path's extension to a compressed audio format.
/// @return The matching format, or `std::nullopt` for an unrecognized
///         extension.
[[nodiscard]] std::optional<sound_mind::codec::CompressedAudioFormat> audioFormatFromExtension(
    const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    for (char& c : extension) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (extension == ".flac") {
        return sound_mind::codec::CompressedAudioFormat::Flac;
    }
    if (extension == ".ogg") {
        return sound_mind::codec::CompressedAudioFormat::Ogg;
    }
    if (extension == ".mp3") {
        return sound_mind::codec::CompressedAudioFormat::Mp3;
    }
    return std::nullopt;
}

/// @brief Shows `message` in `bar` and forces an immediate repaint.
///
/// Imports/exports/pooling are synchronous, blocking calls (see the
/// confirmed scope for this milestone - a background-thread model is
/// deferred to Loop Mode's real-time pipeline work) - without the explicit
/// processEvents() call, Qt wouldn't actually paint the status bar's new
/// text until *after* the blocking call already returned, defeating the
/// whole point of showing progress before a slow operation starts.
void showBusyStatus(QStatusBar* bar, const QString& message) {
    bar->showMessage(message);
    QCoreApplication::processEvents();
}

/// @brief Converts a QImage to codec::RgbImage, forcing a consistent 3-byte-
/// per-pixel layout first regardless of the source file's own format.
[[nodiscard]] sound_mind::codec::RgbImage toRgbImage(const QImage& source) {
    const QImage rgb888 = source.convertToFormat(QImage::Format_RGB888);

    sound_mind::codec::RgbImage image;
    image.width = static_cast<std::uint32_t>(rgb888.width());
    image.height = static_cast<std::uint32_t>(rgb888.height());
    image.pixels.resize(image.pixelCount() * 3);

    for (int y = 0; y < rgb888.height(); ++y) {
        const uchar* line = rgb888.constScanLine(y);
        std::memcpy(image.pixels.data() + static_cast<std::size_t>(y) * image.width * 3, line,
                    static_cast<std::size_t>(image.width) * 3);
    }
    return image;
}

/// @brief Converts a codec::RgbImage to a QImage, copying the pixel data
/// so the result stays valid independent of the source's own lifetime
/// (unlike CanvasWidget's paintEvent(), where the source stays alive for
/// the whole synchronous paint call and a copy would be wasted work).
[[nodiscard]] QImage toQImage(const sound_mind::codec::RgbImage& image) {
    return QImage(image.pixels.data(), static_cast<int>(image.width), static_cast<int>(image.height),
                  static_cast<int>(image.width) * 3, QImage::Format_RGB888)
        .copy();
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Sound Mind Studio v" SOUND_MIND_VERSION));
    setWindowIcon(studioWindowIcon());
    resize(800, 600);

    landingPage_ = new LandingPage(this);
    landingPage_->setRecentProjects(recentProjects_.list());
    connect(landingPage_, &LandingPage::newProjectRequested, this, &MainWindow::newProject);
    connect(landingPage_, &LandingPage::openProjectRequested, this, &MainWindow::openProject);
    connect(landingPage_, &LandingPage::recentProjectRequested, this, [this](const QString& path) {
        // Same guard as openProject() - see its docs - before reaching
        // openProjectAt(), which enforces the Loop Mode/Recording refusal
        // on its own but never prompts about unsaved changes itself.
        if (!confirmDiscardUnsavedChanges()) {
            return;
        }
        QString errorMessage;
        if (!openProjectAt(std::filesystem::path(path.toStdString()), &errorMessage)) {
            QMessageBox::critical(this, tr("Open Project Failed"), errorMessage);
        }
    });

    canvas_ = new CanvasWidget(this);

    stack_ = new QStackedWidget(this);
    stack_->addWidget(landingPage_);  // index 0 - shown first, see setProject().
    stack_->addWidget(canvas_);       // index 1
    setCentralWidget(stack_);

    layersPanel_ = new LayersPanel(this);
    layersPanel_->hide();  // nothing to show until setProject() - see refreshLayersPanel()'s docs.
    addDockWidget(Qt::RightDockWidgetArea, layersPanel_);
    connect(layersPanel_, &LayersPanel::visibilityToggled, this, &MainWindow::toggleLayerVisibility);
    connect(layersPanel_, &LayersPanel::opacityChanged, this, &MainWindow::setLayerOpacity);
    connect(layersPanel_, &LayersPanel::renameRequested, this, &MainWindow::renameLayer);
    connect(layersPanel_, &LayersPanel::deleteRequested, this, &MainWindow::deleteLayer);
    connect(layersPanel_, &LayersPanel::reorderRequested, this, &MainWindow::reorderLayers);

    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    QAction* newAction = fileMenu->addAction(tr("&New Project"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newProject);

    QAction* openAction = fileMenu->addAction(tr("&Open Project..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openProject);

    fileMenu->addSeparator();

    QAction* saveAction = fileMenu->addAction(tr("&Save Project"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveProject);

    QAction* saveAsAction = fileMenu->addAction(tr("Save Project &As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveProjectAs);

    fileMenu->addSeparator();

    QAction* importAudioAction = fileMenu->addAction(tr("Import &Audio..."));
    connect(importAudioAction, &QAction::triggered, this, &MainWindow::importAudio);

    QAction* importImageAction = fileMenu->addAction(tr("Import &Image..."));
    connect(importImageAction, &QAction::triggered, this, &MainWindow::importImage);

    fileMenu->addSeparator();

    QAction* exportAudioAction = fileMenu->addAction(tr("&Export Audio..."));
    connect(exportAudioAction, &QAction::triggered, this, &MainWindow::exportAudio);

    QAction* exportVideoAction = fileMenu->addAction(tr("Export &Video..."));
    connect(exportVideoAction, &QAction::triggered, this, &MainWindow::exportVideo);

    QToolBar* transportToolBar = addToolBar(tr("Transport"));
    // Plain text actions rather than icons - no icon assets exist yet, and
    // these are unambiguous enough on their own for a first pass.
    QAction* playAction = transportToolBar->addAction(tr("Play"));
    connect(playAction, &QAction::triggered, this, &MainWindow::startPlayback);

    QAction* pauseAction = transportToolBar->addAction(tr("Pause"));
    connect(pauseAction, &QAction::triggered, this, &MainWindow::pausePlayback);

    QAction* stopAction = transportToolBar->addAction(tr("Stop"));
    connect(stopAction, &QAction::triggered, this, &MainWindow::stopPlayback);

    QAction* poolAction = transportToolBar->addAction(tr("Pool Layer"));
    connect(poolAction, &QAction::triggered, this, &MainWindow::poolTopmostLayer);

    QAction* loopAction = transportToolBar->addAction(tr("Loop"));
    loopAction->setCheckable(true);
    connect(loopAction, &QAction::triggered, this, &MainWindow::toggleLoopMode);

    // The "Keep Looping" checkbox - a plain QCheckBox rather than a
    // QAction, since it needs to show its own label/checkmark inline
    // rather than toggling a single button's own pressed state (there's no
    // icon asset to use for it either - matching the plain-text-action
    // precedent above). Forwards straight to setKeepLooping() - see its own
    // docs.
    keepLoopingCheckBox_ = new QCheckBox(tr("Keep Looping"), this);
    connect(keepLoopingCheckBox_, &QCheckBox::toggled, this, &MainWindow::setKeepLooping);
    transportToolBar->addWidget(keepLoopingCheckBox_);

    // ~30fps - frequent enough for each completed loop's spectrogram
    // update to read as prompt, without repainting so often it competes
    // noticeably with the background encode/decode worker thread (see
    // LoopEngine's docs) for CPU time.
    loopUpdateTimer_ = new QTimer(this);
    loopUpdateTimer_->setInterval(33);
    connect(loopUpdateTimer_, &QTimer::timeout, this, &MainWindow::updateLoopLayer);

    QAction* recordAction = transportToolBar->addAction(tr("Record"));
    recordAction->setCheckable(true);
    connect(recordAction, &QAction::triggered, this, &MainWindow::toggleRecording);

    // Just needs to keep RecordEngine's ring buffer (~370ms of headroom at
    // its default capacity/sample rate) from ever filling up - unlike
    // loopUpdateTimer_, nothing visual depends on this cadence, so a
    // slower interval with a comfortable safety margin is fine.
    recordDrainTimer_ = new QTimer(this);
    recordDrainTimer_->setInterval(100);
    connect(recordDrainTimer_, &QTimer::timeout, this, &MainWindow::drainRecording);

    // No newProject() call here, per the Landing Page milestone (v0.Y.9.1):
    // the window starts with no project open at all, showing the Landing
    // Page (stack_ defaults to index 0) until New/Open Project actually
    // creates or loads one - see setProject()'s and isShowingLandingPage()'s
    // docs.
}

const sound_mind::core::Project* MainWindow::project() const noexcept {
    return project_ ? &*project_ : nullptr;
}

bool MainWindow::isShowingLandingPage() const noexcept { return stack_->currentWidget() == landingPage_; }

bool MainWindow::hasUnsavedChanges() const noexcept { return hasUnsavedChanges_; }

void MainWindow::closeEvent(QCloseEvent* event) {
    if ((loopEngine_ && loopEngine_->isRunning()) || recordEngine_.isRecording()) {
        statusBar()->showMessage(tr("Stop Loop Mode or Recording before closing."), 5000);
        event->ignore();
        return;
    }
    if (!confirmDiscardUnsavedChanges()) {
        event->ignore();
        return;
    }
    event->accept();
}

bool MainWindow::confirmDiscardUnsavedChanges() {
    if (!hasUnsavedChanges_) {
        return true;
    }

    const auto choice =
        QMessageBox::warning(this, tr("Unsaved Changes"), tr("This project has unsaved changes. Save them first?"),
                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Cancel) {
        return false;
    }
    if (choice == QMessageBox::Discard) {
        return true;
    }

    // Save. saveProject() clears hasUnsavedChanges_ only once the save
    // actually completes - still true afterward means the user cancelled
    // saveProjectAs()'s file picker, or the save itself failed (already
    // reported via its own QMessageBox::critical()), so this correctly
    // still refuses to proceed either way.
    saveProject();
    return !hasUnsavedChanges_;
}

void MainWindow::setProject(sound_mind::core::Project project) {
    // Defensive invariant, not the normal path - see the class docs'
    // v0.Y.10.1 note. Unconditional and idempotent (both engines' stop()
    // no-op when already stopped, or when loopEngine_ doesn't exist yet -
    // see its own docs), so this is always safe regardless of caller.
    loopUpdateTimer_->stop();
    if (loopEngine_) {
        loopEngine_->stop();
    }
    loopLayerId_.reset();
    recordDrainTimer_->stop();
    recordEngine_.stop();

    project_ = std::move(project);

    // (Re)constructed fresh for the new project's own settings - loop
    // length is the project's own duration in samples (canvasWidth
    // timeline columns, each hopLength samples wide) - see the class docs'
    // v0.Y.12.1 note for why this replaced a fixed, always-default-
    // constructed member.
    const sound_mind::core::ProjectSettings& settings = project_->settings();
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    const auto loopLengthSamples = static_cast<std::size_t>(settings.canvasWidth) * config.hopLength;
    loopEngine_ = std::make_unique<sound_mind::core::LoopEngine>(config, loopLengthSamples);

    canvas_->setProject(&*project_);
    playbackEngine_.stop();
    playbackLoaded_ = false;
    hasUnsavedChanges_ = false;
    stack_->setCurrentWidget(canvas_);
    layersPanel_->show();
    refreshLayersPanel();
}

void MainWindow::newProject() {
    if ((loopEngine_ && loopEngine_->isRunning()) || recordEngine_.isRecording()) {
        statusBar()->showMessage(tr("Stop Loop Mode or Recording before starting a new project."), 5000);
        return;
    }
    if (!confirmDiscardUnsavedChanges()) {
        return;
    }

    CreateProjectWizard wizard(this);
    if (wizard.exec() != QDialog::Accepted) {
        return;
    }

    QString errorMessage;
    if (!createProjectAt(wizard.settings(), wizard.path(), &errorMessage)) {
        QMessageBox::critical(this, tr("Save Project Failed"), errorMessage);
    }
}

bool MainWindow::createProjectAt(sound_mind::core::ProjectSettings settings, const std::filesystem::path& path,
                                  QString* errorMessage) {
    setProject(sound_mind::core::Project::createNew(std::move(settings)));
    currentPath_ = path;

    try {
        project_->save(path);
        recentProjects_.add(path);
        landingPage_->setRecentProjects(recentProjects_.list());
        return true;
    } catch (const std::exception& e) {
        hasUnsavedChanges_ = true;
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
}

void MainWindow::openProject() {
    if ((loopEngine_ && loopEngine_->isRunning()) || recordEngine_.isRecording()) {
        statusBar()->showMessage(tr("Stop Loop Mode or Recording before opening a project."), 5000);
        return;
    }
    if (!confirmDiscardUnsavedChanges()) {
        return;
    }

    const QString fileName =
        QFileDialog::getOpenFileName(this, tr("Open Project"), QString(), tr(kProjectFileFilter));
    if (fileName.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!openProjectAt(std::filesystem::path(fileName.toStdString()), &errorMessage)) {
        QMessageBox::critical(this, tr("Open Project Failed"), errorMessage);
    }
}

bool MainWindow::openProjectAt(const std::filesystem::path& path, QString* errorMessage) {
    if ((loopEngine_ && loopEngine_->isRunning()) || recordEngine_.isRecording()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Stop Loop Mode or Recording before opening a project.");
        }
        return false;
    }

    try {
        setProject(sound_mind::core::Project::load(path));
        currentPath_ = path;
        recentProjects_.add(path);
        landingPage_->setRecentProjects(recentProjects_.list());
        return true;
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
}

void MainWindow::saveProject() {
    if (!project_) {
        return;
    }
    if (!currentPath_) {
        saveProjectAs();
        return;
    }

    try {
        project_->save(*currentPath_);
        recentProjects_.add(*currentPath_);
        landingPage_->setRecentProjects(recentProjects_.list());
        hasUnsavedChanges_ = false;
    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Save Project Failed"), QString::fromStdString(e.what()));
    }
}

void MainWindow::saveProjectAs() {
    if (!project_) {
        return;
    }

    const QString fileName =
        QFileDialog::getSaveFileName(this, tr("Save Project As"), QString(), tr(kProjectFileFilter));
    if (fileName.isEmpty()) {
        return;
    }

    currentPath_ = std::filesystem::path(fileName.toStdString());
    saveProject();
}

void MainWindow::importAudio() {
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Import Audio"), QString(), tr(kAudioFileFilter));
    if (fileName.isEmpty()) {
        return;
    }
    QString errorMessage;
    if (!importAudioFile(std::filesystem::path(fileName.toStdString()), &errorMessage)) {
        QMessageBox::critical(this, tr("Import Audio Failed"), errorMessage);
    }
}

void MainWindow::importImage() {
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Import Image"), QString(), tr(kImageFileFilter));
    if (fileName.isEmpty()) {
        return;
    }
    QString errorMessage;
    if (!importImageFile(std::filesystem::path(fileName.toStdString()), &errorMessage)) {
        QMessageBox::critical(this, tr("Import Image Failed"), errorMessage);
    }
}

bool MainWindow::importAudioFile(const std::filesystem::path& path, QString* errorMessage) {
    if (!project_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No project is open.");
        }
        return false;
    }

    showBusyStatus(statusBar(), tr("Importing audio..."));
    try {
        const auto audio = sound_mind::codec::readWavFile(path);
        const auto content = sound_mind::codec::encode(audio, sound_mind::core::streamCodecConfigFor(project_->settings()));

        sound_mind::core::Layer layer(0, path.filename().string(), sound_mind::core::LayerType::Normal);
        layer.setContent(content);
        project_->addLayer(std::move(layer));
        canvas_->update();
        // The topmost layer just changed - the next startPlayback() should
        // pick up the newly imported one instead of whatever was loaded
        // before, rather than silently keep playing stale content.
        playbackLoaded_ = false;
        hasUnsavedChanges_ = true;
        refreshLayersPanel();
        statusBar()->showMessage(tr("Imported \"%1\".").arg(QString::fromStdString(path.filename().string())), 5000);
        return true;
    } catch (const std::exception& e) {
        statusBar()->clearMessage();
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
}

bool MainWindow::importImageFile(const std::filesystem::path& path, QString* errorMessage) {
    if (!project_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No project is open.");
        }
        return false;
    }

    const QImage sourceImage(QString::fromStdString(path.string()));
    if (sourceImage.isNull()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Could not load the image file.");
        }
        return false;
    }

    showBusyStatus(statusBar(), tr("Importing image..."));
    try {
        const auto rgbImage = toRgbImage(sourceImage);
        const auto content =
            sound_mind::codec::fromRgbImage(rgbImage, sound_mind::core::streamCodecConfigFor(project_->settings()));

        sound_mind::core::Layer layer(0, path.filename().string(), sound_mind::core::LayerType::Normal);
        layer.setContent(content);
        project_->addLayer(std::move(layer));
        canvas_->update();
        playbackLoaded_ = false;
        hasUnsavedChanges_ = true;
        refreshLayersPanel();
        statusBar()->showMessage(tr("Imported \"%1\".").arg(QString::fromStdString(path.filename().string())), 5000);
        return true;
    } catch (const std::exception& e) {
        statusBar()->clearMessage();
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
}

void MainWindow::exportAudio() {
    const QString fileName =
        QFileDialog::getSaveFileName(this, tr("Export Audio"), QString(), tr(kExportAudioFileFilter));
    if (fileName.isEmpty()) {
        return;
    }
    QString errorMessage;
    if (!exportTopmostLayerAudioNow(std::filesystem::path(fileName.toStdString()), &errorMessage)) {
        QMessageBox::critical(this, tr("Export Audio Failed"), errorMessage);
    }
}

void MainWindow::exportVideo() {
    const QString fileName =
        QFileDialog::getSaveFileName(this, tr("Export Video"), QString(), tr(kExportVideoFileFilter));
    if (fileName.isEmpty()) {
        return;
    }
    QString errorMessage;
    if (!exportTopmostLayerVideoNow(std::filesystem::path(fileName.toStdString()), &errorMessage)) {
        QMessageBox::critical(this, tr("Export Video Failed"), errorMessage);
    }
}

bool MainWindow::exportTopmostLayerAudioNow(const std::filesystem::path& path, QString* errorMessage) {
    sound_mind::core::Layer* layer = topmostLayerWithContent();
    if (layer == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No layer with content to export.");
        }
        return false;
    }

    const auto format = audioFormatFromExtension(path);
    if (!format.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Unrecognized audio file extension - use .flac, .ogg, or .mp3.");
        }
        return false;
    }

    showBusyStatus(statusBar(), tr("Exporting audio..."));
    try {
        if (!sound_mind::core::exportLayerAudio(*layer, path, *format)) {
            statusBar()->clearMessage();
            return false;
        }
        statusBar()->showMessage(tr("Exported audio to \"%1\".").arg(QString::fromStdString(path.string())), 5000);
        return true;
    } catch (const std::exception& e) {
        statusBar()->clearMessage();
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
}

bool MainWindow::exportTopmostLayerVideoNow(const std::filesystem::path& path, QString* errorMessage) {
    sound_mind::core::Layer* layer = topmostLayerWithContent();
    if (layer == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No layer with content to export.");
        }
        return false;
    }

    showBusyStatus(statusBar(), tr("Exporting video..."));
    try {
        if (!sound_mind::core::exportLayerVideo(*layer, path)) {
            statusBar()->clearMessage();
            return false;
        }
        statusBar()->showMessage(tr("Exported video to \"%1\".").arg(QString::fromStdString(path.string())), 5000);
        return true;
    } catch (const std::exception& e) {
        statusBar()->clearMessage();
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
}

sound_mind::core::Layer* MainWindow::topmostLayerWithContent() {
    if (!project_) {
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

sound_mind::core::Layer* MainWindow::layerById(sound_mind::core::LayerId id) {
    if (!project_) {
        return nullptr;
    }
    for (auto& layer : project_->layers()) {
        if (layer.id() == id) {
            return &layer;
        }
    }
    return nullptr;
}

void MainWindow::refreshLayersPanel() {
    std::vector<LayersPanel::RowData> rows;
    if (project_) {
        rows.reserve(project_->layers().size());
        for (const auto& layer : project_->layers()) {
            LayersPanel::RowData row;
            row.id = layer.id();
            row.name = QString::fromStdString(layer.name());
            row.type = layer.type();
            row.opacity = layer.opacity();
            row.visible = layer.visible();
            rows.push_back(row);
        }
    }
    layersPanel_->setLayers(rows);
}

void MainWindow::toggleLayerVisibility(sound_mind::core::LayerId id, bool visible) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    layer->setVisible(visible);
    hasUnsavedChanges_ = true;
    playbackLoaded_ = false;  // "topmost layer with content" may have changed.
    canvas_->update();
    refreshLayersPanel();
}

void MainWindow::setLayerOpacity(sound_mind::core::LayerId id, float opacity) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    layer->setOpacity(opacity);
    hasUnsavedChanges_ = true;
    refreshLayersPanel();
}

void MainWindow::renameLayer(sound_mind::core::LayerId id) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    bool ok = false;
    const QString newName = QInputDialog::getText(this, tr("Rename Layer"), tr("Name:"), QLineEdit::Normal,
                                                    QString::fromStdString(layer->name()), &ok);
    if (!ok) {
        return;
    }
    renameLayerTo(id, newName);
}

bool MainWindow::renameLayerTo(sound_mind::core::LayerId id, const QString& newName) {
    if (newName.isEmpty()) {
        return false;
    }
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return false;
    }
    layer->setName(newName.toStdString());
    hasUnsavedChanges_ = true;
    refreshLayersPanel();
    return true;
}

void MainWindow::deleteLayer(sound_mind::core::LayerId id) {
    if (!project_) {
        return;
    }
    const sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    if (layer->type() == sound_mind::core::LayerType::Background ||
        layer->type() == sound_mind::core::LayerType::Equalizer) {
        // Defense in depth - LayersPanel doesn't even show a delete
        // button for these, but refuse here too regardless of caller.
        return;
    }

    if (project_->removeLayer(id)) {
        hasUnsavedChanges_ = true;
        playbackLoaded_ = false;
        canvas_->update();
        refreshLayersPanel();
    }
}

void MainWindow::reorderLayers(const std::vector<sound_mind::core::LayerId>& newOrderBottomToTop) {
    if (!project_) {
        return;
    }
    if (project_->reorderLayers(newOrderBottomToTop)) {
        hasUnsavedChanges_ = true;
        canvas_->update();
    }
    // Refreshed either way - even a rejected reorder needs the panel
    // snapped back to the authoritative order (see LayersPanel::
    // reorderRequested()'s docs).
    refreshLayersPanel();
}

void MainWindow::startPlayback() {
    if (!project_) {
        return;
    }
    if ((loopEngine_ && loopEngine_->isRunning()) || recordEngine_.isRecording()) {
        // Mutually exclusive with Loop Mode and Recording - see
        // toggleLoopMode()'s/toggleRecording()'s docs.
        return;
    }

    if (!playbackLoaded_) {
        const sound_mind::core::Layer* layer = topmostLayerWithContent();
        if (layer == nullptr) {
            return;
        }
        playbackEngine_.loadAudio(sound_mind::codec::decode(*layer->content()));
        playbackLoaded_ = true;
    }

    playbackEngine_.play();
}

void MainWindow::pausePlayback() {
    playbackEngine_.pause();
}

void MainWindow::stopPlayback() {
    playbackEngine_.stop();
    playbackLoaded_ = false;
}

void MainWindow::toggleLoopMode() {
    if (!loopEngine_) {
        // No project has ever been opened yet - loopEngine_ doesn't exist
        // until setProject() has run once, since the loop length itself is
        // derived from a project's own duration - nothing to start or stop
        // - see the class docs' v0.Y.12.1 note.
        return;
    }

    if (loopEngine_->isRunning()) {
        loopUpdateTimer_->stop();
        loopEngine_->stop();
        loopLayerId_.reset();
        statusBar()->showMessage(tr("Loop capture stopped."), 5000);
        return;
    }

    if (recordEngine_.isRecording()) {
        // Refuse rather than surprise-stop an in-progress recording - both
        // would otherwise want the same input device at once.
        return;
    }

    stopPlayback();

    // Reuse an existing "Loop Input" layer if the project already has one
    // (from an earlier Loop Mode session this run, or reloaded from disk)
    // rather than creating a new one every time - starting Loop Mode again
    // keeps building on the same layer, so its previous content stays
    // visible immediately instead of the canvas going blank again while
    // the first loop of the new session is still being captured.
    const std::string loopInputName = tr("Loop Input").toStdString();
    sound_mind::core::Layer* existing = nullptr;
    for (sound_mind::core::Layer& candidate : project_->layers()) {
        if (candidate.type() == sound_mind::core::LayerType::Normal && candidate.name() == loopInputName) {
            existing = &candidate;
            break;
        }
    }
    if (existing != nullptr) {
        loopLayerId_ = existing->id();
    } else {
        sound_mind::core::Layer layer(0, loopInputName, sound_mind::core::LayerType::Normal);
        loopLayerId_ = project_->addLayer(std::move(layer));
    }
    hasUnsavedChanges_ = true;
    refreshLayersPanel();

    loopEngine_->start();
    if (!loopEngine_->isDeviceAvailable()) {
        statusBar()->showMessage(
            tr("Loop capture started, but no input device is available - nothing will be captured."), 5000);
    } else {
        statusBar()->showMessage(tr("Looping..."));
    }
    loopUpdateTimer_->start();
}

void MainWindow::setKeepLooping(bool keepLooping) {
    if (!loopEngine_) {
        return;
    }
    loopEngine_->setKeepLooping(keepLooping);
}

void MainWindow::updateLoopLayer() {
    if (!loopLayerId_ || !loopEngine_) {
        return;
    }
    sound_mind::core::Layer* layer = layerById(*loopLayerId_);
    if (layer == nullptr) {
        return;
    }

    sound_mind::codec::StreamImage image = loopEngine_->currentImage();
    if (image.frameCount > 0) {
        layer->setContent(std::move(image));
        canvas_->update();
    }

    // The confirmed scope's "visible loop-delay indicator" - see
    // LoopEngine::loopsBehind()'s own docs for exactly what this counts.
    const std::uint64_t loopsBehind = loopEngine_->loopsBehind();
    if (loopsBehind > 0) {
        statusBar()->showMessage(
            tr("Looping... (%1 loop%2 behind)").arg(loopsBehind).arg(loopsBehind == 1 ? QString() : tr("s")));
    } else {
        statusBar()->showMessage(tr("Looping..."));
    }
}

bool MainWindow::isPlaying() const noexcept {
    return playbackEngine_.isPlaying();
}

bool MainWindow::isLoopModeRunning() const noexcept {
    return loopEngine_ && loopEngine_->isRunning();
}

bool MainWindow::keepLooping() const noexcept {
    return loopEngine_ && loopEngine_->keepLooping();
}

bool MainWindow::isRecording() const noexcept {
    return recordEngine_.isRecording();
}

void MainWindow::toggleRecording() {
    if (recordEngine_.isRecording()) {
        recordDrainTimer_->stop();
        recordEngine_.stop();

        const sound_mind::codec::AudioBuffer& captured = recordEngine_.capturedAudio();
        if (!project_ || captured.frameCount() == 0) {
            statusBar()->showMessage(tr("Recording stopped - nothing captured."), 5000);
            return;
        }

        try {
            // Encoded exactly as an imported file would be - a one-shot
            // whole-buffer encode(), same as LoopEngine now does per loop
            // (see its own docs) - just triggered once, at Recording's own
            // end, rather than every loop.
            const auto content =
                sound_mind::codec::encode(captured, sound_mind::core::streamCodecConfigFor(project_->settings()));
            sound_mind::core::Layer layer(0, tr("Recording").toStdString(), sound_mind::core::LayerType::Normal);
            layer.setContent(content);
            project_->addLayer(std::move(layer));
            canvas_->update();
            playbackLoaded_ = false;
            hasUnsavedChanges_ = true;
            refreshLayersPanel();
            statusBar()->showMessage(tr("Recording added as a new layer."), 5000);
        } catch (const std::exception& e) {
            QMessageBox::critical(this, tr("Record Failed"), QString::fromStdString(e.what()));
        }
        return;
    }

    if (!project_) {
        return;
    }
    if (loopEngine_ && loopEngine_->isRunning()) {
        // Refuse rather than surprise-stop a running Loop Mode session -
        // both would otherwise want the same input device at once.
        return;
    }

    stopPlayback();

    recordEngine_.start();
    if (!recordEngine_.isDeviceAvailable()) {
        statusBar()->showMessage(tr("Recording started, but no input device is available - nothing will be captured."),
                                  5000);
    } else {
        statusBar()->showMessage(tr("Recording..."));
    }
    recordDrainTimer_->start();
}

void MainWindow::drainRecording() {
    recordEngine_.drainAvailable();
}

void MainWindow::poolTopmostLayer() {
    QString errorMessage;
    // Success is reported via the status bar (see poolTopmostLayerNow()),
    // not a modal - only a failure needs one here, since it's the one
    // outcome the status bar's "helpful, not intrusive" role isn't
    // appropriate for (per the confirmed scope for this milestone: a
    // missed error is worse than an intrusive one).
    if (!poolTopmostLayerNow(&errorMessage, nullptr, nullptr)) {
        QMessageBox::critical(this, tr("Pool Layer Failed"), errorMessage);
    }
}

bool MainWindow::poolTopmostLayerNow(QString* errorMessage, QString* streamPngPath, QString* poolPngPath) {
    sound_mind::core::Layer* layer = topmostLayerWithContent();
    if (layer == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No layer with content to pool.");
        }
        return false;
    }

    showBusyStatus(statusBar(), tr("Pooling layer..."));
    try {
        if (!sound_mind::core::poolLayer(*layer)) {
            statusBar()->clearMessage();
            if (errorMessage != nullptr) {
                *errorMessage = tr("Pooling failed unexpectedly.");
            }
            return false;
        }
        canvas_->update();
        // The layer's content was just replaced with a fresh, pool-derived
        // Stream copy - the next startPlayback() should pick that up
        // rather than continue playing whatever was loaded before.
        playbackLoaded_ = false;
        hasUnsavedChanges_ = true;

        const QString base = QString::fromStdString((std::filesystem::temp_directory_path() / "sound-mind-pool-compare").string());
        const QString streamPath = base + "-stream.png";
        const QString poolPath = base + "-pool.png";

        if (!toQImage(sound_mind::codec::toRgbImage(*layer->content())).save(streamPath)) {
            throw std::runtime_error("could not write Stream comparison image");
        }
        if (!toQImage(sound_mind::codec::toRgbImage(*layer->poolContent())).save(poolPath)) {
            throw std::runtime_error("could not write Pool comparison image");
        }

        if (streamPngPath != nullptr) {
            *streamPngPath = streamPath;
        }
        if (poolPngPath != nullptr) {
            *poolPngPath = poolPath;
        }
        statusBar()->showMessage(
            tr("Pooled layer \"%1\". Stream render: %2, Pool render: %3")
                .arg(QString::fromStdString(layer->name()), streamPath, poolPath),
            5000);
        return true;
    } catch (const std::exception& e) {
        statusBar()->clearMessage();
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
}

}  // namespace sound_mind::studio
