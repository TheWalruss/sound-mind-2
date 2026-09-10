#include "test_main_window.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QStatusBar>
#include <QToolBar>
#include <QtTest/QtTest>

#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/image_scale_picker_dialog.h"
#include "sound_mind/studio/landing_page.h"
#include "sound_mind/studio/layers_panel.h"
#include "sound_mind/studio/loop_panel.h"
#include "sound_mind/studio/main_window.h"
#include "sound_mind/studio/playback_panel.h"
#include "sound_mind/studio/record_panel.h"

using sound_mind::studio::ImageScalePickerDialog;
using sound_mind::studio::LandingPage;
using sound_mind::studio::LayersPanel;
using sound_mind::studio::LoopPanel;
using sound_mind::studio::MainWindow;
using sound_mind::studio::PlaybackPanel;
using sound_mind::studio::RecordPanel;

namespace {

void appendUint32(std::vector<char>& bytes, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        bytes.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }
}

void appendUint16(std::vector<char>& bytes, std::uint16_t value) {
    for (int i = 0; i < 2; ++i) {
        bytes.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }
}

/// @brief Writes a tiny, valid 16-bit PCM mono WAV file to `path` - just
/// enough to exercise MainWindow::importAudioFile() without needing a real
/// test asset file on disk.
void writeTestWavFile(const std::filesystem::path& path) {
    const std::vector<std::int16_t> samples = {1000, -1000, 2000, -2000};
    const std::uint32_t dataSize = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));

    std::vector<char> bytes;
    bytes.insert(bytes.end(), {'R', 'I', 'F', 'F'});
    appendUint32(bytes, 36 + dataSize);
    bytes.insert(bytes.end(), {'W', 'A', 'V', 'E'});
    bytes.insert(bytes.end(), {'f', 'm', 't', ' '});
    appendUint32(bytes, 16);
    appendUint16(bytes, 1);  // PCM
    appendUint16(bytes, 1);  // mono
    appendUint32(bytes, 44100);
    appendUint32(bytes, 44100 * 2);
    appendUint16(bytes, 2);
    appendUint16(bytes, 16);
    bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
    appendUint32(bytes, dataSize);
    for (const std::int16_t sample : samples) {
        appendUint16(bytes, static_cast<std::uint16_t>(sample));
    }

    std::ofstream stream(path, std::ios::binary);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

/// @brief Writes a valid 16-bit PCM mono WAV file with exactly
/// `frameCount` samples at `sampleRateHz`, to `path` - for exercising the
/// Audio Import Snippets milestone's splitting logic, which needs audio
/// longer than writeTestWavFile()'s own fixed 4 samples.
void writeTestWavFileWithFrameCount(const std::filesystem::path& path, std::size_t frameCount,
                                     std::uint32_t sampleRateHz = 44100) {
    const std::uint32_t dataSize = static_cast<std::uint32_t>(frameCount * sizeof(std::int16_t));

    std::vector<char> bytes;
    bytes.insert(bytes.end(), {'R', 'I', 'F', 'F'});
    appendUint32(bytes, 36 + dataSize);
    bytes.insert(bytes.end(), {'W', 'A', 'V', 'E'});
    bytes.insert(bytes.end(), {'f', 'm', 't', ' '});
    appendUint32(bytes, 16);
    appendUint16(bytes, 1);  // PCM
    appendUint16(bytes, 1);  // mono
    appendUint32(bytes, sampleRateHz);
    appendUint32(bytes, sampleRateHz * 2);
    appendUint16(bytes, 2);
    appendUint16(bytes, 16);
    bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
    appendUint32(bytes, dataSize);
    for (std::size_t i = 0; i < frameCount; ++i) {
        // A cheap, deterministic ramp - the exact waveform doesn't matter,
        // only that a real, valid PCM stream of the requested length exists.
        const auto sample = static_cast<std::int16_t>((static_cast<int>(i % 2000) - 1000));
        appendUint16(bytes, static_cast<std::uint16_t>(sample));
    }

    std::ofstream stream(path, std::ios::binary);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

/// @brief A project settings struct with a small `canvasWidth`, for fast
/// snippet-splitting tests - the default `ProjectSettings{}`'s own loop
/// length (canvasWidth * hopLength, per LoopEngine's docs) is over 10
/// seconds of audio at the default sample rate, which would make a
/// multi-snippet test file impractically large.
sound_mind::core::ProjectSettings smallCanvasProjectSettings() {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 8;  // loop length = 8 * 441 = 3528 samples at the default 44100 Hz/10 ms timestep.
    return settings;
}

/// @brief A project settings struct with `canvasWidth`/`canvasHeight`/
/// `binCount` values distinct from both each other and from the 30x20 test
/// image the Image Import Scaling tests use - so each of the five scale
/// modes produces a uniquely identifiable (frameCount, binCount) result,
/// unambiguously confirming which mode actually ran.
sound_mind::core::ProjectSettings imageScalingTestProjectSettings() {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 100;
    settings.canvasHeight = 50;
    settings.binCount = 50;  // kept numerically equal to canvasHeight, per ProjectSettings' own docs.
    return settings;
}

/// @brief Gives `window` a fresh, saved-to-disk project with default
/// settings - the testable-core equivalent of the old, no-dialog
/// newProject() for every test that just needs *some* project to work
/// against. newProject() itself now shows a real CreateProjectWizard (see
/// its own docs), which would block forever under the offscreen test
/// platform if called directly here - createProjectAt() is what it calls
/// internally once the wizard is accepted, and carries none of that risk.
/// A fresh, numbered path each call, so distinct MainWindow instances
/// across the whole suite never contend over the same file.
void createFreshTestProject(MainWindow& window) {
    static int counter = 0;
    const auto path = std::filesystem::temp_directory_path() /
                       ("sound-mind-test-fresh-project-" + std::to_string(counter++) + ".smproj");
    QVERIFY(window.createProjectAt(sound_mind::core::ProjectSettings{}, path));
}

}  // namespace

void MainWindowTest::hasARealWindowIconNotTheDefaultOne() {
    // Per the Visual Identity milestone (v0.Y.14.1) - see theme.h.
    const MainWindow window;
    QVERIFY(!window.windowIcon().isNull());
}

void MainWindowTest::startsWithNoProjectOpen() {
    // Per the Landing Page milestone (v0.Y.9.1): the Studio no longer
    // silently creates an in-memory project at startup - the Landing Page
    // is shown until New/Open Project actually creates or loads one.
    const MainWindow window;
    QVERIFY(window.project() == nullptr);
}

void MainWindowTest::newProjectShowsTheCanvasInsteadOfTheLandingPage() {
    // Via createProjectAt() - the testable core newProject() itself calls
    // once its wizard is accepted (see its own docs) - not newProject()
    // directly, which would now block on a real dialog under this
    // headless test platform.
    MainWindow window;
    QVERIFY(window.isShowingLandingPage());

    createFreshTestProject(window);

    QVERIFY(!window.isShowingLandingPage());
}

void MainWindowTest::openProjectAtOpensAndRecordsARecentProject() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-recent-project.smproj";

    // saveProject()/saveProjectAs() prompt interactively (QFileDialog),
    // which would hang under the offscreen test platform - write the
    // project file directly via Project::save() instead, exactly as a
    // real "open a project someone else saved" scenario would find on
    // disk (a window's own currentPath_ isn't involved either way).
    {
        MainWindow writer;
        createFreshTestProject(writer);
        const_cast<sound_mind::core::Project*>(writer.project())->save(projectPath);
    }

    MainWindow window;
    const bool ok = window.openProjectAt(projectPath);

    QVERIFY(ok);
    QVERIFY(window.project() != nullptr);
    QVERIFY(!window.isShowingLandingPage());

    auto* landing = window.findChild<LandingPage*>();
    QVERIFY(landing != nullptr);
    const auto entries = landing->findChildren<QPushButton*>(QStringLiteral("recentProjectButton"));
    bool foundIt = false;
    for (auto* entry : entries) {
        if (entry->toolTip() == QString::fromStdString(projectPath.string())) {
            foundIt = true;
            break;
        }
    }
    QVERIFY(foundIt);

    std::filesystem::remove(projectPath);
}

void MainWindowTest::openProjectAtFailsGracefullyForAMissingFile() {
    MainWindow window;
    const bool ok = window.openProjectAt(std::filesystem::temp_directory_path() / "sound-mind-does-not-exist.smproj");

    QVERIFY(!ok);
    QVERIFY(window.project() == nullptr);
    QVERIFY(window.isShowingLandingPage());
}

// No landingPageNewProjectRequestedCreatesAProject test: as of v0.Y.11.1,
// LandingPage's "New Project" button is wired to the real, interactive
// newProject(), which now shows a real CreateProjectWizard dialog -
// clicking it here would block forever under the offscreen test
// platform. Matches this codebase's established precedent of never
// exercising a modal-dialog-showing path directly in an automated test
// (see importAudioFile()'s docs) - the wiring itself (one `connect()`
// call in the constructor) is straightforward enough to trust by
// inspection, the same as every other signal/slot connection here.

