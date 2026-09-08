#include "sound_mind/studio/main_window.h"

#include <cctype>
#include <cstddef>
#include <cstring>
#include <exception>
#include <stdexcept>

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
#include "sound_mind/core/layer_export.h"
#include "sound_mind/core/pooling.h"
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

    try {
        return sound_mind::core::exportLayerAudio(*layer, path, *format);
    } catch (const std::exception& e) {
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

    try {
        return sound_mind::core::exportLayerVideo(*layer, path);
    } catch (const std::exception& e) {
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
        if (it->content().has_value()) {
            return &*it;
        }
    }
    return nullptr;
}

void MainWindow::startPlayback() {
    if (!project_) {
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

bool MainWindow::isPlaying() const noexcept {
    return playbackEngine_.isPlaying();
}

void MainWindow::poolTopmostLayer() {
    QString errorMessage;
    QString streamPath;
    QString poolPath;
    if (!poolTopmostLayerNow(&errorMessage, &streamPath, &poolPath)) {
        QMessageBox::critical(this, tr("Pool Layer Failed"), errorMessage);
        return;
    }

    QMessageBox::information(this, tr("Pool Layer"),
                              tr("Pooled successfully.\n\nStream render: %1\nPool render: %2")
                                  .arg(streamPath, poolPath));
}

bool MainWindow::poolTopmostLayerNow(QString* errorMessage, QString* streamPngPath, QString* poolPngPath) {
    sound_mind::core::Layer* layer = topmostLayerWithContent();
    if (layer == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No layer with content to pool.");
        }
        return false;
    }

    try {
        if (!sound_mind::core::poolLayer(*layer)) {
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
        return true;
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
}

}  // namespace sound_mind::studio
