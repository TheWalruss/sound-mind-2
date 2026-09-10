#include "sound_mind/studio/main_window.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <map>
#include <memory>
#include <exception>
#include <sstream>
#include <stdexcept>

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QImage>
#include <QInputDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QUrl>

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/wav_file.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/layer_export.h"
#include "sound_mind/core/pooling.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/audio_snippet_picker_dialog.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/create_project_wizard.h"
#include "sound_mind/studio/image_scale_picker_dialog.h"
#include "sound_mind/studio/landing_page.h"
#include "sound_mind/studio/layers_panel.h"
#include "sound_mind/studio/loop_panel.h"
#include "sound_mind/studio/playback_panel.h"
#include "sound_mind/studio/record_panel.h"
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

/// @brief `path`'s extension, lowercased - the shared normalization both
/// dropEvent() and handleDroppedFiles() need to recognize a dropped file's
/// type case-insensitively (`.PNG` and `.png` are the same file type).
[[nodiscard]] std::string lowercasedExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    for (char& c : extension) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return extension;
}

/// @brief Whether `lowercaseExtension` is one of the image extensions
/// `ImageScalePickerDialog`/`importImageFiles()` accept - see
/// kImageFileFilter above for the same list in QFileDialog's own syntax.
[[nodiscard]] bool isImageExtension(const std::string& lowercaseExtension) {
    return lowercaseExtension == ".png" || lowercaseExtension == ".jpg" || lowercaseExtension == ".jpeg" ||
           lowercaseExtension == ".bmp" || lowercaseExtension == ".tga" || lowercaseExtension == ".webp";
}

/// @brief Converts a plain std::string device-name list (as the engines'
/// availableXDeviceNames() methods return) into the QStringList a device
/// picker combo box actually wants.
[[nodiscard]] QStringList toQStringList(const std::vector<std::string>& names) {
    QStringList result;
    result.reserve(static_cast<int>(names.size()));
    for (const std::string& name : names) {
        result.append(QString::fromStdString(name));
    }
    return result;
}

/// @brief Zero-pads a snippet index to four digits ("0000", "0001", ...) -
/// matching the legacy Studio's own `name_0000`/`name_0001`/... naming
/// convention for a multi-snippet audio import.
[[nodiscard]] std::string formatSnippetIndex(std::size_t index) {
    std::ostringstream stream;
    stream << std::setw(4) << std::setfill('0') << index;
    return stream.str();
}

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