void MainWindowTest::landingPageRecentProjectRequestedOpensThatPath() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-landing-recent.smproj";
    {
        MainWindow writer;
        createFreshTestProject(writer);
        const_cast<sound_mind::core::Project*>(writer.project())->save(projectPath);
    }

    MainWindow window;
    // Populate the list the same way a real recent-project entry would get
    // there - via a prior successful open, not by reaching into internals.
    QVERIFY(window.openProjectAt(projectPath));
    createFreshTestProject(window);  // back to a fresh, different project.
    QCOMPARE(window.project()->layers().size(), static_cast<std::size_t>(1));

    auto* landing = window.findChild<LandingPage*>();
    QVERIFY(landing != nullptr);
    QPushButton* recentButton = nullptr;
    for (auto* entry : landing->findChildren<QPushButton*>(QStringLiteral("recentProjectButton"))) {
        if (entry->toolTip() == QString::fromStdString(projectPath.string())) {
            recentButton = entry;
            break;
        }
    }
    QVERIFY(recentButton != nullptr);

    recentButton->click();

    QVERIFY(window.project() != nullptr);
    QVERIFY(!window.isShowingLandingPage());

    std::filesystem::remove(projectPath);
}

void MainWindowTest::newProjectReplacesTheCurrentOne() {
    // MainWindow::project_ is a std::optional<Project>, which reuses its own
    // inline storage across assignment - so project()'s pointer *address*
    // staying the same across creation calls is expected, not a bug. What
    // actually matters is that the *contents* are a fresh project
    // afterwards, which is what this checks. Via createProjectAt() (see
    // createFreshTestProject()'s docs for why, not newProject() directly).
    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.project() != nullptr);

    createFreshTestProject(window);  // replace it with another fresh one.

    QVERIFY(window.project() != nullptr);
    QCOMPARE(window.project()->layers().size(), static_cast<std::size_t>(1));
}

void MainWindowTest::importAudioFileAddsANewLayer() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    const bool ok = window.importAudioFile(path);
    std::filesystem::remove(path);

    QVERIFY(ok);
    QCOMPARE(window.project()->layers().size(), static_cast<std::size_t>(2));
    QVERIFY(window.project()->layers().back().content().has_value());
}

void MainWindowTest::importImageFileAddsANewLayer() {
    QImage image(4, 3, QImage::Format_RGB32);
    image.fill(Qt::red);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import.png";
    QVERIFY(image.save(QString::fromStdString(path.string())));

    MainWindow window;
    createFreshTestProject(window);
    // KeepNativeResolution - matches this test's own pre-existing intent
    // (does importing add a layer at all) rather than exercising scaling.
    const bool ok = window.importImageFile(path, ImageScalePickerDialog::Mode::KeepNativeResolution);
    std::filesystem::remove(path);

    QVERIFY(ok);
    QCOMPARE(window.project()->layers().size(), static_cast<std::size_t>(2));
    QVERIFY(window.project()->layers().back().content().has_value());
    QCOMPARE(window.project()->layers().back().content()->frameCount, static_cast<std::uint32_t>(4));
    QCOMPARE(window.project()->layers().back().content()->config.binCount, static_cast<std::uint32_t>(3));
}

void MainWindowTest::importAudioFileFailsGracefullyForAMissingFile() {
    MainWindow window;
    createFreshTestProject(window);
    const bool ok = window.importAudioFile(std::filesystem::temp_directory_path() / "sound-mind-does-not-exist.wav");

    QVERIFY(!ok);
    QCOMPARE(window.project()->layers().size(), static_cast<std::size_t>(1));
}

void MainWindowTest::startPlaybackDoesNothingWithNoContent() {
    // A fresh project's only layer (Background) has no content yet.
    MainWindow window;
    createFreshTestProject(window);
    window.startPlayback();
    QVERIFY(!window.isPlaying());
}

void MainWindowTest::startPlaybackPlaysAnImportedLayer() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-playback.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    window.startPlayback();
    QVERIFY(window.isPlaying());
}

void MainWindowTest::pauseAndResumePlayback() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-playback-pause.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    window.startPlayback();
    QVERIFY(window.isPlaying());

    window.pausePlayback();
    QVERIFY(!window.isPlaying());

    window.startPlayback();
    QVERIFY(window.isPlaying());
}

void MainWindowTest::stopPlaybackStopsIt() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-playback-stop.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    window.startPlayback();
    QVERIFY(window.isPlaying());

    window.stopPlayback();
    QVERIFY(!window.isPlaying());
}

void MainWindowTest::poolTopmostLayerNowFailsGracefullyWithNoContent() {
    // A fresh project's only layer (Background) has no content yet.
    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(!window.poolTopmostLayerNow());
}

void MainWindowTest::poolTopmostLayerNowPoolsAnImportedLayer() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-pool.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    QString streamPngPath;
    QString poolPngPath;
    const bool ok = window.poolTopmostLayerNow(nullptr, &streamPngPath, &poolPngPath);

    QVERIFY(ok);
    QVERIFY(!streamPngPath.isEmpty());
    QVERIFY(!poolPngPath.isEmpty());
    QVERIFY(QFile::exists(streamPngPath));
    QVERIFY(QFile::exists(poolPngPath));
    QCOMPARE(window.project()->layers().back().poolContent().has_value(), true);

    QFile::remove(streamPngPath);
    QFile::remove(poolPngPath);
}

void MainWindowTest::exportTopmostLayerAudioNowFailsGracefullyWithNoContent() {
    // A fresh project's only layer (Background) has no content yet.
    MainWindow window;
    createFreshTestProject(window);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-export.flac";
    QVERIFY(!window.exportTopmostLayerAudioNow(path));
    QVERIFY(!QFile::exists(QString::fromStdString(path.string())));
}

void MainWindowTest::exportTopmostLayerAudioNowExportsAnImportedLayer() {
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-audio.wav";
    writeTestWavFile(wavPath);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);

    const auto exportPath = std::filesystem::temp_directory_path() / "sound-mind-test-export.flac";
    const bool ok = window.exportTopmostLayerAudioNow(exportPath);

    QVERIFY(ok);
    QVERIFY(QFile::exists(QString::fromStdString(exportPath.string())));
    std::filesystem::remove(exportPath);
}

void MainWindowTest::exportTopmostLayerAudioNowFailsForAnUnrecognizedExtension() {
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-audio2.wav";
    writeTestWavFile(wavPath);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);

    QString errorMessage;
    const auto exportPath = std::filesystem::temp_directory_path() / "sound-mind-test-export.xyz";
    QVERIFY(!window.exportTopmostLayerAudioNow(exportPath, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void MainWindowTest::exportTopmostLayerVideoNowFailsGracefullyWithNoContent() {
    // A fresh project's only layer (Background) has no content yet.
    MainWindow window;
    createFreshTestProject(window);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-export.mp4";
    QVERIFY(!window.exportTopmostLayerVideoNow(path));
    QVERIFY(!QFile::exists(QString::fromStdString(path.string())));
}

void MainWindowTest::exportTopmostLayerVideoNowExportsAnImportedLayer() {
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-video.wav";
    writeTestWavFile(wavPath);

    // A small canvasWidth, not createFreshTestProject()'s default 1024 - as
    // of v0.Y.21.1 (Layer Time Alignment), renderLayer() (which
    // exportLayerVideo() uses) always renders at the project's own
    // canvasWidth rather than the layer's native content width, so a
    // default-sized canvas here would make this test encode a real,
    // needlessly large video just to confirm exporting works at all.
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-video.smproj";
    MainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);

    const auto exportPath = std::filesystem::temp_directory_path() / "sound-mind-test-export.mp4";
    const bool ok = window.exportTopmostLayerVideoNow(exportPath);

    QVERIFY(ok);
    QVERIFY(QFile::exists(QString::fromStdString(exportPath.string())));
    std::filesystem::remove(exportPath);
}

void MainWindowTest::importAudioFileShowsProgressThenCompletionInTheStatusBar() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-status-import.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QSignalSpy spy(window.statusBar(), &QStatusBar::messageChanged);

    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    // At least one "in progress" message, followed by a completion message
    // still showing once the (synchronous) call has returned - not an empty
    // string, which is what a bare clearMessage() would leave behind.
    QVERIFY(spy.count() >= 2);
    QVERIFY(!window.statusBar()->currentMessage().isEmpty());
}

void MainWindowTest::poolTopmostLayerNowShowsProgressThenCompletionInTheStatusBar() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-status-pool.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    QSignalSpy spy(window.statusBar(), &QStatusBar::messageChanged);
    QString streamPngPath;
    QString poolPngPath;
    QVERIFY(window.poolTopmostLayerNow(nullptr, &streamPngPath, &poolPngPath));

    QVERIFY(spy.count() >= 2);
    QVERIFY(!window.statusBar()->currentMessage().isEmpty());

    QFile::remove(streamPngPath);
    QFile::remove(poolPngPath);
}

