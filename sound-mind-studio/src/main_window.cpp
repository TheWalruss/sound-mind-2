#include "sound_mind/studio/main_window.h"

#include <cstddef>
#include <cstring>
#include <exception>

#include <QAction>
#include <QFileDialog>
#include <QImage>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QToolBar>

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/wav_file.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/canvas_widget.h"

#ifndef SOUND_MIND_VERSION
#define SOUND_MIND_VERSION "unknown"
#endif

namespace sound_mind::studio {

namespace {
const char* kProjectFileFilter = "Sound Mind Projects (*.smproj)";
const char* kAudioFileFilter = "WAV Audio (*.wav)";
const char* kImageFileFilter = "Images (*.png *.jpg *.jpeg *.bmp *.tga *.webp)";

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

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Sound Mind Studio v" SOUND_MIND_VERSION));
    resize(800, 600);

    canvas_ = new CanvasWidget(this);
    setCentralWidget(canvas_);

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

    QToolBar* transportToolBar = addToolBar(tr("Transport"));
    // Plain text actions rather than icons - no icon assets exist yet, and
    // these are unambiguous enough on their own for a first pass.
    QAction* playAction = transportToolBar->addAction(tr("Play"));
    connect(playAction, &QAction::triggered, this, &MainWindow::startPlayback);

    QAction* pauseAction = transportToolBar->addAction(tr("Pause"));
    connect(pauseAction, &QAction::triggered, this, &MainWindow::pausePlayback);

    QAction* stopAction = transportToolBar->addAction(tr("Stop"));
    connect(stopAction, &QAction::triggered, this, &MainWindow::stopPlayback);

    newProject();
}

const sound_mind::core::Project* MainWindow::project() const noexcept {
    return project_ ? &*project_ : nullptr;
}

void MainWindow::setProject(sound_mind::core::Project project) {
    project_ = std::move(project);
    canvas_->setProject(&*project_);
    playbackEngine_.stop();
    playbackLoaded_ = false;
}

void MainWindow::newProject() {
    currentPath_.reset();
    setProject(sound_mind::core::Project::createNew(sound_mind::core::ProjectSettings{}));
}

void MainWindow::openProject() {
    const QString fileName =
        QFileDialog::getOpenFileName(this, tr("Open Project"), QString(), tr(kProjectFileFilter));
    if (fileName.isEmpty()) {
        return;
    }

    const std::filesystem::path path(fileName.toStdString());
    try {
        setProject(sound_mind::core::Project::load(path));
        currentPath_ = path;
    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Open Project Failed"), QString::fromStdString(e.what()));
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

    try {
        const auto audio = sound_mind::codec::readWavFile(path);
        const auto content = sound_mind::codec::encode(audio, sound_mind::codec::StreamCodecConfig{});

        sound_mind::core::Layer layer(0, path.filename().string(), sound_mind::core::LayerType::Normal);
        layer.setContent(content);
        project_->addLayer(std::move(layer));
        canvas_->update();
        // The topmost layer just changed - the next startPlayback() should
        // pick up the newly imported one instead of whatever was loaded
        // before, rather than silently keep playing stale content.
        playbackLoaded_ = false;
        return true;
    } catch (const std::exception& e) {
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

    try {
        const auto rgbImage = toRgbImage(sourceImage);
        const auto content = sound_mind::codec::fromRgbImage(rgbImage, sound_mind::codec::StreamCodecConfig{});

        sound_mind::core::Layer layer(0, path.filename().string(), sound_mind::core::LayerType::Normal);
        layer.setContent(content);
        project_->addLayer(std::move(layer));
        canvas_->update();
        playbackLoaded_ = false;
        return true;
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
}

void MainWindow::startPlayback() {
    if (!project_) {
        return;
    }

    if (!playbackLoaded_) {
        const auto& layers = project_->layers();
        bool foundContent = false;
        for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
            if (it->content().has_value()) {
                playbackEngine_.loadAudio(sound_mind::codec::decode(*it->content()));
                foundContent = true;
                break;
            }
        }
        if (!foundContent) {
            return;
        }
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

bool MainWindow::isPlaying() const noexcept {
    return playbackEngine_.isPlaying();
}

}  // namespace sound_mind::studio