/// @brief Resizes `source` to the project's canvas dimensions according to
/// `mode` - see `ImageScalePickerDialog::Mode`'s own docs for exactly what
/// each value means. `Qt::IgnoreAspectRatio` is used throughout, including
/// for `ScaleVerticalProportional`, since that mode's own proportional
/// width is already computed by hand below - asking Qt to *also* fit an
/// aspect ratio on top would risk a slightly different rounding than the
/// one this method's own docs promise.
[[nodiscard]] QImage scaleImageForImport(const QImage& source, sound_mind::studio::ImageScalePickerDialog::Mode mode,
                                          int canvasWidth, int canvasHeight) {
    using Mode = sound_mind::studio::ImageScalePickerDialog::Mode;
    switch (mode) {
        case Mode::RescaleToFitProject:
            return source.scaled(canvasWidth, canvasHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        case Mode::ScaleVerticalKeepHorizontal:
            return source.scaled(source.width(), canvasHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        case Mode::ScaleHorizontalKeepVertical:
            return source.scaled(canvasWidth, source.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        case Mode::ScaleVerticalProportional: {
            const int proportionalWidth =
                source.height() > 0
                    ? std::max(1, static_cast<int>(std::lround(static_cast<double>(source.width()) * canvasHeight /
                                                                source.height())))
                    : canvasWidth;
            return source.scaled(proportionalWidth, canvasHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        case Mode::KeepNativeResolution:
            return source;
    }
    return source;  // unreachable - every Mode value is handled above.
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
    setAcceptDrops(true);  // see dragEnterEvent()/dropEvent()'s own docs.

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
    connect(layersPanel_, &LayersPanel::translationChanged, this, &MainWindow::setLayerTranslation);
    connect(layersPanel_, &LayersPanel::rescaleChanged, this, &MainWindow::setLayerRescale);
    connect(layersPanel_, &LayersPanel::renameRequested, this, &MainWindow::renameLayer);
    connect(layersPanel_, &LayersPanel::deleteRequested, this, &MainWindow::deleteLayer);
    connect(layersPanel_, &LayersPanel::reorderRequested, this, &MainWindow::reorderLayers);

    // Playback/Record/Loop each get their own dockable panel (v0.Y.16.1) -
    // hidden until setProject(), matching layersPanel_'s own "nothing to
    // control yet" treatment, even though playbackEngine_/recordEngine_
    // themselves exist regardless of project state.
    playbackPanel_ = new PlaybackPanel(this);
    playbackPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, playbackPanel_);
    connect(playbackPanel_, &PlaybackPanel::playRequested, this, &MainWindow::startPlayback);
    connect(playbackPanel_, &PlaybackPanel::pauseRequested, this, &MainWindow::pausePlayback);
    connect(playbackPanel_, &PlaybackPanel::stopRequested, this, &MainWindow::stopPlayback);
    connect(playbackPanel_, &PlaybackPanel::outputDeviceChanged, this, &MainWindow::setPlaybackOutputDevice);
    connect(playbackPanel_, &PlaybackPanel::volumePercentChanged, this, &MainWindow::setPlaybackVolume);
    connect(playbackPanel_, &PlaybackPanel::seekRequested, this, &MainWindow::seekPlayback);
    playbackPanel_->setOutputDevices(toQStringList(playbackEngine_.availableOutputDeviceNames()));

    recordPanel_ = new RecordPanel(this);
    recordPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, recordPanel_);
    connect(recordPanel_, &RecordPanel::toggleRequested, this, &MainWindow::toggleRecording);
    connect(recordPanel_, &RecordPanel::inputDeviceChanged, this, &MainWindow::setRecordInputDevice);
    recordPanel_->setInputDevices(toQStringList(recordEngine_.availableInputDeviceNames()));

    loopPanel_ = new LoopPanel(this);
    loopPanel_->hide();  // also needs setProject() - loopEngine_ doesn't exist until then.
    addDockWidget(Qt::RightDockWidgetArea, loopPanel_);
    connect(loopPanel_, &LoopPanel::toggleRequested, this, &MainWindow::toggleLoopMode);
    connect(loopPanel_, &LoopPanel::keepLoopingChanged, this, &MainWindow::setKeepLooping);
    connect(loopPanel_, &LoopPanel::inputDeviceChanged, this, &MainWindow::setLoopInputDevice);
    connect(loopPanel_, &LoopPanel::outputDeviceChanged, this, &MainWindow::setLoopOutputDevice);

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
    QAction* poolAction = transportToolBar->addAction(tr("Pool Layer"));
    connect(poolAction, &QAction::triggered, this, &MainWindow::poolTopmostLayer);

    // As of v0.Y.16.1 (Transport Panels): Play/Pause/Stop/Loop/Record are
    // no longer direct toolbar actions - each now lives inside its own
    // dock panel (see the panel construction above), and these three
    // toolbar actions are pure show/hide toggles for those docks.
    // toggleViewAction() is Qt's own ready-made action for exactly this -
    // it stays in sync with the dock's actual visibility automatically, no
    // manual signal wiring needed (unlike a hand-rolled checkable QAction
    // would).
    // The Layers panel gets the same kind of toggle - unlike the three
    // above (OFF by default, see setProject()'s own docs), it's ON by
    // default, matching its pre-existing "just show it" behavior; both are
    // confirmed with the user, along with visibility persisting across
    // project switches within a session rather than resetting every time.
    transportToolBar->addAction(layersPanel_->toggleViewAction());
    transportToolBar->addAction(playbackPanel_->toggleViewAction());
    transportToolBar->addAction(recordPanel_->toggleViewAction());
    transportToolBar->addAction(loopPanel_->toggleViewAction());

    // ~30fps - frequent enough for each completed loop's spectrogram
    // update to read as prompt, without repainting so often it competes
    // noticeably with the background encode/decode worker thread (see
    // LoopEngine's docs) for CPU time.
    loopUpdateTimer_ = new QTimer(this);
    loopUpdateTimer_->setInterval(33);
    connect(loopUpdateTimer_, &QTimer::timeout, this, &MainWindow::updateLoopLayer);

    // Same ~30fps cadence, for the same reason - a moving playhead/position
    // bar that visibly stutters would undercut the point of having one.
    playbackUpdateTimer_ = new QTimer(this);
    playbackUpdateTimer_->setInterval(33);
    connect(playbackUpdateTimer_, &QTimer::timeout, this, &MainWindow::updatePlaybackPosition);

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

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    std::vector<std::filesystem::path> paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            paths.emplace_back(url.toLocalFile().toStdString());
        }
    }

    if (paths.empty()) {
        event->ignore();
        return;
    }

    event->acceptProposedAction();

    std::vector<std::filesystem::path> imagePaths;
    std::vector<std::filesystem::path> audioPaths;
    for (const auto& path : paths) {
        const std::string extension = lowercasedExtension(path);
        if (isImageExtension(extension)) {
            imagePaths.push_back(path);
        } else if (extension == ".wav") {
            audioPaths.push_back(path);
        }
    }

    // Same choices File -> Import Audio/Image would offer, confirmed with
    // the user - see this method's own docs for the full reasoning.
    // Cancelling any one of these dialogs cancels the whole drop.
    ImageScalePickerDialog::Mode imageMode = ImageScalePickerDialog::Mode::RescaleToFitProject;
    bool importImagesAsSequence = false;
    if (!imagePaths.empty()) {
        ImageScalePickerDialog dialog(this, /*allowSequential=*/imagePaths.size() > 1);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        imageMode = dialog.selectedMode();
        importImagesAsSequence = dialog.importAsSequence();
    }

    std::map<std::filesystem::path, std::vector<std::size_t>> audioSnippetSelections;
    for (const auto& path : audioPaths) {
        QString snippetsError;
        const auto snippets = audioSnippetsForFile(path, &snippetsError);
        if (snippets.size() <= 1) {
            // 0 (unreadable - handleDroppedFiles() will report the real
            // error when it actually tries to import) or 1 (no real choice
            // to make) - no picker needed, same as importAudio()'s own
            // logic for a single file.
            continue;
        }
        AudioSnippetPickerDialog dialog(snippets, this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        audioSnippetSelections[path] = dialog.selectedIndices();
    }

    handleDroppedFiles(paths, imageMode, importImagesAsSequence, audioSnippetSelections);
}

void MainWindow::handleDroppedFiles(const std::vector<std::filesystem::path>& paths,
                                     ImageScalePickerDialog::Mode imageMode, bool importImagesAsSequence,
                                     const std::map<std::filesystem::path, std::vector<std::size_t>>& audioSnippetSelections) {
    std::vector<std::filesystem::path> imagePaths;
    for (const auto& path : paths) {
        if (isImageExtension(lowercasedExtension(path))) {
            imagePaths.push_back(path);
        }
    }
    if (!imagePaths.empty()) {
        QString errorMessage;
        if (!importImageFiles(imagePaths, imageMode, importImagesAsSequence, &errorMessage)) {
            statusBar()->showMessage(tr("Could not import image(s): %1").arg(errorMessage), 5000);
        }
    }

    for (const std::filesystem::path& path : paths) {
        const std::string extension = lowercasedExtension(path);

        QString errorMessage;
        if (extension == ".wav") {
            const auto selection = audioSnippetSelections.find(path);
            const bool ok = selection != audioSnippetSelections.end()
                                 ? importAudioSnippets(path, selection->second, &errorMessage)
                                 : importAudioFile(path, &errorMessage);
            if (!ok) {
                statusBar()->showMessage(
                    tr("Could not import \"%1\": %2").arg(QString::fromStdString(path.filename().string()), errorMessage),
                    5000);
            }
        } else if (isImageExtension(extension)) {
            continue;  // already handled above, as a batch.
        } else if (extension == ".smproj") {
            // Same guard as openProject() - see its docs - before reaching
            // openProjectAt(), which enforces the Loop Mode/Recording
            // refusal on its own but never prompts about unsaved changes
            // itself.
            if (!confirmDiscardUnsavedChanges()) {
                continue;
            }
            if (!openProjectAt(path, &errorMessage)) {
                statusBar()->showMessage(
                    tr("Could not open \"%1\": %2").arg(QString::fromStdString(path.filename().string()), errorMessage),
                    5000);
            }
        }
        // Every other extension is silently ignored - see this method's
        // own docs.
    }
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
    loopPanel_->setRunning(false);
    recordDrainTimer_->stop();
    recordEngine_.stop();
    recordPanel_->setRecording(false);
    playbackUpdateTimer_->stop();
    playbackPanel_->setDuration(0.0);
    canvas_->setPlayheadFraction(std::nullopt);

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
    loopPanel_->setInputDevices(toQStringList(loopEngine_->availableInputDeviceNames()));
    loopPanel_->setOutputDevices(toQStringList(loopEngine_->availableOutputDeviceNames()));

    canvas_->setProject(&*project_);
    playbackEngine_.stop();
    playbackLoaded_ = false;
    hasUnsavedChanges_ = false;
    stack_->setCurrentWidget(canvas_);
    // Layers is shown automatically the *first* time any project exists in
    // this session, then left alone - a user's own show/hide choice
    // (Playback/Record/Loop included, which start OFF and are never forced
    // here at all) persists across New/Open Project rather than resetting
    // every time, confirmed with the user.
    if (!layersPanelShownOnce_) {
        layersPanel_->show();
        // A real bug, found via manual testing: show() called directly
        // (bypassing toggleViewAction()) never updates that action's own
        // checked state - QDockWidget only syncs it *from* the action
        // (toggled(bool) -> show()/hide()), not the other way around, and
        // toggleViewAction() was already lazily created (checked false,
        // matching layersPanel_'s hidden state at the time) back in the
        // constructor when it was first added to the toolbar. Left
        // unsynced, the toolbar button's first click would silently no-op
        // (checked false -> true means "show", already shown) instead of
        // actually toggling anything - explicitly setting it here is what
        // keeps the button and the panel's real visibility in agreement
        // from this point on.
        layersPanel_->toggleViewAction()->setChecked(true);
        layersPanelShownOnce_ = true;
    }
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
    updateWindowTitle();

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
        updateWindowTitle();
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
    updateWindowTitle();
    saveProject();
}

void MainWindow::importAudio() {
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Import Audio"), QString(), tr(kAudioFileFilter));
    if (fileName.isEmpty()) {
        return;
    }
    const std::filesystem::path path(fileName.toStdString());

    QString snippetsError;
    const auto snippets = audioSnippetsForFile(path, &snippetsError);
    if (snippets.empty()) {
        QMessageBox::critical(this, tr("Import Audio Failed"),
                               snippetsError.isEmpty() ? tr("Could not read the audio file.") : snippetsError);
        return;
    }

    std::vector<std::size_t> indices;
    if (snippets.size() == 1) {
        // The common case - audio no longer than the project's own
        // duration - skips the picker entirely, matching this method's
        // pre-existing behavior exactly (see importAudio()'s own docs).
        indices.push_back(snippets.front().index);
    } else {
        AudioSnippetPickerDialog dialog(snippets, this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        indices = dialog.selectedIndices();
        if (indices.empty()) {
            return;
        }
    }

    QString errorMessage;
    if (!importAudioSnippets(path, indices, &errorMessage)) {
        QMessageBox::critical(this, tr("Import Audio Failed"), errorMessage);
    }
}

void MainWindow::importImage() {
    const QStringList fileNames =
        QFileDialog::getOpenFileNames(this, tr("Import Image(s)"), QString(), tr(kImageFileFilter));
    if (fileNames.isEmpty()) {
        return;
    }

    std::vector<std::filesystem::path> paths;
    paths.reserve(static_cast<std::size_t>(fileNames.size()));
    for (const QString& fileName : fileNames) {
        paths.emplace_back(fileName.toStdString());
    }

    ImageScalePickerDialog dialog(this, /*allowSequential=*/paths.size() > 1);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString errorMessage;
    if (!importImageFiles(paths, dialog.selectedMode(), dialog.importAsSequence(), &errorMessage)) {
        QMessageBox::critical(this, tr("Import Image Failed"), errorMessage);
    }
}

bool MainWindow::importAudioFile(const std::filesystem::path& path, QString* errorMessage) {
    QString snippetsError;
    const auto snippets = audioSnippetsForFile(path, &snippetsError);
    if (snippets.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = snippetsError;
        }
        return false;
    }

    std::vector<std::size_t> allIndices;
    allIndices.reserve(snippets.size());
    for (const auto& snippet : snippets) {
        allIndices.push_back(snippet.index);
    }
    return importAudioSnippets(path, allIndices, errorMessage);
}

std::vector<AudioSnippetPickerDialog::RowData> MainWindow::audioSnippetsForFile(const std::filesystem::path& path,
                                                                                 QString* errorMessage) const {
    if (!project_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No project is open.");
        }
        return {};
    }

    try {
        const auto audio = sound_mind::codec::readWavFile(path);
        const auto config = sound_mind::core::streamCodecConfigFor(project_->settings());
        const auto loopLengthSamples =
            static_cast<std::size_t>(project_->settings().canvasWidth) * static_cast<std::size_t>(config.hopLength);
        if (loopLengthSamples == 0 || audio.sampleRateHz == 0) {
            if (errorMessage != nullptr) {
                *errorMessage = tr("The project's own duration is zero - nothing to split against.");
            }
            return {};
        }

        const std::size_t totalSamples = audio.frameCount();
        const std::size_t snippetCount =
            std::max<std::size_t>((totalSamples + loopLengthSamples - 1) / loopLengthSamples, std::size_t{1});

        std::vector<AudioSnippetPickerDialog::RowData> result;
        result.reserve(snippetCount);
        for (std::size_t index = 0; index < snippetCount; ++index) {
            const std::size_t start = index * loopLengthSamples;
            const std::size_t end = std::min(start + loopLengthSamples, totalSamples);
            AudioSnippetPickerDialog::RowData row;
            row.index = index;
            row.startSeconds = static_cast<double>(start) / static_cast<double>(audio.sampleRateHz);
            row.endSeconds = static_cast<double>(end) / static_cast<double>(audio.sampleRateHz);
            result.push_back(row);
        }
        return result;
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return {};
    }
}

bool MainWindow::importAudioSnippets(const std::filesystem::path& path, const std::vector<std::size_t>& snippetIndices,
                                      QString* errorMessage) {
    if (!project_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No project is open.");
        }
        return false;
    }

    showBusyStatus(statusBar(), tr("Importing audio..."));
    try {
        const auto audio = sound_mind::codec::readWavFile(path);
        const auto config = sound_mind::core::streamCodecConfigFor(project_->settings());
        const auto loopLengthSamples =
            static_cast<std::size_t>(project_->settings().canvasWidth) * static_cast<std::size_t>(config.hopLength);
        const std::size_t totalSamples = audio.frameCount();
        const std::size_t snippetCount = loopLengthSamples > 0
                                              ? std::max<std::size_t>((totalSamples + loopLengthSamples - 1) / loopLengthSamples,
                                                                       std::size_t{1})
                                              : 1;

        // Sorted, de-duplicated so layers land in the project in ascending
        // snippet order regardless of the order the caller listed indices
        // in - a snippet picker's checked order needn't match position
        // order.
        std::vector<std::size_t> sortedIndices = snippetIndices;
        std::sort(sortedIndices.begin(), sortedIndices.end());
        sortedIndices.erase(std::unique(sortedIndices.begin(), sortedIndices.end()), sortedIndices.end());

        const std::string stem = path.stem().string();
        int importedCount = 0;
        for (const std::size_t index : sortedIndices) {
            if (index >= snippetCount) {
                continue;  // silently skipped - see this method's own docs.
            }
            const std::size_t start = loopLengthSamples > 0 ? index * loopLengthSamples : 0;
            const std::size_t end =
                loopLengthSamples > 0 ? std::min(start + loopLengthSamples, totalSamples) : totalSamples;
            if (start > end) {
                continue;
            }

            sound_mind::codec::AudioBuffer snippet;
            snippet.sampleRateHz = audio.sampleRateHz;
            snippet.left.assign(audio.left.begin() + static_cast<std::ptrdiff_t>(start),
                                 audio.left.begin() + static_cast<std::ptrdiff_t>(end));
            snippet.right.assign(audio.right.begin() + static_cast<std::ptrdiff_t>(start),
                                  audio.right.begin() + static_cast<std::ptrdiff_t>(end));

            const auto content = sound_mind::codec::encode(snippet, config);
            const std::string layerName =
                snippetCount > 1 ? stem + "_" + formatSnippetIndex(index) : path.filename().string();

            sound_mind::core::Layer layer(0, layerName, sound_mind::core::LayerType::Normal);
            layer.setContent(content);
            project_->addLayer(std::move(layer));
            ++importedCount;
        }

        if (importedCount == 0) {
            statusBar()->clearMessage();
            if (errorMessage != nullptr) {
                *errorMessage = tr("No snippets were imported.");
            }
            return false;
        }

        canvas_->update();
        // The topmost layer just changed - the next startPlayback() should
        // pick up the newly imported one instead of whatever was loaded
        // before, rather than silently keep playing stale content.
        playbackLoaded_ = false;
        hasUnsavedChanges_ = true;
        refreshLayersPanel();
        statusBar()->showMessage(
            tr("Imported %1 layer(s) from \"%2\".").arg(importedCount).arg(QString::fromStdString(path.filename().string())),
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

bool MainWindow::importImageFile(const std::filesystem::path& path, ImageScalePickerDialog::Mode mode,
                                  QString* errorMessage) {
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
        const auto& settings = project_->settings();
        const QImage scaledImage = scaleImageForImport(sourceImage, mode, static_cast<int>(settings.canvasWidth),
                                                        static_cast<int>(settings.canvasHeight));
        const auto rgbImage = toRgbImage(scaledImage);
        const auto content = sound_mind::codec::fromRgbImage(rgbImage, sound_mind::core::streamCodecConfigFor(settings));

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

bool MainWindow::importImageFiles(const std::vector<std::filesystem::path>& paths, ImageScalePickerDialog::Mode mode,
                                   bool importAsSequence, QString* errorMessage) {
    if (!project_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No project is open.");
        }
        return false;
    }
    if (paths.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No files to import.");
        }
        return false;
    }

    std::vector<std::filesystem::path> orderedPaths = paths;
    if (importAsSequence) {
        // Deterministic - the natural choice for numbered frame sequences
        // (frame001.png, frame002.png, ...) regardless of the file dialog's
        // own selection/return order, confirmed with the user before
        // implementing.
        std::sort(orderedPaths.begin(), orderedPaths.end());
    }

    const auto canvasWidth = static_cast<std::int64_t>(project_->settings().canvasWidth);
    std::int64_t cumulativeTranslation = 0;
    int importedCount = 0;
    QString firstError;

    for (const auto& path : orderedPaths) {
        const auto fileMode = importAsSequence ? ImageScalePickerDialog::Mode::ScaleVerticalProportional : mode;
        QString thisError;
        if (!importImageFile(path, fileMode, &thisError)) {
            if (firstError.isEmpty()) {
                firstError = thisError;
            }
            continue;
        }
        ++importedCount;

        if (importAsSequence) {
            // Wrap back to column 0 once the running total reaches
            // canvasWidth - matches the legacy Studio's own
            // cumulative-offset placement exactly (confirmed with the user
            // before implementing) rather than just letting later layers
            // keep extending past canvasWidth (which renderLayer() would
            // crop anyway, per its own Decision #25 padding/cropping).
            if (canvasWidth > 0 && cumulativeTranslation >= canvasWidth) {
                cumulativeTranslation = 0;
            }
            sound_mind::core::Layer& justImported = project_->layers().back();
            justImported.setTranslationColumns(cumulativeTranslation);
            const std::int64_t thisWidth =
                justImported.content().has_value() ? static_cast<std::int64_t>(justImported.content()->frameCount) : 0;
            cumulativeTranslation += thisWidth;
        }
    }

    if (importedCount == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = firstError.isEmpty() ? tr("No files were imported.") : firstError;
        }
        return false;
    }
    if (importAsSequence) {
        // Each importImageFile() call above already refreshed the canvas/
        // Layers Panel for its own layer - this just makes sure the final
        // translationColumns() changes made afterward are reflected too.
        canvas_->update();
        refreshLayersPanel();
    }
    return true;
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
        if (!sound_mind::core::exportLayerVideo(*layer, path, project_->settings().canvasWidth)) {
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

void MainWindow::updateWindowTitle() {
    QString title = QStringLiteral("Sound Mind Studio v" SOUND_MIND_VERSION);
    if (currentPath_) {
        title += QStringLiteral(" - ") + QString::fromStdString(currentPath_->stem().string());
    }
    setWindowTitle(title);
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
            row.translationColumns = layer.translationColumns();
            row.rescaleFactor = layer.rescaleFactor();
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

void MainWindow::setLayerTranslation(sound_mind::core::LayerId id, std::int64_t translationColumns) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    layer->setTranslationColumns(translationColumns);
    hasUnsavedChanges_ = true;
    canvas_->update();
    refreshLayersPanel();
}

void MainWindow::setLayerRescale(sound_mind::core::LayerId id, double rescaleFactor) {
    sound_mind::core::Layer* layer = layerById(id);
    if (layer == nullptr) {
        return;
    }
    layer->setRescaleFactor(rescaleFactor);
    hasUnsavedChanges_ = true;
    canvas_->update();
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
        // Duration only needs setting once per load, not on every resume -
        // setDuration() also resets the displayed position to 0:00, which
        // a mere pause/resume shouldn't do.
        const auto sampleRate = playbackEngine_.sampleRateHz();
        const double totalSeconds =
            sampleRate > 0 ? static_cast<double>(playbackEngine_.totalSamples()) / sampleRate : 0.0;
        playbackPanel_->setDuration(totalSeconds);
    }

    playbackEngine_.play();
    playbackUpdateTimer_->start();
}

void MainWindow::pausePlayback() {
    playbackEngine_.pause();
    // Position bar/playhead stay where they are - only stopPlayback()
    // resets them, matching "startPlayback() resumes from the same
    // position" - no point polling a position that isn't moving.
    playbackUpdateTimer_->stop();
}

void MainWindow::stopPlayback() {
    playbackEngine_.stop();
    playbackLoaded_ = false;
    playbackUpdateTimer_->stop();
    playbackPanel_->setDuration(0.0);
    canvas_->setPlayheadFraction(std::nullopt);
}

void MainWindow::seekPlayback(double positionSeconds) {
    if (!playbackLoaded_) {
        return;  // nothing loaded to seek within.
    }
    const auto sampleRate = playbackEngine_.sampleRateHz();
    if (sampleRate == 0) {
        return;
    }
    const auto sampleIndex = static_cast<std::size_t>(std::max(0.0, positionSeconds) * sampleRate);
    playbackEngine_.seek(sampleIndex);
    // Immediate feedback rather than waiting for the next timer tick - a
    // drag that ends while paused (playbackUpdateTimer_ not running)
    // should still show the new position right away.
    updatePlaybackPosition();
}

void MainWindow::updatePlaybackPosition() {
    const auto sampleRate = playbackEngine_.sampleRateHz();
    const double totalSeconds =
        sampleRate > 0 ? static_cast<double>(playbackEngine_.totalSamples()) / sampleRate : 0.0;
    const double positionSeconds =
        sampleRate > 0 ? static_cast<double>(playbackEngine_.positionSamples()) / sampleRate : 0.0;

    playbackPanel_->setPositionSeconds(positionSeconds);
    canvas_->setPlayheadFraction(totalSeconds > 0.0 ? std::optional<double>(positionSeconds / totalSeconds)
                                                     : std::nullopt);

    if (!playbackEngine_.isPlaying()) {
        // Playback reached the end on its own (PlaybackEngine::isPlaying()
        // clears itself there - see its own docs) - stop polling rather
        // than continuing to tick against a position that's no longer
        // advancing.
        playbackUpdateTimer_->stop();
    }
}

void MainWindow::setPlaybackOutputDevice(const QString& deviceName) {
    // Unlike Loop/Record's device preferences (only applied on their next
    // start()), PlaybackEngine's device is open for its whole lifetime, so
    // this switches immediately - see PlaybackEngine::setPreferredOutputDevice()'s
    // own docs.
    if (!playbackEngine_.setPreferredOutputDevice(deviceName.toStdString())) {
        statusBar()->showMessage(tr("Could not switch to the selected output device."), 5000);
    }
}

void MainWindow::setPlaybackVolume(int percent) {
    playbackEngine_.setVolume(static_cast<float>(percent) / 100.0f);
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
        loopPanel_->setRunning(false);
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
        // A silent, correctly-dimensioned placeholder - see LoopEngine::
        // emptyImage()'s own docs - so a brand-new layer has something to
        // render immediately, rather than nothing at all until the first
        // real loop finishes. A *reused* layer (the branch above) already
        // has real content from a previous session, so it deliberately
        // keeps that instead of being overwritten with a blank image here.
        layer.setContent(loopEngine_->emptyImage());
        loopLayerId_ = project_->addLayer(std::move(layer));
    }
    hasUnsavedChanges_ = true;
    refreshLayersPanel();
    canvas_->update();

    loopEngine_->start();
    loopPanel_->setRunning(true);
    if (!loopEngine_->isDeviceAvailable()) {
        statusBar()->showMessage(
            tr("Loop capture started, but no input device is available - nothing will be captured."), 5000);
    } else {
        statusBar()->showMessage(tr("Looping..."));
    }
    loopUpdateTimer_->start();
}

void MainWindow::setKeepLooping(bool keepLooping) {
    loopPanel_->setKeepLoopingChecked(keepLooping);
    if (!loopEngine_) {
        return;
    }
    loopEngine_->setKeepLooping(keepLooping);
}

void MainWindow::setLoopInputDevice(const QString& deviceName) {
    if (!loopEngine_) {
        return;
    }
    loopEngine_->setPreferredInputDevice(deviceName.toStdString());
}

void MainWindow::setLoopOutputDevice(const QString& deviceName) {
    if (!loopEngine_) {
        return;
    }
    loopEngine_->setPreferredOutputDevice(deviceName.toStdString());
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

QString MainWindow::loopInputDevice() const {
    return loopEngine_ ? QString::fromStdString(loopEngine_->preferredInputDevice()) : QString();
}

QString MainWindow::loopOutputDevice() const {
    return loopEngine_ ? QString::fromStdString(loopEngine_->preferredOutputDevice()) : QString();
}

QString MainWindow::recordInputDevice() const {
    return QString::fromStdString(recordEngine_.preferredInputDevice());
}

float MainWindow::playbackVolume() const noexcept {
    return playbackEngine_.volume();
}

void MainWindow::toggleRecording() {
    if (recordEngine_.isRecording()) {
        recordDrainTimer_->stop();
        recordEngine_.stop();
        recordPanel_->setRecording(false);

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
    recordPanel_->setRecording(true);
    if (!recordEngine_.isDeviceAvailable()) {
        statusBar()->showMessage(tr("Recording started, but no input device is available - nothing will be captured."),
                                  5000);
    } else {
        statusBar()->showMessage(tr("Recording..."));
    }
    recordDrainTimer_->start();
}

void MainWindow::setRecordInputDevice(const QString& deviceName) {
    recordEngine_.setPreferredInputDevice(deviceName.toStdString());
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