void MainWindowTest::exportTopmostLayerAudioNowShowsProgressThenCompletionInTheStatusBar() {
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-status-export.wav";
    writeTestWavFile(wavPath);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);

    const auto exportPath = std::filesystem::temp_directory_path() / "sound-mind-test-status-export.flac";
    QSignalSpy spy(window.statusBar(), &QStatusBar::messageChanged);
    QVERIFY(window.exportTopmostLayerAudioNow(exportPath));
    std::filesystem::remove(exportPath);

    QVERIFY(spy.count() >= 2);
    QVERIFY(!window.statusBar()->currentMessage().isEmpty());
}

void MainWindowTest::aFailedOperationClearsTheStatusBarRatherThanLeavingAStaleMessage() {
    // No layer with content to export - fails, and shows a modal in the
    // interactive path (exportAudio()), but exportTopmostLayerAudioNow()
    // itself never shows dialogs (see its docs) - it should still leave the
    // status bar clean rather than stuck on an "Exporting..." message that
    // never actually completed.
    MainWindow window;
    createFreshTestProject(window);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-status-fail.flac";

    QVERIFY(!window.exportTopmostLayerAudioNow(path));
    QVERIFY(window.statusBar()->currentMessage().isEmpty());
}

void MainWindowTest::toggleLoopModeAddsALayerAndStartsTheEngine() {
    MainWindow window;
    createFreshTestProject(window);
    const std::size_t layerCountBefore = window.project()->layers().size();

    window.toggleLoopMode();

    QVERIFY(window.isLoopModeRunning());
    QCOMPARE(window.project()->layers().size(), layerCountBefore + 1);
    QCOMPARE(QString::fromStdString(window.project()->layers().back().name()), QStringLiteral("Loop Input"));

    window.toggleLoopMode();  // cleanup - stop before the window is destroyed.
}

void MainWindowTest::toggleLoopModeStopsARunningCapture() {
    MainWindow window;
    createFreshTestProject(window);
    window.toggleLoopMode();
    QVERIFY(window.isLoopModeRunning());

    window.toggleLoopMode();

    QVERIFY(!window.isLoopModeRunning());
}

void MainWindowTest::startPlaybackDoesNothingWhileLoopModeIsRunning() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-loop-playback-guard.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    window.toggleLoopMode();
    QVERIFY(window.isLoopModeRunning());

    window.startPlayback();
    QVERIFY(!window.isPlaying());

    window.toggleLoopMode();  // cleanup.
}

void MainWindowTest::toggleRecordingStartsAndStopsWithoutAddingALayerWhenNothingWasCaptured() {
    MainWindow window;
    createFreshTestProject(window);
    const std::size_t layerCountBefore = window.project()->layers().size();

    window.toggleRecording();
    QVERIFY(window.isRecording());

    // No real audio device delivers samples in this test environment, so
    // stopping immediately should find nothing captured - no new layer.
    window.toggleRecording();
    QVERIFY(!window.isRecording());
    QCOMPARE(window.project()->layers().size(), layerCountBefore);
}

void MainWindowTest::startPlaybackDoesNothingWhileRecordingIsRunning() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-record-playback-guard.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    window.toggleRecording();
    QVERIFY(window.isRecording());

    window.startPlayback();
    QVERIFY(!window.isPlaying());

    window.toggleRecording();  // cleanup.
}

void MainWindowTest::toggleLoopModeDoesNothingWhileRecordingIsRunning() {
    MainWindow window;
    createFreshTestProject(window);
    window.toggleRecording();
    QVERIFY(window.isRecording());

    window.toggleLoopMode();

    QVERIFY(!window.isLoopModeRunning());
    window.toggleRecording();  // cleanup.
}

void MainWindowTest::toggleRecordingDoesNothingWhileLoopModeIsRunning() {
    MainWindow window;
    createFreshTestProject(window);
    window.toggleLoopMode();
    QVERIFY(window.isLoopModeRunning());

    window.toggleRecording();

    QVERIFY(!window.isRecording());
    window.toggleLoopMode();  // cleanup.
}

void MainWindowTest::newProjectStartsWithNoUnsavedChanges() {
    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(!window.hasUnsavedChanges());
}

void MainWindowTest::importAudioFileMarksUnsavedChanges() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-lifecycle-import.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(!window.hasUnsavedChanges());

    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::poolTopmostLayerNowMarksUnsavedChanges() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-lifecycle-pool.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    QString streamPngPath;
    QString poolPngPath;
    QVERIFY(window.poolTopmostLayerNow(nullptr, &streamPngPath, &poolPngPath));
    QFile::remove(streamPngPath);
    QFile::remove(poolPngPath);

    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::toggleLoopModeMarksUnsavedChangesWhenItStarts() {
    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(!window.hasUnsavedChanges());

    window.toggleLoopMode();
    QVERIFY(window.hasUnsavedChanges());

    window.toggleLoopMode();  // cleanup.
}

void MainWindowTest::savingProjectClearsUnsavedChanges() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-lifecycle-save.smproj";
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-lifecycle-save.wav";
    writeTestWavFile(wavPath);

    MainWindow window;
    createFreshTestProject(window);
    // Project::save() directly, not saveProjectAs() - see
    // openProjectAtOpensAndRecordsARecentProject()'s comment for why:
    // saveProjectAs() prompts interactively, which would hang here.
    const_cast<sound_mind::core::Project*>(window.project())->save(projectPath);
    QVERIFY(window.openProjectAt(projectPath));  // establishes currentPath_ without a dialog.

    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);
    QVERIFY(window.hasUnsavedChanges());

    window.saveProject();  // currentPath_ is already set - no dialog.

    QVERIFY(!window.hasUnsavedChanges());
    std::filesystem::remove(projectPath);
}

void MainWindowTest::openProjectAtClearsUnsavedChangesFromThePreviousProject() {
    const auto projectPath =
        std::filesystem::temp_directory_path() / "sound-mind-test-lifecycle-switch.smproj";
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-lifecycle-switch.wav";
    writeTestWavFile(wavPath);
    {
        MainWindow writer;
        createFreshTestProject(writer);
        const_cast<sound_mind::core::Project*>(writer.project())->save(projectPath);
    }

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);
    QVERIFY(window.hasUnsavedChanges());

    // openProjectAt() is deliberately the non-prompting core (see its own
    // docs) - calling it directly here, bypassing openProject()'s
    // unsaved-changes guard, is the same intentional bypass every other
    // *At()/*Now() test in this file relies on.
    QVERIFY(window.openProjectAt(projectPath));

    QVERIFY(!window.hasUnsavedChanges());
    std::filesystem::remove(projectPath);
}

void MainWindowTest::closeAcceptsWhenThereAreNoUnsavedChanges() {
    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(!window.hasUnsavedChanges());

    QVERIFY(window.close());
}

void MainWindowTest::closeRefusesWhileLoopModeIsRunning() {
    MainWindow window;
    createFreshTestProject(window);
    window.toggleLoopMode();
    QVERIFY(window.isLoopModeRunning());

    // Refused outright (no dialog reached - see closeEvent()'s docs), so
    // this is safe to call even though the project is also now dirty
    // (starting Loop Mode just added a layer).
    QVERIFY(!window.close());

    window.toggleLoopMode();  // cleanup.
}

void MainWindowTest::newProjectRefusesWhileLoopModeIsRunning() {
    MainWindow window;
    createFreshTestProject(window);
    window.toggleLoopMode();
    QVERIFY(window.isLoopModeRunning());
    const std::size_t layerCountBefore = window.project()->layers().size();

    // The real, interactive newProject() - safe to call directly, since
    // the Loop Mode/Recording check runs *before* the wizard would ever
    // be shown (see its own docs) - refused outright, no dialog reached.
    window.newProject();

    QVERIFY(window.isLoopModeRunning());
    QCOMPARE(window.project()->layers().size(), layerCountBefore);

    window.toggleLoopMode();  // cleanup.
}

void MainWindowTest::openProjectRefusesWhileRecordingIsRunning() {
    MainWindow window;
    createFreshTestProject(window);
    window.toggleRecording();
    QVERIFY(window.isRecording());

    // Refused before ever reaching the file dialog - see openProject()'s
    // docs - so this is safe to call directly in a headless test.
    window.openProject();

    QVERIFY(window.isRecording());

    window.toggleRecording();  // cleanup.
}

void MainWindowTest::openProjectAtRefusesWhileLoopModeIsRunning() {
    MainWindow window;
    createFreshTestProject(window);
    window.toggleLoopMode();
    QVERIFY(window.isLoopModeRunning());

    QString errorMessage;
    const bool ok = window.openProjectAt(std::filesystem::temp_directory_path() / "sound-mind-does-not-exist.smproj",
                                          &errorMessage);

    QVERIFY(!ok);
    QVERIFY(!errorMessage.isEmpty());
    QVERIFY(window.isLoopModeRunning());

    window.toggleLoopMode();  // cleanup.
}

void MainWindowTest::createProjectAtSavesImmediatelyAndBecomesCurrent() {
    // Per the Create Project Wizard milestone (v0.Y.11.1): "the wizard's
    // completion is the first save" - a real file should exist the
    // moment createProjectAt() returns, not only after some later,
    // separate Save.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-create-project.smproj";

    MainWindow window;
    const bool ok = window.createProjectAt(sound_mind::core::ProjectSettings{}, path);

    QVERIFY(ok);
    QVERIFY(window.project() != nullptr);
    QVERIFY(!window.isShowingLandingPage());
    QVERIFY(!window.hasUnsavedChanges());
    QVERIFY(QFile::exists(QString::fromStdString(path.string())));

    std::filesystem::remove(path);
}

void MainWindowTest::createProjectAtAppliesGivenSettings() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-create-project-settings.smproj";

    sound_mind::core::ProjectSettings settings;
    settings.sampleRateHz = 48000;
    settings.binCount = 256;

    MainWindow window;
    QVERIFY(window.createProjectAt(settings, path));

    QCOMPARE(window.project()->settings().sampleRateHz, settings.sampleRateHz);
    QCOMPARE(window.project()->settings().binCount, settings.binCount);

    std::filesystem::remove(path);
}

void MainWindowTest::createProjectAtFailsGracefullyForAnUnwritableLocation() {
    // A directory that doesn't exist - Project::save() can't create it.
    const auto path =
        std::filesystem::temp_directory_path() / "sound-mind-test-nonexistent-dir" / "project.smproj";

    MainWindow window;
    QString errorMessage;
    const bool ok = window.createProjectAt(sound_mind::core::ProjectSettings{}, path, &errorMessage);

    QVERIFY(!ok);
    QVERIFY(!errorMessage.isEmpty());
    // Still becomes current, per createProjectAt()'s "report, don't
    // silently revert" docs - just not actually saved yet.
    QVERIFY(window.project() != nullptr);
    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::importAudioFileUsesTheProjectsConfiguredCodecSettings() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-uses-settings.wav";
    writeTestWavFile(path);
    const auto projectPath =
        std::filesystem::temp_directory_path() / "sound-mind-test-import-uses-settings.smproj";

    sound_mind::core::ProjectSettings settings;
    settings.binCount = 128;  // Deliberately not StreamCodecConfig{}'s own default (512).

    MainWindow window;
    QVERIFY(window.createProjectAt(settings, projectPath));

    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QCOMPARE(window.project()->layers().back().content()->config.binCount, static_cast<std::uint32_t>(128));
}

void MainWindowTest::layersPanelIsHiddenUntilAProjectExists() {
    // isHidden(), not isVisible() - see LayersPanel's own tests for why
    // (the dialog/window chain is never actually shown in this headless
    // test, but hide()/show() still set each widget's own explicit flag).
    MainWindow window;
    auto* panel = window.findChild<LayersPanel*>();
    QVERIFY(panel != nullptr);
    QVERIFY(panel->isHidden());

    createFreshTestProject(window);

    QVERIFY(!panel->isHidden());
}

void MainWindowTest::refreshLayersPanelReflectsTheCurrentLayers() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-layers-refresh.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    auto* panel = window.findChild<LayersPanel*>();
    QVERIFY(panel != nullptr);
    // LayersPanel::setLayers() deletes the previous rows via deleteLater()
    // (see its own docs for why) - QTest::qWait(0) spins the event loop
    // once so the initial (createFreshTestProject()'s own) row is
    // actually gone before counting below, the same as it would have been
    // by the time a real user's next interaction runs.
    QTest::qWait(0);
    QCOMPARE(panel->findChildren<QLabel*>(QStringLiteral("nameLabel")).size(), 2);
}

void MainWindowTest::toggleLayerVisibilityHidesALayerFromTopmostLookup() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-layers-visibility.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);
    const auto layerId = window.project()->layers().back().id();

    // poolTopmostLayerNow() needs a topmost layer with content - a clean,
    // already-established way to observe topmostLayerWithContent()
    // (private) skipping a hidden layer without exposing it directly.
    QVERIFY(window.poolTopmostLayerNow());

    window.toggleLayerVisibility(layerId, false);

    QVERIFY(!window.poolTopmostLayerNow());
}

void MainWindowTest::toggleLayerVisibilityMarksUnsavedChanges() {
    MainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();
    QVERIFY(!window.hasUnsavedChanges());

    window.toggleLayerVisibility(backgroundId, false);

    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::setLayerOpacityChangesTheLayersOpacity() {
    MainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();

    window.setLayerOpacity(backgroundId, 0.5f);

    QCOMPARE(window.project()->layers().front().opacity(), 0.5f);
    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::setLayerTranslationChangesTheLayersTranslation() {
    MainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();

    window.setLayerTranslation(backgroundId, 150);

    QCOMPARE(window.project()->layers().front().translationColumns(), static_cast<std::int64_t>(150));
    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::setLayerRescaleChangesTheLayersRescale() {
    MainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();

    window.setLayerRescale(backgroundId, 2.0);

    QCOMPARE(window.project()->layers().front().rescaleFactor(), 2.0);
    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::renameLayerToRenamesTheLayer() {
    MainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();

    const bool ok = window.renameLayerTo(backgroundId, QStringLiteral("Floor"));

    QVERIFY(ok);
    QCOMPARE(QString::fromStdString(window.project()->layers().front().name()), QStringLiteral("Floor"));
    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::renameLayerToFailsForAnEmptyName() {
    MainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();

    const bool ok = window.renameLayerTo(backgroundId, QString());

    QVERIFY(!ok);
    QCOMPARE(QString::fromStdString(window.project()->layers().front().name()), QStringLiteral("Background"));
}

void MainWindowTest::renameLayerToFailsForAnUnknownId() {
    MainWindow window;
    createFreshTestProject(window);

    QVERIFY(!window.renameLayerTo(999999, QStringLiteral("Nope")));
}

void MainWindowTest::deleteLayerRemovesANormalLayer() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-layers-delete.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);
    const auto layerId = window.project()->layers().back().id();
    const std::size_t countBefore = window.project()->layers().size();

    window.deleteLayer(layerId);

    QCOMPARE(window.project()->layers().size(), countBefore - 1);
}

void MainWindowTest::deleteLayerRefusesToDeleteTheBackgroundLayer() {
    MainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();
    const std::size_t countBefore = window.project()->layers().size();

    window.deleteLayer(backgroundId);

    QCOMPARE(window.project()->layers().size(), countBefore);
}

void MainWindowTest::reorderLayersAppliesAValidPermutation() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-layers-reorder.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);
    const auto backgroundId = window.project()->layers().at(0).id();
    const auto importedId = window.project()->layers().at(1).id();

    window.reorderLayers({importedId, backgroundId});

    QCOMPARE(window.project()->layers().at(0).id(), importedId);
    QCOMPARE(window.project()->layers().at(1).id(), backgroundId);
    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::reorderLayersRejectsAnInvalidPermutation() {
    MainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();

    window.reorderLayers({backgroundId, 999999});  // not a valid permutation.

    QCOMPARE(window.project()->layers().size(), static_cast<std::size_t>(1));
    QVERIFY(!window.hasUnsavedChanges());
}

void MainWindowTest::changingARealRowsOpacitySliderDoesNotCrash() {
    // Regression test for a real crash: dragging a row's opacity slider
    // (or double-clicking its name to rename) used to crash the whole
    // app. Root cause: MainWindow::setLayerOpacity()/renameLayerTo() both
    // call refreshLayersPanel(), which used to *synchronously* delete
    // every row widget - including the very slider/label still further
    // up this same call stack, mid-emission of its own valueChanged()/
    // doubleClicked() signal. LayersPanel::setLayers() now uses
    // deleteLater() instead - this test drives the *real* embedded
    // slider directly (not MainWindow::setLayerOpacity() on its own,
    // which wouldn't reproduce the crash - nothing would be "mid-
    // emission"), so it actually exercises the fixed code path. Renaming
    // shares the identical setLayers() call and isn't separately
    // exercised here only because it needs a real QInputDialog, which
    // would block this headless test - see importAudioFile()'s docs.
    MainWindow window;
    createFreshTestProject(window);

    auto* panel = window.findChild<LayersPanel*>();
    QVERIFY(panel != nullptr);
    auto* slider = panel->findChild<QSlider*>(QStringLiteral("opacitySlider"));
    QVERIFY(slider != nullptr);

    slider->setValue(42);  // Emits valueChanged() for real, synchronously.

    // If this line is reached at all, the process didn't crash.
    QCOMPARE(window.project()->layers().front().opacity(), 0.42f);
}

void MainWindowTest::toggleLoopModeDoesNothingWithNoProjectOpen() {
    // loopEngine_ doesn't exist until setProject() has been called at
    // least once (see the class docs' v0.Y.12.1 note) - toggling before
    // that must be a plain no-op, not a null-dereference crash.
    MainWindow window;
    QVERIFY(!window.isLoopModeRunning());

    window.toggleLoopMode();

    QVERIFY(!window.isLoopModeRunning());
    QVERIFY(window.isShowingLandingPage());
}

void MainWindowTest::settingProjectReconfiguresTheLoopEngineForItsOwnSettings() {
    // Regression test for the v0.Y.12.1 construction-time-config fix:
    // loopEngine_ used to be a single member built once, before any
    // project existed, with a hardcoded default config - switching to a
    // second, differently-configured project never reconfigured it (see
    // the class docs' v0.Y.11.1 note). Not directly inspectable
    // (loopEngine_ is private) - this exercises the observable
    // consequence instead: Loop Mode still starts and adds a layer
    // normally on a *second* project, with different settings (and so a
    // different loop length) than the first.
    MainWindow window;
    createFreshTestProject(window);
    window.toggleLoopMode();
    QVERIFY(window.isLoopModeRunning());
    window.toggleLoopMode();  // stop before switching projects.

    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 2048;  // different from ProjectSettings{}'s default (1024).
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-loop-reconfigure.smproj";
    QVERIFY(window.createProjectAt(settings, path));

    const std::size_t layerCountBefore = window.project()->layers().size();
    window.toggleLoopMode();

    QVERIFY(window.isLoopModeRunning());
    QCOMPARE(window.project()->layers().size(), layerCountBefore + 1);

    window.toggleLoopMode();  // cleanup.
    std::filesystem::remove(path);
}

void MainWindowTest::setKeepLoopingForwardsToTheLoopEngine() {
    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(!window.keepLooping());

    window.setKeepLooping(true);
    QVERIFY(window.keepLooping());

    window.setKeepLooping(false);
    QVERIFY(!window.keepLooping());
}

void MainWindowTest::setKeepLoopingDoesNothingWithNoProjectOpen() {
    MainWindow window;
    QVERIFY(!window.keepLooping());

    window.setKeepLooping(true);  // must not crash - loopEngine_ is still null.

    QVERIFY(!window.keepLooping());
}

void MainWindowTest::toggleLoopModeReusesAnExistingLoopInputLayerInsteadOfCreatingANewOne() {
    // Regression test: starting Loop Mode used to unconditionally add a
    // brand new "Loop Input" layer every time, so stopping and restarting
    // (or reopening a project that already captured one) piled up
    // duplicates instead of continuing to build on the same one - see
    // toggleLoopMode()'s own docs.
    MainWindow window;
    createFreshTestProject(window);
    const std::size_t layerCountBefore = window.project()->layers().size();

    window.toggleLoopMode();  // start - creates "Loop Input".
    QVERIFY(window.isLoopModeRunning());
    QCOMPARE(window.project()->layers().size(), layerCountBefore + 1);

    window.toggleLoopMode();  // stop.
    QVERIFY(!window.isLoopModeRunning());

    window.toggleLoopMode();  // start again.
    QVERIFY(window.isLoopModeRunning());

    int loopInputCount = 0;
    for (const auto& layer : window.project()->layers()) {
        if (layer.name() == "Loop Input") {
            ++loopInputCount;
        }
    }
    QCOMPARE(loopInputCount, 1);
    QCOMPARE(window.project()->layers().size(), layerCountBefore + 1);

    window.toggleLoopMode();  // cleanup.
}

void MainWindowTest::toggleLoopModeGivesANewLoopInputLayerAPlaceholderContentImmediately() {
    // Regression test: a freshly created "Loop Input" layer used to have
    // no content at all until the first whole loop finished capturing -
    // which can take as long as the project's own duration, with nothing
    // rendered on the canvas in the meantime - easily read as "Loop Mode
    // isn't capturing anything". toggleLoopMode() now seeds it with
    // LoopEngine::emptyImage() immediately, before any real capture has
    // happened.
    MainWindow window;
    createFreshTestProject(window);

    window.toggleLoopMode();
    QVERIFY(window.isLoopModeRunning());

    const sound_mind::core::Layer& loopLayer = window.project()->layers().back();
    QCOMPARE(QString::fromStdString(loopLayer.name()), QStringLiteral("Loop Input"));
    QVERIFY(loopLayer.content().has_value());
    QVERIFY(loopLayer.content()->frameCount > 0);

    window.toggleLoopMode();  // cleanup.
}

void MainWindowTest::transportPanelsStayHiddenByDefaultEvenAfterAProjectExists() {
    // Unlike the Layers panel (shown automatically the first time a
    // project exists - see layersPanelIsHiddenUntilAProjectExists()),
    // Playback/Record/Loop start OFF and stay OFF until the user
    // explicitly toggles one on, confirmed with the user.
    MainWindow window;
    auto* loopPanel = window.findChild<LoopPanel*>();
    auto* recordPanel = window.findChild<RecordPanel*>();
    auto* playbackPanel = window.findChild<PlaybackPanel*>();
    QVERIFY(loopPanel != nullptr);
    QVERIFY(recordPanel != nullptr);
    QVERIFY(playbackPanel != nullptr);
    QVERIFY(loopPanel->isHidden());
    QVERIFY(recordPanel->isHidden());
    QVERIFY(playbackPanel->isHidden());

    createFreshTestProject(window);

    QVERIFY(loopPanel->isHidden());
    QVERIFY(recordPanel->isHidden());
    QVERIFY(playbackPanel->isHidden());
}

void MainWindowTest::panelVisibilityPersistsAcrossProjectSwitches() {
    // Confirmed with the user: a manual show/hide choice persists across
    // New/Open Project within the same session, rather than resetting to
    // the OFF-by-default/ON-by-default state every time setProject() runs.
    MainWindow window;
    createFreshTestProject(window);
    auto* playbackPanel = window.findChild<PlaybackPanel*>();
    QVERIFY(playbackPanel != nullptr);
    QVERIFY(playbackPanel->isHidden());

    playbackPanel->show();  // simulate toggling it on via the toolbar.
    QVERIFY(!playbackPanel->isHidden());

    createFreshTestProject(window);  // switch to a different project.

    QVERIFY(!playbackPanel->isHidden());
}

void MainWindowTest::layersToggleActionShowsAndHidesTheLayersPanel() {
    MainWindow window;
    createFreshTestProject(window);
    auto* panel = window.findChild<LayersPanel*>();
    QVERIFY(panel != nullptr);
    QVERIFY(!panel->isHidden());  // shown by default - see layersPanelIsHiddenUntilAProjectExists().

    QAction* toggleAction = panel->toggleViewAction();
    QVERIFY(toggleAction != nullptr);
    QVERIFY(toggleAction->isCheckable());
    QVERIFY(toggleAction->isChecked());  // synced to match - see setProject()'s own comment for the bug this guards.
    auto* toolBar = window.findChild<QToolBar*>();
    QVERIFY(toolBar != nullptr);
    QVERIFY(toolBar->actions().contains(toggleAction));

    // The real bug this whole test was actually chasing (found via manual
    // testing, and only reproduced once realizing it had nothing to do
    // with this suite's usual "MainWindow is never shown()" limitation):
    // QDockWidget::toggleViewAction()'s checked state can be freely
    // flipped either way regardless, but the dock only actually hides
    // when QDockWidget::DockWidgetClosable is one of its own features -
    // LayersPanel's constructor originally left it out. See
    // LayersPanel::LayersPanel()'s own comment for the fix.
    toggleAction->trigger();
    QVERIFY(panel->isHidden());

    toggleAction->trigger();
    QVERIFY(!panel->isHidden());
}

void MainWindowTest::toggleLoopModeSyncsTheLoopPanelsRunningState() {
    MainWindow window;
    createFreshTestProject(window);
    auto* button = window.findChild<QPushButton*>(QStringLiteral("loopToggleButton"));
    QVERIFY(button != nullptr);
    QVERIFY(!button->isChecked());

    window.toggleLoopMode();
    QVERIFY(button->isChecked());
    QCOMPARE(button->text(), QStringLiteral("Stop Loop"));

    window.toggleLoopMode();
    QVERIFY(!button->isChecked());
    QCOMPARE(button->text(), QStringLiteral("Start Loop"));
}

void MainWindowTest::toggleRecordingSyncsTheRecordPanelsRecordingState() {
    MainWindow window;
    createFreshTestProject(window);
    auto* button = window.findChild<QPushButton*>(QStringLiteral("recordToggleButton"));
    QVERIFY(button != nullptr);
    QVERIFY(!button->isChecked());

    window.toggleRecording();
    QVERIFY(button->isChecked());
    QCOMPARE(button->text(), QStringLiteral("Stop Recording"));

    window.toggleRecording();
    QVERIFY(!button->isChecked());
    QCOMPARE(button->text(), QStringLiteral("Start Recording"));
}

void MainWindowTest::setKeepLoopingSyncsTheLoopPanelsCheckBox() {
    MainWindow window;
    createFreshTestProject(window);
    auto* checkBox = window.findChild<QCheckBox*>(QStringLiteral("keepLoopingCheckBox"));
    QVERIFY(checkBox != nullptr);

    window.setKeepLooping(true);
    QVERIFY(checkBox->isChecked());

    window.setKeepLooping(false);
    QVERIFY(!checkBox->isChecked());
}

void MainWindowTest::setLoopInputDeviceForwardsToTheLoopEngine() {
    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.loopInputDevice().isEmpty());

    window.setLoopInputDevice(QStringLiteral("Some Microphone"));
    QCOMPARE(window.loopInputDevice(), QStringLiteral("Some Microphone"));
}

void MainWindowTest::setLoopOutputDeviceForwardsToTheLoopEngine() {
    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.loopOutputDevice().isEmpty());

    window.setLoopOutputDevice(QStringLiteral("Some Speakers"));
    QCOMPARE(window.loopOutputDevice(), QStringLiteral("Some Speakers"));
}

void MainWindowTest::setRecordInputDeviceForwardsToTheRecordEngine() {
    MainWindow window;
    QVERIFY(window.recordInputDevice().isEmpty());

    window.setRecordInputDevice(QStringLiteral("Some Microphone"));
    QCOMPARE(window.recordInputDevice(), QStringLiteral("Some Microphone"));
}

void MainWindowTest::setPlaybackVolumeForwardsToThePlaybackEngine() {
    MainWindow window;
    QCOMPARE(window.playbackVolume(), 1.0f);

    window.setPlaybackVolume(150);
    QCOMPARE(window.playbackVolume(), 1.5f);
}

void MainWindowTest::loopPanelToggleButtonStartsAndStopsTheRealEngine() {
    // End-to-end wiring check, driving the real embedded button rather
    // than calling toggleLoopMode() directly.
    MainWindow window;
    createFreshTestProject(window);
    auto* button = window.findChild<QPushButton*>(QStringLiteral("loopToggleButton"));
    QVERIFY(button != nullptr);

    button->click();
    QVERIFY(window.isLoopModeRunning());

    button->click();
    QVERIFY(!window.isLoopModeRunning());
}

void MainWindowTest::playbackPanelButtonsDriveRealPlayback() {
    // End-to-end wiring check, driving the real embedded Play/Stop buttons
    // rather than calling startPlayback()/stopPlayback() directly.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-playback-panel-buttons.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    auto* playButton = window.findChild<QPushButton*>(QStringLiteral("playButton"));
    auto* stopButton = window.findChild<QPushButton*>(QStringLiteral("stopButton"));
    QVERIFY(playButton != nullptr);
    QVERIFY(stopButton != nullptr);

    playButton->click();
    QVERIFY(window.isPlaying());

    stopButton->click();
    QVERIFY(!window.isPlaying());
}

void MainWindowTest::audioSnippetsForFileReturnsOneSnippetForAudioNoLongerThanTheProject() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-snippets-short.wav";
    writeTestWavFile(path);  // 4 samples - far shorter than any project's own duration.

    MainWindow window;
    createFreshTestProject(window);

    const auto snippets = window.audioSnippetsForFile(path);
    std::filesystem::remove(path);

    QCOMPARE(snippets.size(), static_cast<std::size_t>(1));
    QCOMPARE(snippets.front().index, static_cast<std::size_t>(0));
    QCOMPARE(snippets.front().startSeconds, 0.0);
}

void MainWindowTest::audioSnippetsForFileSplitsLongerAudioIntoProjectLengthSegments() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-snippets-long.wav";
    constexpr std::size_t loopLengthSamples = 3528;  // 8 * 441 - see smallCanvasProjectSettings()'s docs.
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 7 / 2);  // 3.5 loops - a shorter final snippet.

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-snippets-long.smproj";
    MainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));

    const auto snippets = window.audioSnippetsForFile(path);
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QCOMPARE(snippets.size(), static_cast<std::size_t>(4));
    for (std::size_t i = 0; i < snippets.size(); ++i) {
        QCOMPARE(snippets[i].index, i);
    }
    constexpr double sampleRate = 44100.0;
    QCOMPARE(snippets[0].startSeconds, 0.0);
    QCOMPARE(snippets[0].endSeconds, loopLengthSamples / sampleRate);
    // The final snippet is shorter - half a loop's worth.
    const double finalDuration = snippets[3].endSeconds - snippets[3].startSeconds;
    QVERIFY(finalDuration < loopLengthSamples / sampleRate);
    QVERIFY(finalDuration > 0.0);
}

void MainWindowTest::audioSnippetsForFileFailsGracefullyWithNoProjectOpen() {
    MainWindow window;
    const auto snippets =
        window.audioSnippetsForFile(std::filesystem::temp_directory_path() / "sound-mind-does-not-exist.wav");
    QVERIFY(snippets.empty());
}

void MainWindowTest::importAudioFileImportsEveryComputedSnippetForLongAudio() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-snippets-all.wav";
    constexpr std::size_t loopLengthSamples = 3528;
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 3);  // exactly 3 whole snippets.

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-import-snippets-all.smproj";
    MainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    const bool ok = window.importAudioFile(path);
    const std::string stem = path.stem().string();
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    QCOMPARE(window.project()->layers().size(), layerCountBefore + 3);
    QCOMPARE(QString::fromStdString(window.project()->layers()[layerCountBefore].name()),
              QString::fromStdString(stem + "_0000"));
    QCOMPARE(QString::fromStdString(window.project()->layers()[layerCountBefore + 1].name()),
              QString::fromStdString(stem + "_0001"));
    QCOMPARE(QString::fromStdString(window.project()->layers()[layerCountBefore + 2].name()),
              QString::fromStdString(stem + "_0002"));
}

void MainWindowTest::importAudioSnippetsImportsOnlyTheRequestedSubset() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-snippets-subset.wav";
    constexpr std::size_t loopLengthSamples = 3528;
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 4);  // 4 whole snippets: 0, 1, 2, 3.

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-import-snippets-subset.smproj";
    MainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    // Requested out of order - confirms imported layers land in ascending
    // position order regardless, not request order.
    const bool ok = window.importAudioSnippets(path, {2, 0});
    const std::string stem = path.stem().string();
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    QCOMPARE(window.project()->layers().size(), layerCountBefore + 2);
    QCOMPARE(QString::fromStdString(window.project()->layers()[layerCountBefore].name()),
              QString::fromStdString(stem + "_0000"));
    QCOMPARE(QString::fromStdString(window.project()->layers()[layerCountBefore + 1].name()),
              QString::fromStdString(stem + "_0002"));
}

void MainWindowTest::importAudioSnippetsSkipsOutOfRangeIndicesGracefully() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-snippets-range.wav";
    constexpr std::size_t loopLengthSamples = 3528;
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 2);  // exactly 2 snippets: 0, 1.

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-import-snippets-range.smproj";
    MainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    const bool ok = window.importAudioSnippets(path, {0, 99});  // 99 is out of range.
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);  // at least one requested snippet (0) was imported.
    QCOMPARE(window.project()->layers().size(), layerCountBefore + 1);
}

void MainWindowTest::importAudioSnippetsFailsWhenNothingWasImported() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-snippets-none.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    const std::size_t layerCountBefore = window.project()->layers().size();

    const bool ok = window.importAudioSnippets(path, {});  // nothing requested.
    std::filesystem::remove(path);

    QVERIFY(!ok);
    QCOMPARE(window.project()->layers().size(), layerCountBefore);
}

namespace {

/// @brief Writes a 30x20 PNG to `path` - the fixed source size every Image
/// Import Scaling test uses, paired with imageScalingTestProjectSettings()'s
/// 100x50 project so each scale mode's result is unambiguous (see that
/// function's own docs).
void writeImageScalingTestImage(const std::filesystem::path& path) {
    QImage image(30, 20, QImage::Format_RGB32);
    image.fill(Qt::red);
    QVERIFY2(image.save(QString::fromStdString(path.string())), "failed to write the test PNG");
}

}  // namespace

void MainWindowTest::importImageFileRescaleToFitProjectStretchesBothAxes() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-fit.png";
    writeImageScalingTestImage(path);
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-fit.smproj";

    MainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));

    const bool ok = window.importImageFile(path, ImageScalePickerDialog::Mode::RescaleToFitProject);
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    const auto& content = *window.project()->layers().back().content();
    QCOMPARE(content.frameCount, static_cast<std::uint32_t>(100));    // project's canvasWidth.
    QCOMPARE(content.config.binCount, static_cast<std::uint32_t>(50));  // project's binCount.
}

void MainWindowTest::importImageFileScaleVerticalKeepHorizontalKeepsNativeWidth() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-v.png";
    writeImageScalingTestImage(path);
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-v.smproj";

    MainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));

    const bool ok = window.importImageFile(path, ImageScalePickerDialog::Mode::ScaleVerticalKeepHorizontal);
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    const auto& content = *window.project()->layers().back().content();
    QCOMPARE(content.frameCount, static_cast<std::uint32_t>(30));     // the source image's own native width.
    QCOMPARE(content.config.binCount, static_cast<std::uint32_t>(50));  // project's binCount.
}

void MainWindowTest::importImageFileScaleHorizontalKeepVerticalKeepsNativeHeight() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-h.png";
    writeImageScalingTestImage(path);
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-h.smproj";

    MainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));

    const bool ok = window.importImageFile(path, ImageScalePickerDialog::Mode::ScaleHorizontalKeepVertical);
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    const auto& content = *window.project()->layers().back().content();
    QCOMPARE(content.frameCount, static_cast<std::uint32_t>(100));    // project's canvasWidth.
    QCOMPARE(content.config.binCount, static_cast<std::uint32_t>(20));  // the source image's own native height.
}

void MainWindowTest::importImageFileScaleVerticalProportionalPreservesAspectRatio() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-proportional.png";
    writeImageScalingTestImage(path);
    const auto projectPath =
        std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-proportional.smproj";

    MainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));

    const bool ok = window.importImageFile(path, ImageScalePickerDialog::Mode::ScaleVerticalProportional);
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    const auto& content = *window.project()->layers().back().content();
    QCOMPARE(content.config.binCount, static_cast<std::uint32_t>(50));  // project's binCount.
    QCOMPARE(content.frameCount, static_cast<std::uint32_t>(75));       // 30 * 50 / 20, preserving the 3:2 aspect ratio.
}

void MainWindowTest::importImageFileKeepNativeResolutionDoesNotRescale() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-native.png";
    writeImageScalingTestImage(path);
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-native.smproj";

    MainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));

    const bool ok = window.importImageFile(path, ImageScalePickerDialog::Mode::KeepNativeResolution);
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    const auto& content = *window.project()->layers().back().content();
    QCOMPARE(content.frameCount, static_cast<std::uint32_t>(30));     // the source image's own native width.
    QCOMPARE(content.config.binCount, static_cast<std::uint32_t>(20));  // the source image's own native height.
}

void MainWindowTest::handleDroppedFilesImportsAWavFile() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-drop.wav";
    writeTestWavFile(path);

    MainWindow window;
    createFreshTestProject(window);
    const std::size_t layerCountBefore = window.project()->layers().size();

    window.handleDroppedFiles({path});
    std::filesystem::remove(path);

    QCOMPARE(window.project()->layers().size(), layerCountBefore + 1);
}

void MainWindowTest::handleDroppedFilesImportsAnImageFile() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-drop.png";
    writeImageScalingTestImage(path);

    MainWindow window;
    createFreshTestProject(window);
    const std::size_t layerCountBefore = window.project()->layers().size();

    window.handleDroppedFiles({path});
    std::filesystem::remove(path);

    QCOMPARE(window.project()->layers().size(), layerCountBefore + 1);
}

void MainWindowTest::handleDroppedFilesOpensASmprojFile() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-drop-open.smproj";
    {
        // A separate window just to create the file on disk - dropped onto
        // a second, fresh window below.
        MainWindow writer;
        QVERIFY(writer.createProjectAt(sound_mind::core::ProjectSettings{}, projectPath));
    }

    MainWindow window;
    QVERIFY(window.isShowingLandingPage());

    window.handleDroppedFiles({projectPath});
    std::filesystem::remove(projectPath);

    QVERIFY(!window.isShowingLandingPage());
}

void MainWindowTest::handleDroppedFilesIgnoresUnrecognizedExtensions() {
    MainWindow window;
    createFreshTestProject(window);
    const std::size_t layerCountBefore = window.project()->layers().size();

    // A real file (so any accidental "try to read/import it" path would
    // have something to fail on) with an extension nothing routes on.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-drop-ignored.txt";
    {
        std::ofstream stream(path);
        stream << "not audio, not an image, not a project";
    }

    window.handleDroppedFiles({path});
    std::filesystem::remove(path);

    QCOMPARE(window.project()->layers().size(), layerCountBefore);
}

void MainWindowTest::handleDroppedFilesRoutesMultipleFilesInOrder() {
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-drop-multi.wav";
    writeTestWavFile(wavPath);
    const auto imagePath = std::filesystem::temp_directory_path() / "sound-mind-test-drop-multi.png";
    writeImageScalingTestImage(imagePath);
    const auto ignoredPath = std::filesystem::temp_directory_path() / "sound-mind-test-drop-multi.txt";
    {
        std::ofstream stream(ignoredPath);
        stream << "ignored";
    }

    MainWindow window;
    createFreshTestProject(window);
    const std::size_t layerCountBefore = window.project()->layers().size();

    window.handleDroppedFiles({wavPath, ignoredPath, imagePath});
    std::filesystem::remove(wavPath);
    std::filesystem::remove(imagePath);
    std::filesystem::remove(ignoredPath);

    // Both the audio and image files became layers; the ignored one
    // contributed nothing - exactly two new layers, not three.
    QCOMPARE(window.project()->layers().size(), layerCountBefore + 2);
}

void MainWindowTest::handleDroppedFilesSmprojRefusesWhileLoopModeIsRunning() {
    MainWindow window;
    createFreshTestProject(window);
    window.toggleLoopMode();
    QVERIFY(window.isLoopModeRunning());
    // toggleLoopMode() itself marks hasUnsavedChanges() - clear it with a
    // real save first, so handleDroppedFiles()'s own
    // confirmDiscardUnsavedChanges() call returns immediately below
    // instead of blocking on a real QMessageBox (this test's own real bug,
    // caught by a genuine 300s timeout the first time this test ran).
    window.saveProject();
    QVERIFY(!window.hasUnsavedChanges());

    // openProjectAt() itself refuses (no dialog) while Loop Mode is
    // running - handleDroppedFiles() must not crash propagating that,
    // and must leave the current project (and Loop Mode) untouched.
    window.handleDroppedFiles({std::filesystem::temp_directory_path() / "sound-mind-does-not-exist.smproj"});

    QVERIFY(window.isLoopModeRunning());

    window.toggleLoopMode();  // cleanup.
}

void MainWindowTest::handleDroppedFilesAppliesTheGivenImageMode() {
    // dropEvent() (untestable directly - see its own docs) decides the
    // mode/sequence via a real ImageScalePickerDialog and passes the
    // result to handleDroppedFiles(); this proves handleDroppedFiles()
    // itself actually honors whatever it's given, rather than always
    // falling back to its own default (RescaleToFitProject) - a non-default
    // mode's own dimensions (KeepNativeResolution's) prove it took effect.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-drop-image-mode.png";
    writeImageScalingTestImage(path);  // 30x20.
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-drop-image-mode.smproj";

    MainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));  // 100x50 canvas.

    window.handleDroppedFiles({path}, ImageScalePickerDialog::Mode::KeepNativeResolution, /*importAsSequence=*/false);
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    const auto& content = *window.project()->layers().back().content();
    QCOMPARE(content.frameCount, static_cast<std::uint32_t>(30));      // native, not the canvas's 100.
    QCOMPARE(content.config.binCount, static_cast<std::uint32_t>(20));  // native, not the canvas's 50.
}

void MainWindowTest::handleDroppedFilesSequencesDroppedImagesWhenRequested() {
    // Same cumulative-translation math as
    // importImageFilesAppliesProportionalScalingAndCumulativeTranslationWhenSequential():
    // 30x20 into a 100x50 canvas proportionally scales to width 75.
    const auto pathA = std::filesystem::temp_directory_path() / "sound-mind-test-drop-seq-a.png";
    const auto pathB = std::filesystem::temp_directory_path() / "sound-mind-test-drop-seq-b.png";
    writeImageScalingTestImage(pathA);
    writeImageScalingTestImage(pathB);
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-drop-seq.smproj";

    MainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    window.handleDroppedFiles({pathA, pathB}, ImageScalePickerDialog::Mode::RescaleToFitProject,
                               /*importAsSequence=*/true);
    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
    std::filesystem::remove(projectPath);

    QCOMPARE(window.project()->layers().size(), layerCountBefore + 2);
    QCOMPARE(window.project()->layers()[layerCountBefore].translationColumns(), static_cast<std::int64_t>(0));
    QCOMPARE(window.project()->layers()[layerCountBefore + 1].translationColumns(), static_cast<std::int64_t>(75));
}

void MainWindowTest::handleDroppedFilesAppliesGivenAudioSnippetSelections() {
    // dropEvent() (untestable directly) computes each dropped .wav's own
    // audioSnippetsForFile() and, for one with more than one snippet, shows
    // the same AudioSnippetPickerDialog importAudio() would - giving a drop
    // parity with File -> Import Audio, confirmed with the user. This
    // proves handleDroppedFiles() itself actually honors a given selection
    // (only 2 of 4 snippets), rather than always falling back to its own
    // default (every snippet).
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-drop-audio-snippets.wav";
    constexpr std::size_t loopLengthSamples = 3528;  // 8 * 441 - see smallCanvasProjectSettings()'s docs.
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 4);  // 4 whole snippets: 0, 1, 2, 3.
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-drop-audio-snippets.smproj";

    MainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    window.handleDroppedFiles({path}, ImageScalePickerDialog::Mode::RescaleToFitProject,
                               /*importAsSequence=*/false, {{path, {0, 2}}});
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QCOMPARE(window.project()->layers().size(), layerCountBefore + 2);  // not all 4.
}

namespace {

/// @brief Writes a solid-color PNG of the given size to `path` - like
/// writeImageScalingTestImage() above, but with a caller-chosen size, for
/// Image Sequence Import's tests, which need multiple distinctly-sized
/// images to make each one's own proportional-scaling contribution to the
/// cumulative offset unambiguous.
void writeSizedTestImage(const std::filesystem::path& path, int width, int height) {
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::blue);
    QVERIFY2(image.save(QString::fromStdString(path.string())), "failed to write the test PNG");
}

}  // namespace

void MainWindowTest::importImageFilesImportsEachFileIndependentlyWhenNotSequential() {
    const auto pathA = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-a.png";
    const auto pathB = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-b.png";
    writeImageScalingTestImage(pathA);
    writeImageScalingTestImage(pathB);
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-independent.smproj";

    MainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    const bool ok = window.importImageFiles({pathA, pathB}, ImageScalePickerDialog::Mode::KeepNativeResolution,
                                             /*importAsSequence=*/false);
    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    QCOMPARE(window.project()->layers().size(), layerCountBefore + 2);
    // Neither file was translated - each imported independently, per its
    // own explicitly-chosen mode (KeepNativeResolution here).
    QCOMPARE(window.project()->layers()[layerCountBefore].translationColumns(), static_cast<std::int64_t>(0));
    QCOMPARE(window.project()->layers()[layerCountBefore + 1].translationColumns(), static_cast<std::int64_t>(0));
}

void MainWindowTest::importImageFilesAppliesProportionalScalingAndCumulativeTranslationWhenSequential() {
    // canvasWidth=100/canvasHeight=50 (imageScalingTestProjectSettings()) -
    // image A is 30x20 (aspect 3:2, proportional width round(30*50/20)=75),
    // image B is 40x50 (aspect 4:5, proportional width round(40*50/50)=40).
    // Neither offset reaches canvasWidth, so no wrap here - see
    // importImageFilesWrapsCumulativeOffsetPastCanvasWidth() for that.
    const auto pathA = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-cum-a.png";
    const auto pathB = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-cum-b.png";
    writeImageScalingTestImage(pathA);  // 30x20.
    writeSizedTestImage(pathB, 40, 50);
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-cumulative.smproj";

    MainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    // Mode is ignored when importAsSequence is true - ScaleVerticalProportional
    // always applies regardless, so pass a different mode to prove that.
    const bool ok = window.importImageFiles({pathA, pathB}, ImageScalePickerDialog::Mode::KeepNativeResolution,
                                             /*importAsSequence=*/true);
    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    QCOMPARE(window.project()->layers().size(), layerCountBefore + 2);

    const auto& layerA = window.project()->layers()[layerCountBefore];
    QCOMPARE(layerA.translationColumns(), static_cast<std::int64_t>(0));
    QCOMPARE(layerA.content()->frameCount, static_cast<std::uint32_t>(75));

    const auto& layerB = window.project()->layers()[layerCountBefore + 1];
    QCOMPARE(layerB.translationColumns(), static_cast<std::int64_t>(75));  // starts right after A's own width.
    QCOMPARE(layerB.content()->frameCount, static_cast<std::uint32_t>(40));
}

void MainWindowTest::importImageFilesWrapsCumulativeOffsetPastCanvasWidth() {
    // Three 30x20 images (each 75 columns wide once proportionally scaled -
    // see the test above) into a canvasWidth=100 project: A starts at 0
    // (0 < 100), B starts at 75 (75 < 100), and by the time C is placed the
    // running total (150) has already reached canvasWidth, so it wraps back
    // to 0 - matching the legacy Studio's own cumulative-offset behavior,
    // confirmed with the user before implementing.
    const auto pathA = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-wrap-a.png";
    const auto pathB = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-wrap-b.png";
    const auto pathC = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-wrap-c.png";
    writeImageScalingTestImage(pathA);
    writeImageScalingTestImage(pathB);
    writeImageScalingTestImage(pathC);
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-wrap.smproj";

    MainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    const bool ok = window.importImageFiles({pathA, pathB, pathC}, ImageScalePickerDialog::Mode::RescaleToFitProject,
                                             /*importAsSequence=*/true);
    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
    std::filesystem::remove(pathC);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    QCOMPARE(window.project()->layers()[layerCountBefore].translationColumns(), static_cast<std::int64_t>(0));
    QCOMPARE(window.project()->layers()[layerCountBefore + 1].translationColumns(), static_cast<std::int64_t>(75));
    QCOMPARE(window.project()->layers()[layerCountBefore + 2].translationColumns(), static_cast<std::int64_t>(0));
}

void MainWindowTest::importImageFilesSortsFilesAlphabeticallyWhenSequential() {
    const auto pathA = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-sort-a.png";
    const auto pathB = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-sort-b.png";
    writeImageScalingTestImage(pathA);
    writeImageScalingTestImage(pathB);
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-sort.smproj";

    MainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    // Passed in reverse (B, A) - sequential import must still place them in
    // filename order (A first, translation 0; B second, translation 75).
    const bool ok = window.importImageFiles({pathB, pathA}, ImageScalePickerDialog::Mode::RescaleToFitProject,
                                             /*importAsSequence=*/true);
    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    const auto& firstImported = window.project()->layers()[layerCountBefore];
    const auto& secondImported = window.project()->layers()[layerCountBefore + 1];
    QCOMPARE(firstImported.name(), pathA.filename().string());
    QCOMPARE(secondImported.name(), pathB.filename().string());
    QCOMPARE(firstImported.translationColumns(), static_cast<std::int64_t>(0));
    QCOMPARE(secondImported.translationColumns(), static_cast<std::int64_t>(75));
}

void MainWindowTest::importImageFilesSucceedsIfAtLeastOneFileImports() {
    const auto goodPath = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-partial-good.png";
    writeImageScalingTestImage(goodPath);
    const auto badPath = std::filesystem::temp_directory_path() / "sound-mind-does-not-exist.png";
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-partial.smproj";

    MainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    QString errorMessage;
    const bool ok = window.importImageFiles({badPath, goodPath}, ImageScalePickerDialog::Mode::KeepNativeResolution,
                                             /*importAsSequence=*/false, &errorMessage);
    std::filesystem::remove(goodPath);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    QCOMPARE(window.project()->layers().size(), layerCountBefore + 1);
}

void MainWindowTest::importImageFilesFailsWhenNothingWasImported() {
    const auto badPathA = std::filesystem::temp_directory_path() / "sound-mind-does-not-exist-a.png";
    const auto badPathB = std::filesystem::temp_directory_path() / "sound-mind-does-not-exist-b.png";
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-all-fail.smproj";

    MainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    QString errorMessage;
    const bool ok = window.importImageFiles({badPathA, badPathB}, ImageScalePickerDialog::Mode::KeepNativeResolution,
                                             /*importAsSequence=*/false, &errorMessage);
    std::filesystem::remove(projectPath);

    QVERIFY(!ok);
    QVERIFY(!errorMessage.isEmpty());
    QCOMPARE(window.project()->layers().size(), layerCountBefore);
}

void MainWindowTest::windowTitleIncludesTheProjectNameOnceOneExists() {
    MainWindow window;
    QVERIFY(!window.windowTitle().contains(QStringLiteral(" - ")));  // no project yet.

    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-window-title.smproj";
    QVERIFY(window.createProjectAt(sound_mind::core::ProjectSettings{}, path));

    QVERIFY(window.windowTitle().contains(QStringLiteral("sound-mind-test-window-title")));
}

void MainWindowTest::startPlaybackSetsThePlaybackPanelDuration() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-playback-duration.wav";
    writeTestWavFileWithFrameCount(path, 44100, 44100);  // exactly 1 second.

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    window.startPlayback();

    auto* panel = window.findChild<PlaybackPanel*>();
    QVERIFY(panel != nullptr);
    auto* label = panel->findChild<QLabel*>(QStringLiteral("positionLabel"));
    QVERIFY(label != nullptr);
    QCOMPARE(label->text(), QStringLiteral("0:00 / 0:01"));

    window.stopPlayback();
}

void MainWindowTest::seekPlaybackMovesThePlaybackPosition() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-playback-seek.wav";
    writeTestWavFileWithFrameCount(path, 441000, 44100);  // exactly 10 seconds.

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    window.startPlayback();
    window.seekPlayback(5.0);

    auto* panel = window.findChild<PlaybackPanel*>();
    QVERIFY(panel != nullptr);
    auto* label = panel->findChild<QLabel*>(QStringLiteral("positionLabel"));
    QVERIFY(label != nullptr);
    QCOMPARE(label->text(), QStringLiteral("0:05 / 0:10"));

    window.stopPlayback();
}

void MainWindowTest::stopPlaybackResetsThePlaybackPanelPosition() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-playback-stop.wav";
    writeTestWavFileWithFrameCount(path, 441000, 44100);  // exactly 10 seconds.

    MainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    window.startPlayback();
    window.seekPlayback(5.0);

    window.stopPlayback();

    auto* panel = window.findChild<PlaybackPanel*>();
    QVERIFY(panel != nullptr);
    auto* label = panel->findChild<QLabel*>(QStringLiteral("positionLabel"));
    QVERIFY(label != nullptr);
    QCOMPARE(label->text(), QStringLiteral("0:00 / 0:00"));
}
