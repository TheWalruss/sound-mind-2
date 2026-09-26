#include "test_main_window.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFile>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QStatusBar>
#include <QToolBar>
#include <QUrl>
#include <QtTest/QtTest>

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/gpu_compute_availability.h"
#include "sound_mind/core/gradient.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/paste_operation.h"
#include "sound_mind/core/playback_engine.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/filter_configuration_panel.h"
#include "sound_mind/studio/image_scale_picker_dialog.h"
#include "sound_mind/studio/landing_page.h"
#include "sound_mind/studio/layers_panel.h"
#include "sound_mind/studio/loop_panel.h"
#include "sound_mind/studio/main_window.h"
#include "sound_mind/studio/playback_panel.h"
#include "sound_mind/studio/record_panel.h"
#include "sound_mind/studio/selection_configuration_panel.h"
#include "sound_mind/studio/tool_configuration_panel.h"

using sound_mind::codec::CompressedAudioFormat;
using sound_mind::core::BlendMode;
using sound_mind::core::FillOperation;
using sound_mind::core::Gradient;
using sound_mind::core::PaintOperation;
using sound_mind::core::PasteOperation;
using sound_mind::core::PathNodeType;
using sound_mind::studio::CanvasWidget;
using sound_mind::studio::ImageScalePickerDialog;
using sound_mind::studio::LandingPage;
using sound_mind::studio::FilterConfigurationPanel;
using sound_mind::studio::LayersPanel;
using sound_mind::studio::LoopPanel;
using sound_mind::studio::MainWindow;
using sound_mind::studio::PlaybackPanel;
using sound_mind::studio::PlaybackScope;
using sound_mind::studio::RecordPanel;
using sound_mind::studio::SelectionConfigurationPanel;
using sound_mind::studio::ToolConfigurationPanel;

namespace {

/// @brief The `MainWindow` this whole test file actually constructs -
/// identical in every other respect, just always attached with
/// `AudioDeviceMode::None` instead of `MainWindow`'s own real-device
/// default.
///
/// Before this existed, every `MainWindow` construction in this file - all
/// ~115 of them, regardless of what that particular test actually
/// exercised - opened and closed two real system audio devices (one in via
/// `recordEngine_`, one out via `playbackController_`), since neither had
/// any way to be told otherwise. That's real device driver I/O on every
/// single test, the dominant cost behind this suite's own slowness (found
/// while investigating why `ctest` took over 12 minutes on this one
/// binary). None of this file's tests depend on real device
/// enumeration/behavior (grep-confirmed before making this change) - the
/// few that exercise device-preference forwarding
/// (`setLoopInputDevice()`/`setRecordInputDevice()`/etc.) only check that a
/// caller-given name round-trips as a stored *preference* string, never
/// that a real device by that name exists.
class TestMainWindow : public MainWindow {
public:
    TestMainWindow() : MainWindow(nullptr, sound_mind::core::AudioDeviceMode::None) {}

    /// @brief The last URL passed to openExternalUrl() - see its own
    /// override below.
    std::optional<QUrl> lastOpenedUrl;

protected:
    /// @brief Records `url` instead of actually launching a real browser/OS
    /// handler (`QDesktopServices::openUrl()`'s own real effect) - see
    /// `MainWindow::openExternalUrl()`'s own docs on why this seam exists.
    void openExternalUrl(const QUrl& url) override { lastOpenedUrl = url; }
};

/// @brief The topmost layer that isn't the Equalizer - what this whole
/// file's own many `project()->layers().back()` call sites actually meant
/// before every `Project::createNew()` started adding an Equalizer layer
/// (v0.Y.28.1's own Installment D): "whichever layer a just-completed
/// action (import, paint, pool, add) most recently touched or created",
/// which is always the layer just below the Equalizer now, not the very
/// last element. A small helper here rather than updating each call site
/// to `.at(size() - 2)` individually - this doesn't assume exactly one
/// layer beyond Background/Equalizer exists, only that the Equalizer
/// itself (if present) is never the layer under test.
const sound_mind::core::Layer& topmostNonEqualizerLayer(const sound_mind::core::Project& project) {
    const auto& layers = project.layers();
    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        if (it->type() != sound_mind::core::LayerType::Equalizer) {
            return *it;
        }
    }
    return layers.back();  // Defensive only - a real project always has a Background layer at least.
}

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

/// @brief Imports the same tiny WAV `layerCount` times, giving `window`'s
/// current (default-sized, 1024x512) project that many real, contributing
/// layers - so compositeProject()'s own per-layer loop (see its docs) has
/// enough real per-cell mixing work, spread over enough layers, to give
/// requestCancel() (called right after startPlayback() starts its own
/// background composite) a reliable window to land before the whole
/// composite finishes - the same reasoning cancelPoolDiscardsTheComputedResultWithoutApplyingIt()'s
/// own comment gives for a several-second clip, applied here to "many
/// layers" instead (compositeProject()'s own cost scales with canvasWidth
/// x binCount x layer count, not with any one layer's own source clip
/// length).
void importSeveralLayersForASlowComposite(MainWindow& window, const std::filesystem::path& path,
                                           int layerCount = 20) {
    for (int i = 0; i < layerCount; ++i) {
        QVERIFY(window.importAudioFile(path));
    }
}

}  // namespace

void MainWindowTest::hasARealWindowIconNotTheDefaultOne() {
    // Per the Visual Identity milestone (v0.Y.14.1) - see theme.h.
    const TestMainWindow window;
    QVERIFY(!window.windowIcon().isNull());
}

void MainWindowTest::startsWithNoProjectOpen() {
    // Per the Landing Page milestone (v0.Y.9.1): the Studio no longer
    // silently creates an in-memory project at startup - the Landing Page
    // is shown until New/Open Project actually creates or loads one.
    const TestMainWindow window;
    QVERIFY(window.project() == nullptr);
}

void MainWindowTest::newProjectShowsTheCanvasInsteadOfTheLandingPage() {
    // Via createProjectAt() - the testable core newProject() itself calls
    // once its wizard is accepted (see its own docs) - not newProject()
    // directly, which would now block on a real dialog under this
    // headless test platform.
    TestMainWindow window;
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
        TestMainWindow writer;
        createFreshTestProject(writer);
        const_cast<sound_mind::core::Project*>(writer.project())->save(projectPath);
    }

    TestMainWindow window;
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
    TestMainWindow window;
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
        TestMainWindow writer;
        createFreshTestProject(writer);
        const_cast<sound_mind::core::Project*>(writer.project())->save(projectPath);
    }

    TestMainWindow window;
    // Populate the list the same way a real recent-project entry would get
    // there - via a prior successful open, not by reaching into internals.
    QVERIFY(window.openProjectAt(projectPath));
    createFreshTestProject(window);  // back to a fresh, different project.
    QCOMPARE(window.project()->layers().size(), static_cast<std::size_t>(2));  // Background + Equalizer.

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
    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.project() != nullptr);

    createFreshTestProject(window);  // replace it with another fresh one.

    QVERIFY(window.project() != nullptr);
    QCOMPARE(window.project()->layers().size(), static_cast<std::size_t>(2));  // Background + Equalizer.
}

void MainWindowTest::importAudioFileAddsANewLayer() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import.wav";
    writeTestWavFile(path);

    TestMainWindow window;
    createFreshTestProject(window);
    const bool ok = window.importAudioFile(path);
    std::filesystem::remove(path);

    QVERIFY(ok);
    QCOMPARE(window.project()->layers().size(), static_cast<std::size_t>(3));  // Background + Equalizer + imported.
    QVERIFY(topmostNonEqualizerLayer(*window.project()).content().has_value());
}

void MainWindowTest::importImageFileAddsANewLayer() {
    QImage image(4, 3, QImage::Format_RGB32);
    image.fill(Qt::red);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import.png";
    QVERIFY(image.save(QString::fromStdString(path.string())));

    TestMainWindow window;
    createFreshTestProject(window);
    // KeepNativeResolution - matches this test's own pre-existing intent
    // (does importing add a layer at all) rather than exercising scaling.
    const bool ok = window.importImageFile(path, ImageScalePickerDialog::Mode::KeepNativeResolution);
    std::filesystem::remove(path);

    QVERIFY(ok);
    QCOMPARE(window.project()->layers().size(), static_cast<std::size_t>(3));  // Background + Equalizer + imported.
    QVERIFY(topmostNonEqualizerLayer(*window.project()).content().has_value());
    QCOMPARE(topmostNonEqualizerLayer(*window.project()).content()->frameCount, static_cast<std::uint32_t>(4));
    QCOMPARE(topmostNonEqualizerLayer(*window.project()).content()->config.binCount, static_cast<std::uint32_t>(3));
}

void MainWindowTest::importAudioFileFailsGracefullyForAMissingFile() {
    TestMainWindow window;
    createFreshTestProject(window);
    const bool ok = window.importAudioFile(std::filesystem::temp_directory_path() / "sound-mind-does-not-exist.wav");

    QVERIFY(!ok);
    QCOMPARE(window.project()->layers().size(), static_cast<std::size_t>(2));  // Background + Equalizer, unchanged.
}

void MainWindowTest::startPlaybackDoesNothingWithNoContent() {
    // A fresh project's Background/Equalizer layers have no content yet.
    TestMainWindow window;
    createFreshTestProject(window);
    window.startPlayback();
    QVERIFY(!window.isPlaying());
}

void MainWindowTest::startPlaybackPlaysAnImportedLayer() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-playback.wav";
    writeTestWavFile(path);

    // A small canvasWidth, not createFreshTestProject()'s default 1024 - as
    // of v0.Y.27.1 (Multi-layer Compositing), startPlayback() decodes the
    // project's own real composite, which always spans exactly
    // canvasWidth's own duration - a default-sized project would decode
    // several seconds of mostly silence for writeTestWavFile()'s own
    // 4-sample clip, for no reason this test actually needs.
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-playback.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    // As of v0.0.45.20 (finding #12, Installment H), a fresh start runs
    // compositeProject() on a background task - see
    // poolTopmostLayerAsyncRunsInTheBackgroundAndShowsTheCancelButton()'s
    // own comment on why this waits for the cancel button itself, not
    // just isPlaying() (which could still read stale for a few ticks
    // after the underlying BackgroundTask finishes but before this
    // window's own ~33ms poll timer has had its own next chance to react).
    window.startPlayback();
    auto* compositeCancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(compositeCancelButton != nullptr);
    while (!compositeCancelButton->isHidden()) {
        QTest::qWait(5);
    }
    QVERIFY(window.isPlaying());
}

void MainWindowTest::pauseAndResumePlayback() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-playback-pause.wav";
    writeTestWavFile(path);

    // See startPlaybackPlaysAnImportedLayer()'s own comment on why a
    // small canvasWidth, not createFreshTestProject()'s default.
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-playback-pause.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    // See startPlaybackPlaysAnImportedLayer()'s own comment on why the
    // first (fresh) startPlayback() needs to wait for the background
    // composite - the second one (resuming after pausePlayback(), already
    // loaded) stays fully synchronous, unchanged.
    window.startPlayback();
    auto* compositeCancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(compositeCancelButton != nullptr);
    while (!compositeCancelButton->isHidden()) {
        QTest::qWait(5);
    }
    QVERIFY(window.isPlaying());

    window.pausePlayback();
    QVERIFY(!window.isPlaying());

    window.startPlayback();
    QVERIFY(window.isPlaying());
}

void MainWindowTest::stopPlaybackStopsIt() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-playback-stop.wav";
    writeTestWavFile(path);

    // See startPlaybackPlaysAnImportedLayer()'s own comment on why a
    // small canvasWidth, not createFreshTestProject()'s default.
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-playback-stop.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    // See startPlaybackPlaysAnImportedLayer()'s own comment on why this
    // waits for the background composite to finish.
    window.startPlayback();
    auto* compositeCancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(compositeCancelButton != nullptr);
    while (!compositeCancelButton->isHidden()) {
        QTest::qWait(5);
    }
    QVERIFY(window.isPlaying());

    window.stopPlayback();
    QVERIFY(!window.isPlaying());
}

void MainWindowTest::startPlaybackRunsCompositeInTheBackgroundAndShowsTheCancelButton() {
    // Real-world testing pass, 2026-09-20, finding #12 ("a real,
    // non-blocking cancel affordance for long operations"), Installment H.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-composite-async.wav";
    writeTestWavFile(path);

    TestMainWindow window;
    createFreshTestProject(window);
    importSeveralLayersForASlowComposite(window, path);
    std::filesystem::remove(path);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(cancelButton != nullptr);
    QVERIFY(cancelButton->isHidden());

    window.startPlayback();

    QVERIFY(window.isCompositingForPlayback());
    QVERIFY(!cancelButton->isHidden());

    // See exportTopmostLayerVideoAsyncRunsInTheBackgroundAndShowsTheCancelButton()'s
    // own comment on why this waits for the cancel button itself, not just
    // isCompositingForPlayback().
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
}

void MainWindowTest::cancelPlaybackCompositeDiscardsTheResultWithoutStartingPlayback() {
    // Many layers - see importSeveralLayersForASlowComposite()'s own
    // comment on why that (not a longer clip) is what gives
    // requestCancel() (called immediately after starting) a reliable
    // window to land before compositeProject()'s own per-layer loop races
    // through to completion.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-composite-cancel.wav";
    writeTestWavFile(path);

    TestMainWindow window;
    createFreshTestProject(window);
    importSeveralLayersForASlowComposite(window, path);
    std::filesystem::remove(path);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(cancelButton != nullptr);

    window.startPlayback();
    QVERIFY(window.isCompositingForPlayback());

    window.cancelPlaybackComposite();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }

    // Nothing to roll back beyond discarding the computed result -
    // playbackController_ was never touched, matching Pool's own
    // simplest-possible rollback (see cancelPoolDiscardsTheComputedResultWithoutApplyingIt()'s
    // own comment).
    QVERIFY(!window.isPlaying());
    QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("cancelled")));
}

void MainWindowTest::startPlaybackDoesNothingWhileAlreadyCompositing() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-composite-already.wav";
    writeTestWavFile(path);

    TestMainWindow window;
    createFreshTestProject(window);
    importSeveralLayersForASlowComposite(window, path);
    std::filesystem::remove(path);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(cancelButton != nullptr);

    window.startPlayback();
    QVERIFY(window.isCompositingForPlayback());

    // A second call while the first is still running is refused outright
    // (a status bar message only) rather than starting a second
    // BackgroundTask - matching every other Finding #12 installment's own
    // "already running" guard.
    window.startPlayback();
    QVERIFY(window.isCompositingForPlayback());
    QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("Already preparing")));

    window.cancelPlaybackComposite();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
}

void MainWindowTest::closeRefusesWhileCompositingForPlayback() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-composite-refuse-close.wav";
    writeTestWavFile(path);

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-composite-refuse-close.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(sound_mind::core::ProjectSettings{}, projectPath));
    importSeveralLayersForASlowComposite(window, path);
    std::filesystem::remove(path);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(cancelButton != nullptr);

    window.startPlayback();
    QVERIFY(window.isCompositingForPlayback());

    // Refused outright (no dialog reached - see closeEvent()'s docs).
    QVERIFY(!window.close());
    QVERIFY(window.isCompositingForPlayback());

    window.cancelPlaybackComposite();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
}

void MainWindowTest::newProjectRefusesWhileCompositingForPlayback() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-composite-refuse-new.wav";
    writeTestWavFile(path);

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-composite-refuse-new.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(sound_mind::core::ProjectSettings{}, projectPath));
    importSeveralLayersForASlowComposite(window, path);
    std::filesystem::remove(path);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(cancelButton != nullptr);

    window.startPlayback();
    QVERIFY(window.isCompositingForPlayback());
    const std::size_t layerCountBefore = window.project()->layers().size();

    // The real, interactive newProject() - safe to call directly, since
    // the composite check runs before the wizard would ever be shown.
    window.newProject();

    QVERIFY(window.isCompositingForPlayback());
    QCOMPARE(window.project()->layers().size(), layerCountBefore);

    window.cancelPlaybackComposite();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
}

void MainWindowTest::openProjectRefusesWhileCompositingForPlayback() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-composite-refuse-open.wav";
    writeTestWavFile(path);

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-composite-refuse-open.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(sound_mind::core::ProjectSettings{}, projectPath));
    importSeveralLayersForASlowComposite(window, path);
    std::filesystem::remove(path);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(cancelButton != nullptr);

    window.startPlayback();
    QVERIFY(window.isCompositingForPlayback());

    // Refused before ever reaching the file dialog.
    window.openProject();
    QVERIFY(window.isCompositingForPlayback());

    window.cancelPlaybackComposite();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
}

void MainWindowTest::openProjectAtRefusesWhileCompositingForPlayback() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-composite-refuse-openat.wav";
    writeTestWavFile(path);

    const auto projectPath =
        std::filesystem::temp_directory_path() / "sound-mind-test-composite-refuse-openat.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(sound_mind::core::ProjectSettings{}, projectPath));
    importSeveralLayersForASlowComposite(window, path);
    std::filesystem::remove(path);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(cancelButton != nullptr);

    window.startPlayback();
    QVERIFY(window.isCompositingForPlayback());

    QString errorMessage;
    const bool ok = window.openProjectAt(std::filesystem::temp_directory_path() / "sound-mind-does-not-exist.smproj",
                                          &errorMessage);

    QVERIFY(!ok);
    QVERIFY(!errorMessage.isEmpty());
    QVERIFY(window.isCompositingForPlayback());

    window.cancelPlaybackComposite();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
}

void MainWindowTest::poolTopmostLayerNowFailsGracefullyWithNoContent() {
    // A fresh project's Background/Equalizer layers have no content yet.
    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(!window.poolTopmostLayerNow());
}

void MainWindowTest::poolTopmostLayerNowPoolsAnImportedLayer() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-pool.wav";
    writeTestWavFile(path);

    TestMainWindow window;
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
    QCOMPARE(topmostNonEqualizerLayer(*window.project()).poolContent().has_value(), true);

    QFile::remove(streamPngPath);
    QFile::remove(poolPngPath);
}

void MainWindowTest::poolTopmostLayerAsyncRunsInTheBackgroundAndShowsTheCancelButton() {
    // Real-world testing pass, 2026-09-20, finding #12 ("a real,
    // non-blocking cancel affordance for long operations"), Installment G.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-pool-async.wav";
    writeTestWavFile(path);

    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("poolCancelButton"));
    QVERIFY(cancelButton != nullptr);
    QVERIFY(cancelButton->isHidden());

    window.poolTopmostLayerAsync();

    QVERIFY(window.isPoolRunning());
    QVERIFY(!cancelButton->isHidden());

    // See exportTopmostLayerVideoAsyncRunsInTheBackgroundAndShowsTheCancelButton()'s
    // own comment on why this waits for the cancel button itself, not just
    // isPoolRunning().
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
}

void MainWindowTest::poolTopmostLayerAsyncCompletesSuccessfullyAndAppliesTheResult() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-pool-async-success.wav";
    writeTestWavFile(path);

    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("poolCancelButton"));
    QVERIFY(cancelButton != nullptr);

    window.poolTopmostLayerAsync();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }

    QCOMPARE(topmostNonEqualizerLayer(*window.project()).poolContent().has_value(), true);
    QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("Pooled layer")));
}

void MainWindowTest::cancelPoolDiscardsTheComputedResultWithoutApplyingIt() {
    // A several-second clip - see cancelVideoExportStopsItAndDeletesThePartialFile()'s
    // own comment on why a real, non-trivial encode gives requestCancel()
    // (called immediately after starting) a reliable window to land before
    // all four of computePooledContent()'s own phases (see its docs) race
    // through to completion.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-pool-cancel.wav";
    writeTestWavFileWithFrameCount(path, 441000, 44100);  // 10 seconds.

    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("poolCancelButton"));
    QVERIFY(cancelButton != nullptr);

    window.poolTopmostLayerAsync();
    QVERIFY(window.isPoolRunning());

    window.cancelPool();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }

    // The whole point of finding #12's own "rolls back" requirement: the
    // layer's own real content is untouched.
    QCOMPARE(topmostNonEqualizerLayer(*window.project()).poolContent().has_value(), false);
    QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("Pooling cancelled."));
}

void MainWindowTest::poolTopmostLayerAsyncDoesNothingWhileAlreadyRunning() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-pool-already-running.wav";
    writeTestWavFileWithFrameCount(path, 441000, 44100);  // stays running for this whole test.

    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("poolCancelButton"));
    QVERIFY(cancelButton != nullptr);

    window.poolTopmostLayerAsync();
    QVERIFY(window.isPoolRunning());

    // A second call while the first is still running must not destroy (and
    // therefore block on joining) the still-running task.
    window.poolTopmostLayerAsync();
    QVERIFY(window.isPoolRunning());

    window.cancelPool();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
}

void MainWindowTest::closeRefusesWhileAPoolIsRunning() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-pool-refuse-close.wav";
    writeTestWavFileWithFrameCount(path, 441000, 44100);

    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("poolCancelButton"));
    QVERIFY(cancelButton != nullptr);

    window.poolTopmostLayerAsync();
    QVERIFY(window.isPoolRunning());

    QVERIFY(!window.close());
    QVERIFY(window.isPoolRunning());

    window.cancelPool();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
}

void MainWindowTest::newProjectRefusesWhileAPoolIsRunning() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-pool-refuse-new.wav";
    writeTestWavFileWithFrameCount(path, 441000, 44100);

    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("poolCancelButton"));
    QVERIFY(cancelButton != nullptr);

    window.poolTopmostLayerAsync();
    QVERIFY(window.isPoolRunning());

    window.newProject();
    QVERIFY(window.isPoolRunning());

    window.cancelPool();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
}

void MainWindowTest::exportTopmostLayerAudioNowFailsGracefullyWithNoContent() {
    // A fresh project's Background/Equalizer layers have no content yet.
    TestMainWindow window;
    createFreshTestProject(window);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-export.flac";
    QVERIFY(!window.exportTopmostLayerAudioNow(path));
    QVERIFY(!QFile::exists(QString::fromStdString(path.string())));
}

void MainWindowTest::exportTopmostLayerAudioNowExportsAnImportedLayer() {
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-audio.wav";
    writeTestWavFile(wavPath);

    TestMainWindow window;
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

    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);

    QString errorMessage;
    const auto exportPath = std::filesystem::temp_directory_path() / "sound-mind-test-export.xyz";
    QVERIFY(!window.exportTopmostLayerAudioNow(exportPath, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void MainWindowTest::exportTopmostLayerVideoNowFailsGracefullyWithNoContent() {
    // A fresh project's Background/Equalizer layers have no content yet.
    TestMainWindow window;
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
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);

    const auto exportPath = std::filesystem::temp_directory_path() / "sound-mind-test-export.mp4";
    const bool ok = window.exportTopmostLayerVideoNow(exportPath);

    QVERIFY(ok);
    QVERIFY(QFile::exists(QString::fromStdString(exportPath.string())));
    std::filesystem::remove(exportPath);
}

void MainWindowTest::exportTopmostLayerVideoAsyncRunsInTheBackgroundAndShowsTheCancelButton() {
    // Real-world testing pass, 2026-09-20, finding #12 ("a real,
    // non-blocking cancel affordance for long operations"), Installment C.
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-async.wav";
    writeTestWavFile(wavPath);

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-async.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("exportCancelButton"));
    QVERIFY(cancelButton != nullptr);
    QVERIFY(cancelButton->isHidden());

    const auto exportPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-async.mp4";
    window.exportTopmostLayerVideoAsync(exportPath);

    // Genuinely still running, not already finished - a real background
    // thread, not a disguised synchronous call.
    QVERIFY(window.isExportRunning());
    QVERIFY(!cancelButton->isHidden());

    // Waits for the cancel button to actually hide again, not just for
    // isExportRunning() to flip false - pollExportProgress()'s
    // own ~30fps timer (see its docs) reacts to that up to one tick later,
    // and this test's own qWait()-based poll would otherwise sometimes
    // race ahead of it.
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
    std::filesystem::remove(exportPath);
}

void MainWindowTest::exportTopmostLayerVideoAsyncCompletesSuccessfullyAndHidesTheCancelButton() {
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-async-success.wav";
    writeTestWavFile(wavPath);

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-async-success.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("exportCancelButton"));
    QVERIFY(cancelButton != nullptr);

    const auto exportPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-async-success.mp4";
    window.exportTopmostLayerVideoAsync(exportPath);
    // See exportTopmostLayerVideoAsyncRunsInTheBackgroundAndShowsTheCancelButton()'s
    // own comment on why this waits for the cancel button itself, not just
    // isExportRunning().
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }

    QVERIFY(QFile::exists(QString::fromStdString(exportPath.string())));
    QVERIFY(QFile::exists(QString::fromStdString(exportPath.string())));
    QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("Exported video")));

    std::filesystem::remove(exportPath);
}

void MainWindowTest::cancelVideoExportStopsItAndDeletesThePartialFile() {
    // A several-second clip, not writeTestWavFile()'s own ~0.0001s one -
    // enough real frames (see exportVideo()'s own per-frame shouldCancel()
    // check) that requestCancel(), called immediately after starting,
    // reliably lands well before the encode would finish naturally.
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-cancel.wav";
    writeTestWavFileWithFrameCount(wavPath, 441000, 44100);  // 10 seconds.

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-cancel.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("exportCancelButton"));
    QVERIFY(cancelButton != nullptr);

    const auto exportPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-cancel.mp4";
    window.exportTopmostLayerVideoAsync(exportPath);
    QVERIFY(window.isExportRunning());

    window.cancelExport();
    // See exportTopmostLayerVideoAsyncRunsInTheBackgroundAndShowsTheCancelButton()'s
    // own comment on why this waits for the cancel button itself, not just
    // isExportRunning() - pollExportProgress()'s own file
    // deletion/status message below only happen once that poll actually
    // runs, up to one ~30fps tick after the background thread itself stops.
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }

    QVERIFY(!QFile::exists(QString::fromStdString(exportPath.string())));
    QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("Video export cancelled."));
}

void MainWindowTest::exportTopmostLayerVideoAsyncDoesNothingWhileAlreadyRunning() {
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-already-running.wav";
    writeTestWavFileWithFrameCount(wavPath, 441000, 44100);  // 10 seconds - stays running for this whole test.

    const auto projectPath =
        std::filesystem::temp_directory_path() / "sound-mind-test-export-already-running.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("exportCancelButton"));
    QVERIFY(cancelButton != nullptr);

    const auto firstPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-first.mp4";
    const auto secondPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-second.mp4";
    window.exportTopmostLayerVideoAsync(firstPath);
    QVERIFY(window.isExportRunning());

    // A second call while the first is still running must not destroy (and
    // therefore block on joining) the still-running task - see
    // exportTopmostLayerVideoAsync()'s own docs.
    window.exportTopmostLayerVideoAsync(secondPath);
    QVERIFY(window.isExportRunning());
    QVERIFY(!QFile::exists(QString::fromStdString(secondPath.string())));

    window.cancelExport();
    // See exportTopmostLayerVideoAsyncRunsInTheBackgroundAndShowsTheCancelButton()'s
    // own comment on why this waits for the cancel button itself, not just
    // isExportRunning().
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
    std::filesystem::remove(firstPath);
}

void MainWindowTest::exportTopmostLayerAudioAsyncCompletesSuccessfullyAndHidesTheCancelButton() {
    // Real-world testing pass, 2026-09-20, finding #12 ("a real,
    // non-blocking cancel affordance for long operations"), Installment E -
    // exportTopmostLayerAudioAsync() mirrors exportTopmostLayerVideoAsync()'s
    // own shape exactly (see its own tests above), so this only re-checks
    // the pieces genuinely specific to audio: the format parameter actually
    // reaches the encoder, and the status bar message says "audio".
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-audio-async.wav";
    writeTestWavFile(wavPath);

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-audio-async.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("exportCancelButton"));
    QVERIFY(cancelButton != nullptr);

    const auto exportPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-audio-async.flac";
    window.exportTopmostLayerAudioAsync(exportPath, CompressedAudioFormat::Flac);
    QVERIFY(window.isExportRunning());
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }

    QVERIFY(QFile::exists(QString::fromStdString(exportPath.string())));
    QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("Exported audio")));

    std::filesystem::remove(exportPath);
}

void MainWindowTest::cancelAudioExportStopsItAndDeletesThePartialFile() {
    // A several-second clip - see cancelVideoExportStopsItAndDeletesThePartialFile()'s
    // own comment on why this is long enough for a reliable mid-encode
    // cancel, using Flac specifically since writeViaJuceFormat()'s own
    // chunked rewrite (Decision #137) is what actually gives it a
    // checkpoint to cancel between.
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-audio-cancel.wav";
    writeTestWavFileWithFrameCount(wavPath, 441000, 44100);  // 10 seconds.

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-audio-cancel.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("exportCancelButton"));
    QVERIFY(cancelButton != nullptr);

    const auto exportPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-audio-cancel.flac";
    window.exportTopmostLayerAudioAsync(exportPath, CompressedAudioFormat::Flac);
    QVERIFY(window.isExportRunning());

    window.cancelExport();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }

    QVERIFY(!QFile::exists(QString::fromStdString(exportPath.string())));
    QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("Audio export cancelled."));
}

void MainWindowTest::exportTopmostLayerAudioAsyncDoesNothingWhileAVideoExportIsRunning() {
    // Confirms video/audio async export genuinely share one slot (see
    // exportTopmostLayerAudioAsync()'s own docs), not just that each
    // individually refuses a second call of its own same kind.
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-mixed-kind.wav";
    writeTestWavFileWithFrameCount(wavPath, 441000, 44100);  // 10 seconds - stays running for this whole test.

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-mixed-kind.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    QVERIFY(window.importAudioFile(wavPath));
    std::filesystem::remove(wavPath);

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("exportCancelButton"));
    QVERIFY(cancelButton != nullptr);

    const auto videoPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-mixed-kind.mp4";
    const auto audioPath = std::filesystem::temp_directory_path() / "sound-mind-test-export-mixed-kind.flac";
    window.exportTopmostLayerVideoAsync(videoPath);
    QVERIFY(window.isExportRunning());

    window.exportTopmostLayerAudioAsync(audioPath, CompressedAudioFormat::Flac);
    QVERIFY(window.isExportRunning());
    QVERIFY(!QFile::exists(QString::fromStdString(audioPath.string())));

    window.cancelExport();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
    std::filesystem::remove(videoPath);
}

void MainWindowTest::importAudioFileShowsProgressThenCompletionInTheStatusBar() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-status-import.wav";
    writeTestWavFile(path);

    TestMainWindow window;
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

    TestMainWindow window;
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

    TestMainWindow window;
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
    TestMainWindow window;
    createFreshTestProject(window);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-status-fail.flac";

    QVERIFY(!window.exportTopmostLayerAudioNow(path));
    QVERIFY(window.statusBar()->currentMessage().isEmpty());
}

void MainWindowTest::toggleLoopModeAddsALayerAndStartsTheEngine() {
    TestMainWindow window;
    createFreshTestProject(window);
    const std::size_t layerCountBefore = window.project()->layers().size();

    window.toggleLoopMode();

    QVERIFY(window.isLoopModeRunning());
    QCOMPARE(window.project()->layers().size(), layerCountBefore + 1);
    QCOMPARE(QString::fromStdString(topmostNonEqualizerLayer(*window.project()).name()), QStringLiteral("Loop Input"));

    window.toggleLoopMode();  // cleanup - stop before the window is destroyed.
}

void MainWindowTest::toggleLoopModeStopsARunningCapture() {
    TestMainWindow window;
    createFreshTestProject(window);
    window.toggleLoopMode();
    QVERIFY(window.isLoopModeRunning());

    window.toggleLoopMode();

    QVERIFY(!window.isLoopModeRunning());
}

void MainWindowTest::startPlaybackDoesNothingWhileLoopModeIsRunning() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-loop-playback-guard.wav";
    writeTestWavFile(path);

    TestMainWindow window;
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
    TestMainWindow window;
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

    TestMainWindow window;
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
    TestMainWindow window;
    createFreshTestProject(window);
    window.toggleRecording();
    QVERIFY(window.isRecording());

    window.toggleLoopMode();

    QVERIFY(!window.isLoopModeRunning());
    window.toggleRecording();  // cleanup.
}

void MainWindowTest::toggleRecordingDoesNothingWhileLoopModeIsRunning() {
    TestMainWindow window;
    createFreshTestProject(window);
    window.toggleLoopMode();
    QVERIFY(window.isLoopModeRunning());

    window.toggleRecording();

    QVERIFY(!window.isRecording());
    window.toggleLoopMode();  // cleanup.
}

void MainWindowTest::newProjectStartsWithNoUnsavedChanges() {
    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(!window.hasUnsavedChanges());
}

void MainWindowTest::importAudioFileMarksUnsavedChanges() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-lifecycle-import.wav";
    writeTestWavFile(path);

    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(!window.hasUnsavedChanges());

    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::poolTopmostLayerNowMarksUnsavedChanges() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-lifecycle-pool.wav";
    writeTestWavFile(path);

    TestMainWindow window;
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
    TestMainWindow window;
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

    TestMainWindow window;
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
        TestMainWindow writer;
        createFreshTestProject(writer);
        const_cast<sound_mind::core::Project*>(writer.project())->save(projectPath);
    }

    TestMainWindow window;
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
    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(!window.hasUnsavedChanges());

    QVERIFY(window.close());
}

void MainWindowTest::closeRefusesWhileLoopModeIsRunning() {
    TestMainWindow window;
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
    TestMainWindow window;
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
    TestMainWindow window;
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
    TestMainWindow window;
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

    TestMainWindow window;
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

    TestMainWindow window;
    QVERIFY(window.createProjectAt(settings, path));

    QCOMPARE(window.project()->settings().sampleRateHz, settings.sampleRateHz);
    QCOMPARE(window.project()->settings().binCount, settings.binCount);

    std::filesystem::remove(path);
}

void MainWindowTest::createProjectAtFailsGracefullyForAnUnwritableLocation() {
    // A directory that doesn't exist - Project::save() can't create it.
    const auto path =
        std::filesystem::temp_directory_path() / "sound-mind-test-nonexistent-dir" / "project.smproj";

    TestMainWindow window;
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

    TestMainWindow window;
    QVERIFY(window.createProjectAt(settings, projectPath));

    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QCOMPARE(topmostNonEqualizerLayer(*window.project()).content()->config.binCount, static_cast<std::uint32_t>(128));
}

void MainWindowTest::layersPanelIsHiddenUntilAProjectExists() {
    // isHidden(), not isVisible() - see LayersPanel's own tests for why
    // (the dialog/window chain is never actually shown in this headless
    // test, but hide()/show() still set each widget's own explicit flag).
    TestMainWindow window;
    auto* panel = window.findChild<LayersPanel*>();
    QVERIFY(panel != nullptr);
    QVERIFY(panel->isHidden());

    createFreshTestProject(window);

    QVERIFY(!panel->isHidden());
}

void MainWindowTest::refreshLayersPanelReflectsTheCurrentLayers() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-layers-refresh.wav";
    writeTestWavFile(path);

    TestMainWindow window;
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
    QCOMPARE(panel->findChildren<QLabel*>(QStringLiteral("nameLabel")).size(), 3);  // Background + Equalizer + imported.
}

void MainWindowTest::cycleLayerVisibilityStateEventuallyHidesALayerFromTopmostLookup() {
    // v0.Y.46.1 Installment B - cycleLayerVisibilityState() replaced the
    // old plain on/off toggleLayerVisibility()/visibilityToggled() pair.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-layers-visibility.wav";
    writeTestWavFile(path);

    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);
    const auto layerId = topmostNonEqualizerLayer(*window.project()).id();

    // poolTopmostLayerNow() needs a topmost layer with content - a clean,
    // already-established way to observe topmostLayerWithContent()
    // (private) skipping a hidden layer without exposing it directly.
    QVERIFY(window.poolTopmostLayerNow());

    window.cycleLayerVisibilityState(layerId);  // Visible -> Muted - still has content, still poolable.
    QVERIFY(window.poolTopmostLayerNow());

    window.cycleLayerVisibilityState(layerId);  // Muted -> Invisible - now skipped.
    QVERIFY(!window.poolTopmostLayerNow());
}

void MainWindowTest::cycleLayerVisibilityStateMarksUnsavedChanges() {
    TestMainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();
    QVERIFY(!window.hasUnsavedChanges());

    window.cycleLayerVisibilityState(backgroundId);

    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::setLayerOpacityChangesTheLayersOpacity() {
    TestMainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();

    window.setLayerOpacity(backgroundId, 0.5f);

    QCOMPARE(window.project()->layers().front().opacity(), 0.5f);
    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::setLayerTranslationChangesTheLayersTranslation() {
    TestMainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();

    window.setLayerTranslation(backgroundId, 150);

    QCOMPARE(window.project()->layers().front().translationColumns(), static_cast<std::int64_t>(150));
    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::setLayerRescaleChangesTheLayersRescale() {
    TestMainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();

    window.setLayerRescale(backgroundId, 2.0);

    QCOMPARE(window.project()->layers().front().rescaleFactor(), 2.0);
    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::renameLayerToRenamesTheLayer() {
    TestMainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();

    const bool ok = window.renameLayerTo(backgroundId, QStringLiteral("Floor"));

    QVERIFY(ok);
    QCOMPARE(QString::fromStdString(window.project()->layers().front().name()), QStringLiteral("Floor"));
    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::renameLayerToFailsForAnEmptyName() {
    TestMainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();

    const bool ok = window.renameLayerTo(backgroundId, QString());

    QVERIFY(!ok);
    QCOMPARE(QString::fromStdString(window.project()->layers().front().name()), QStringLiteral("Background"));
}

void MainWindowTest::renameLayerToFailsForAnUnknownId() {
    TestMainWindow window;
    createFreshTestProject(window);

    QVERIFY(!window.renameLayerTo(999999, QStringLiteral("Nope")));
}

void MainWindowTest::deleteLayerRemovesANormalLayer() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-layers-delete.wav";
    writeTestWavFile(path);

    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);
    const auto layerId = topmostNonEqualizerLayer(*window.project()).id();
    const std::size_t countBefore = window.project()->layers().size();

    window.deleteLayer(layerId);

    QCOMPARE(window.project()->layers().size(), countBefore - 1);
}

void MainWindowTest::deleteLayerRefusesToDeleteTheBackgroundLayer() {
    TestMainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();
    const std::size_t countBefore = window.project()->layers().size();

    window.deleteLayer(backgroundId);

    QCOMPARE(window.project()->layers().size(), countBefore);
}

void MainWindowTest::reorderLayersAppliesAValidPermutation() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-layers-reorder.wav";
    writeTestWavFile(path);

    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);
    const auto backgroundId = window.project()->layers().at(0).id();
    const auto importedId = window.project()->layers().at(1).id();
    const auto equalizerId = window.project()->layers().at(2).id();

    // Every layer, Equalizer included, must appear exactly once -
    // Project::reorderLayers() has no Equalizer-position enforcement of
    // its own (that's a UI-level rule - see its own docs).
    window.reorderLayers({importedId, backgroundId, equalizerId});

    QCOMPARE(window.project()->layers().at(0).id(), importedId);
    QCOMPARE(window.project()->layers().at(1).id(), backgroundId);
    QCOMPARE(window.project()->layers().at(2).id(), equalizerId);
    QVERIFY(window.hasUnsavedChanges());
}

void MainWindowTest::reorderLayersRejectsAnInvalidPermutation() {
    TestMainWindow window;
    createFreshTestProject(window);
    const auto backgroundId = window.project()->layers().front().id();

    window.reorderLayers({backgroundId, 999999});  // not a valid permutation.

    QCOMPARE(window.project()->layers().size(), static_cast<std::size_t>(2));  // Background + Equalizer, unchanged.
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
    //
    // A fresh project's Background layer (per Decisions Made #30) has no
    // opacity slider at all any more - a real Normal layer, imported
    // here, is what this test actually needs a slider from.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-opacity-slider-crash.wav";
    writeTestWavFile(path);
    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    auto* panel = window.findChild<LayersPanel*>();
    QVERIFY(panel != nullptr);
    // setLayers() deletes old rows via deleteLater() (see its own docs) -
    // without waiting a tick, createFreshTestProject()'s own now-stale
    // Equalizer row (with its own now-orphaned slider) would still be
    // findable alongside the current one - see
    // refreshLayersPanelReflectsTheCurrentLayers()'s own comment for the
    // identical timing issue.
    QTest::qWait(0);
    // The opacity slider is one of the Layers Panel Redesign's own
    // (v0.Y.44.1) selection-revealed controls - select the imported
    // layer's own row first, matching how a real user would actually
    // reach its slider.
    const auto importedLayerId = topmostNonEqualizerLayer(*window.project()).id();
    panel->selectLayer(importedLayerId);
    QTest::qWait(0);  // let selectLayer()'s own rebuildRows() finish deleteLater()-ing the stale rows too.

    const auto sliders = panel->findChildren<QSlider*>(QStringLiteral("opacitySlider"));
    QCOMPARE(sliders.size(), 1);  // only the selected row's own.
    QSlider* slider = sliders.at(0);

    slider->setValue(42);  // Emits valueChanged() for real, synchronously.

    // If this line is reached at all, the process didn't crash. The
    // topmost imported layer is the topmost non-Equalizer one - the
    // Background layer at the very bottom has no opacity slider to have
    // driven in the first place.
    QCOMPARE(topmostNonEqualizerLayer(*window.project()).opacity(), 0.42f);
}

void MainWindowTest::toggleLoopModeDoesNothingWithNoProjectOpen() {
    // loopEngine_ doesn't exist until setProject() has been called at
    // least once (see the class docs' v0.Y.12.1 note) - toggling before
    // that must be a plain no-op, not a null-dereference crash.
    TestMainWindow window;
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
    TestMainWindow window;
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
    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(!window.keepLooping());

    window.setKeepLooping(true);
    QVERIFY(window.keepLooping());

    window.setKeepLooping(false);
    QVERIFY(!window.keepLooping());
}

void MainWindowTest::setKeepLoopingDoesNothingWithNoProjectOpen() {
    TestMainWindow window;
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
    TestMainWindow window;
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
    TestMainWindow window;
    createFreshTestProject(window);

    window.toggleLoopMode();
    QVERIFY(window.isLoopModeRunning());

    const sound_mind::core::Layer& loopLayer = topmostNonEqualizerLayer(*window.project());
    QCOMPARE(QString::fromStdString(loopLayer.name()), QStringLiteral("Loop Input"));
    QVERIFY(loopLayer.content().has_value());
    QVERIFY(loopLayer.content()->frameCount > 0);

    window.toggleLoopMode();  // cleanup.
}

void MainWindowTest::configuredDeviceAndGainPersistAcrossANewProjectsFreshLoopEngine() {
    // Regression test - real-world testing pass, 2026-09-20, finding #10
    // ("no audio is heard" during Loop Mode): unlike recordEngine_ (a
    // persistent member, never recreated), setProject() rebuilds
    // loopEngine_ fresh for every new/opened project - previously without
    // re-applying whatever input/output device and gain were already
    // configured, so a project switch silently reset Loop Mode's own
    // engine back to system-default devices at unity gain, regardless of
    // what Configure Devices actually showed as selected.
    TestMainWindow window;
    createFreshTestProject(window);

    window.setConfiguredInputDevice(QStringLiteral("Some Microphone"));
    window.setConfiguredOutputDevice(QStringLiteral("Some Speakers"));
    window.setConfiguredInputGain(150);
    QCOMPARE(window.loopInputDevice(), QStringLiteral("Some Microphone"));
    QCOMPARE(window.loopOutputDevice(), QStringLiteral("Some Speakers"));
    QCOMPARE(window.loopInputGain(), 1.5f);

    createFreshTestProject(window);  // reconstructs loopEngine_.

    QCOMPARE(window.loopInputDevice(), QStringLiteral("Some Microphone"));
    QCOMPARE(window.loopOutputDevice(), QStringLiteral("Some Speakers"));
    QCOMPARE(window.loopInputGain(), 1.5f);
}

void MainWindowTest::theLoopInputLayersNameIsActuallyVisibleInTheLayersPanel() {
    // Regression test - real-world testing pass, 2026-09-20, finding #10's
    // other symptom ("the Loop Input layer never actually appears in the
    // Layers panel"). Every prior test covering this layer only checked
    // window.project()->layers() (the model) - never the actual
    // LayersPanel widget. At the time this was reported, a layer with any
    // real thumbnail content (which a freshly-seeded "Loop Input" layer
    // always has - see toggleLoopModeGivesANewLoopInputLayerAPlaceholderContentImmediately())
    // had its own name label hidden behind that thumbnail (Decision #125's
    // QStackedLayout bug, fixed in v0.0.45.4) - indistinguishable from "not
    // there at all" without recognizing a near-silent spectrogram thumbnail
    // for what it is. This locks in that the fix actually covers this case
    // too, not just the cases findings #1/#2/#3 originally reported.
    TestMainWindow window;
    createFreshTestProject(window);

    window.toggleLoopMode();
    QVERIFY(window.isLoopModeRunning());

    auto* panel = window.findChild<LayersPanel*>();
    QVERIFY(panel != nullptr);
    QTest::qWait(0);  // see refreshLayersPanelReflectsTheCurrentLayers()'s own comment on why.
    const auto nameLabels = panel->findChildren<QLabel*>(QStringLiteral("nameLabel"));
    bool foundLoopInput = false;
    for (const auto* label : nameLabels) {
        if (label->text() == QStringLiteral("Loop Input")) {
            foundLoopInput = true;
            break;
        }
    }
    QVERIFY(foundLoopInput);

    window.toggleLoopMode();  // cleanup.
}

void MainWindowTest::transportPanelsStayHiddenByDefaultEvenAfterAProjectExists() {
    // Unlike the Layers panel (shown automatically the first time a
    // project exists - see layersPanelIsHiddenUntilAProjectExists()),
    // Playback/Record/Loop start OFF and stay OFF until the user
    // explicitly toggles one on, confirmed with the user.
    TestMainWindow window;
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
    TestMainWindow window;
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
    TestMainWindow window;
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
    TestMainWindow window;
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
    TestMainWindow window;
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
    TestMainWindow window;
    createFreshTestProject(window);
    auto* checkBox = window.findChild<QCheckBox*>(QStringLiteral("keepLoopingCheckBox"));
    QVERIFY(checkBox != nullptr);

    window.setKeepLooping(true);
    QVERIFY(checkBox->isChecked());

    window.setKeepLooping(false);
    QVERIFY(!checkBox->isChecked());
}

void MainWindowTest::setConfiguredInputDeviceForwardsToTheLoopEngine() {
    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.loopInputDevice().isEmpty());

    window.setConfiguredInputDevice(QStringLiteral("Some Microphone"));
    QCOMPARE(window.loopInputDevice(), QStringLiteral("Some Microphone"));
}

void MainWindowTest::setConfiguredOutputDeviceForwardsToTheLoopEngine() {
    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.loopOutputDevice().isEmpty());

    window.setConfiguredOutputDevice(QStringLiteral("Some Speakers"));
    QCOMPARE(window.loopOutputDevice(), QStringLiteral("Some Speakers"));
}

void MainWindowTest::setConfiguredInputDeviceForwardsToTheRecordEngine() {
    TestMainWindow window;
    QVERIFY(window.recordInputDevice().isEmpty());

    window.setConfiguredInputDevice(QStringLiteral("Some Microphone"));
    QCOMPARE(window.recordInputDevice(), QStringLiteral("Some Microphone"));
}

void MainWindowTest::setPlaybackVolumeForwardsToThePlaybackEngine() {
    TestMainWindow window;
    QCOMPARE(window.playbackVolume(), 1.0f);

    window.setPlaybackVolume(150);
    QCOMPARE(window.playbackVolume(), 1.5f);
}

void MainWindowTest::loopPanelToggleButtonStartsAndStopsTheRealEngine() {
    // End-to-end wiring check, driving the real embedded button rather
    // than calling toggleLoopMode() directly.
    TestMainWindow window;
    createFreshTestProject(window);
    auto* button = window.findChild<QPushButton*>(QStringLiteral("loopToggleButton"));
    QVERIFY(button != nullptr);

    button->click();
    QVERIFY(window.isLoopModeRunning());

    button->click();
    QVERIFY(!window.isLoopModeRunning());
}

void MainWindowTest::loopModeLocksAndUnlocksConfigureDevicesDeviceCombos() {
    // Real-world testing pass, 2026-09-20, finding #7: Configure Devices'
    // own combos must preserve the "locked while running" safety behavior
    // LoopPanel's own now-removed device pickers used to provide locally.
    TestMainWindow window;
    createFreshTestProject(window);
    auto* button = window.findChild<QPushButton*>(QStringLiteral("loopToggleButton"));
    auto* inputCombo = window.findChild<QComboBox*>(QStringLiteral("inputDeviceCombo"));
    auto* outputCombo = window.findChild<QComboBox*>(QStringLiteral("outputDeviceCombo"));
    QVERIFY(button != nullptr);
    QVERIFY(inputCombo != nullptr);
    QVERIFY(outputCombo != nullptr);
    QVERIFY(inputCombo->isEnabled());
    QVERIFY(outputCombo->isEnabled());

    button->click();
    QVERIFY(!inputCombo->isEnabled());
    QVERIFY(!outputCombo->isEnabled());

    button->click();
    QVERIFY(inputCombo->isEnabled());
    QVERIFY(outputCombo->isEnabled());
}

void MainWindowTest::recordingLocksTheInputComboButNotTheOutputCombo() {
    // Recording never touches the output device, unlike Loop Mode - see
    // updateConfiguredDeviceLockState()'s own docs.
    TestMainWindow window;
    createFreshTestProject(window);
    auto* button = window.findChild<QPushButton*>(QStringLiteral("recordToggleButton"));
    auto* inputCombo = window.findChild<QComboBox*>(QStringLiteral("inputDeviceCombo"));
    auto* outputCombo = window.findChild<QComboBox*>(QStringLiteral("outputDeviceCombo"));
    QVERIFY(button != nullptr);
    QVERIFY(inputCombo != nullptr);
    QVERIFY(outputCombo != nullptr);

    button->click();
    QVERIFY(!inputCombo->isEnabled());
    QVERIFY(outputCombo->isEnabled());

    button->click();
    QVERIFY(inputCombo->isEnabled());
}

void MainWindowTest::playbackPanelButtonsDriveRealPlayback() {
    // End-to-end wiring check, driving the real embedded Play/Stop buttons
    // rather than calling startPlayback()/stopPlayback() directly.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-playback-panel-buttons.wav";
    writeTestWavFile(path);

    // See startPlaybackPlaysAnImportedLayer()'s own comment on why a
    // small canvasWidth, not createFreshTestProject()'s default.
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-playback-panel-buttons.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    auto* playButton = window.findChild<QPushButton*>(QStringLiteral("playButton"));
    auto* stopButton = window.findChild<QPushButton*>(QStringLiteral("stopButton"));
    QVERIFY(playButton != nullptr);
    QVERIFY(stopButton != nullptr);

    // See startPlaybackPlaysAnImportedLayer()'s own comment on why this
    // waits for the background composite to finish.
    playButton->click();
    auto* compositeCancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(compositeCancelButton != nullptr);
    while (!compositeCancelButton->isHidden()) {
        QTest::qWait(5);
    }
    QVERIFY(window.isPlaying());

    stopButton->click();
    QVERIFY(!window.isPlaying());
}

void MainWindowTest::audioSnippetsForFileReturnsOneSnippetForAudioNoLongerThanTheProject() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-snippets-short.wav";
    writeTestWavFile(path);  // 4 samples - far shorter than any project's own duration.

    TestMainWindow window;
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
    TestMainWindow window;
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
    TestMainWindow window;
    const auto snippets =
        window.audioSnippetsForFile(std::filesystem::temp_directory_path() / "sound-mind-does-not-exist.wav");
    QVERIFY(snippets.empty());
}

void MainWindowTest::importAudioFileImportsEveryComputedSnippetForLongAudio() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-snippets-all.wav";
    constexpr std::size_t loopLengthSamples = 3528;
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 3);  // exactly 3 whole snippets.

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-import-snippets-all.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    const bool ok = window.importAudioFile(path);
    const std::string stem = path.stem().string();
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    QCOMPARE(window.project()->layers().size(), layerCountBefore + 3);
    // Indexed from layerCountBefore - 1, not layerCountBefore - every
    // import inserts just below the Equalizer (addLayer()'s own docs),
    // so the newly imported layers start one slot earlier than the old
    // Equalizer-less stack would have put them.
    QCOMPARE(QString::fromStdString(window.project()->layers()[layerCountBefore - 1].name()),
              QString::fromStdString(stem + "_0000"));
    QCOMPARE(QString::fromStdString(window.project()->layers()[layerCountBefore].name()),
              QString::fromStdString(stem + "_0001"));
    QCOMPARE(QString::fromStdString(window.project()->layers()[layerCountBefore + 1].name()),
              QString::fromStdString(stem + "_0002"));
}

void MainWindowTest::importAudioSnippetsImportsOnlyTheRequestedSubset() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-snippets-subset.wav";
    constexpr std::size_t loopLengthSamples = 3528;
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 4);  // 4 whole snippets: 0, 1, 2, 3.

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-import-snippets-subset.smproj";
    TestMainWindow window;
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
    // See importAudioFileImportsEveryComputedSnippetForLongAudio()'s own comment.
    QCOMPARE(QString::fromStdString(window.project()->layers()[layerCountBefore - 1].name()),
              QString::fromStdString(stem + "_0000"));
    QCOMPARE(QString::fromStdString(window.project()->layers()[layerCountBefore].name()),
              QString::fromStdString(stem + "_0002"));
}

void MainWindowTest::importAudioSnippetsSkipsOutOfRangeIndicesGracefully() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-snippets-range.wav";
    constexpr std::size_t loopLengthSamples = 3528;
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 2);  // exactly 2 snippets: 0, 1.

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-import-snippets-range.smproj";
    TestMainWindow window;
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

    TestMainWindow window;
    createFreshTestProject(window);
    const std::size_t layerCountBefore = window.project()->layers().size();

    const bool ok = window.importAudioSnippets(path, {});  // nothing requested.
    std::filesystem::remove(path);

    QVERIFY(!ok);
    QCOMPARE(window.project()->layers().size(), layerCountBefore);
}

void MainWindowTest::importAudioSnippetsAsyncRunsInTheBackgroundAndShowsTheCancelButton() {
    // Real-world testing pass, 2026-09-20, finding #12 ("a real,
    // non-blocking cancel affordance for long operations"), Installment F.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-async.wav";
    constexpr std::size_t loopLengthSamples = 3528;
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 2);  // 2 whole snippets.

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-import-async.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("importCancelButton"));
    QVERIFY(cancelButton != nullptr);
    QVERIFY(cancelButton->isHidden());

    window.importAudioSnippetsAsync(path, {0, 1});

    QVERIFY(window.isImportRunning());
    QVERIFY(!cancelButton->isHidden());

    // See exportTopmostLayerVideoAsyncRunsInTheBackgroundAndShowsTheCancelButton()'s
    // own comment on why this waits for the cancel button itself, not just
    // isImportRunning().
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);
}

void MainWindowTest::importAudioSnippetsAsyncCompletesSuccessfullyAndAddsTheLayers() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-async-success.wav";
    constexpr std::size_t loopLengthSamples = 3528;
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 2);  // 2 whole snippets.

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-import-async-success.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("importCancelButton"));
    QVERIFY(cancelButton != nullptr);

    window.importAudioSnippetsAsync(path, {0, 1});
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }

    QCOMPARE(window.project()->layers().size(), layerCountBefore + 2);
    QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("Imported")));

    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);
}

void MainWindowTest::cancelImportDiscardsTheEncodedLayersWithoutAddingThem() {
    // Several whole snippets - see cancelVideoExportStopsItAndDeletesThePartialFile()'s
    // own comment on why this many real checkpoints (one per snippet - see
    // encodeAudioSnippets()'s own docs) makes an immediate-after-start
    // cancel() reliably land mid-encode.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-cancel.wav";
    constexpr std::size_t loopLengthSamples = 3528;
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 10);  // 10 whole snippets.

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-import-cancel.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("importCancelButton"));
    QVERIFY(cancelButton != nullptr);

    std::vector<std::size_t> allIndices;
    for (std::size_t i = 0; i < 10; ++i) {
        allIndices.push_back(i);
    }
    window.importAudioSnippetsAsync(path, allIndices);
    QVERIFY(window.isImportRunning());

    window.cancelImport();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }

    // The whole point of finding #12's own "rolls back" requirement:
    // nothing partially encoded ever reached the project.
    QCOMPARE(window.project()->layers().size(), layerCountBefore);
    QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("Audio import cancelled."));

    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);
}

void MainWindowTest::importAudioSnippetsAsyncDoesNothingWhileAlreadyRunning() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-already-running.wav";
    constexpr std::size_t loopLengthSamples = 3528;
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 10);  // stays running for this whole test.

    const auto projectPath =
        std::filesystem::temp_directory_path() / "sound-mind-test-import-already-running.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("importCancelButton"));
    QVERIFY(cancelButton != nullptr);

    std::vector<std::size_t> allIndices;
    for (std::size_t i = 0; i < 10; ++i) {
        allIndices.push_back(i);
    }
    window.importAudioSnippetsAsync(path, allIndices);
    QVERIFY(window.isImportRunning());

    // A second call while the first is still running must not destroy (and
    // therefore block on joining) the still-running task.
    window.importAudioSnippetsAsync(path, {0});
    QVERIFY(window.isImportRunning());

    window.cancelImport();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
    QCOMPARE(window.project()->layers().size(), layerCountBefore);

    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);
}

void MainWindowTest::closeRefusesWhileAnImportIsRunning() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-refuse-close.wav";
    constexpr std::size_t loopLengthSamples = 3528;
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 10);

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-import-refuse-close.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("importCancelButton"));
    QVERIFY(cancelButton != nullptr);

    std::vector<std::size_t> allIndices;
    for (std::size_t i = 0; i < 10; ++i) {
        allIndices.push_back(i);
    }
    window.importAudioSnippetsAsync(path, allIndices);
    QVERIFY(window.isImportRunning());

    // Refused outright (no dialog reached - see closeEvent()'s docs).
    QVERIFY(!window.close());
    QVERIFY(window.isImportRunning());

    window.cancelImport();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);
}

void MainWindowTest::newProjectRefusesWhileAnImportIsRunning() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-refuse-new.wav";
    constexpr std::size_t loopLengthSamples = 3528;
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 10);

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-import-refuse-new.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("importCancelButton"));
    QVERIFY(cancelButton != nullptr);

    std::vector<std::size_t> allIndices;
    for (std::size_t i = 0; i < 10; ++i) {
        allIndices.push_back(i);
    }
    window.importAudioSnippetsAsync(path, allIndices);
    QVERIFY(window.isImportRunning());
    const std::size_t layerCountBefore = window.project()->layers().size();

    // The real, interactive newProject() - safe to call directly, since
    // the import check runs before the wizard would ever be shown.
    window.newProject();

    QVERIFY(window.isImportRunning());
    QCOMPARE(window.project()->layers().size(), layerCountBefore);

    window.cancelImport();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);
}

void MainWindowTest::openProjectRefusesWhileAnImportIsRunning() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-refuse-open.wav";
    constexpr std::size_t loopLengthSamples = 3528;
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 10);

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-import-refuse-open.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("importCancelButton"));
    QVERIFY(cancelButton != nullptr);

    std::vector<std::size_t> allIndices;
    for (std::size_t i = 0; i < 10; ++i) {
        allIndices.push_back(i);
    }
    window.importAudioSnippetsAsync(path, allIndices);
    QVERIFY(window.isImportRunning());

    // Refused before ever reaching the file dialog.
    window.openProject();
    QVERIFY(window.isImportRunning());

    window.cancelImport();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);
}

void MainWindowTest::openProjectAtRefusesWhileAnImportIsRunning() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-import-refuse-openat.wav";
    constexpr std::size_t loopLengthSamples = 3528;
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 10);

    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-import-refuse-openat.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));

    auto* cancelButton = window.findChild<QPushButton*>(QStringLiteral("importCancelButton"));
    QVERIFY(cancelButton != nullptr);

    std::vector<std::size_t> allIndices;
    for (std::size_t i = 0; i < 10; ++i) {
        allIndices.push_back(i);
    }
    window.importAudioSnippetsAsync(path, allIndices);
    QVERIFY(window.isImportRunning());

    QString errorMessage;
    const bool ok = window.openProjectAt(std::filesystem::temp_directory_path() / "sound-mind-does-not-exist.smproj",
                                          &errorMessage);

    QVERIFY(!ok);
    QVERIFY(!errorMessage.isEmpty());
    QVERIFY(window.isImportRunning());

    window.cancelImport();
    while (!cancelButton->isHidden()) {
        QTest::qWait(5);
    }
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);
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

    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));

    const bool ok = window.importImageFile(path, ImageScalePickerDialog::Mode::RescaleToFitProject);
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    const auto& content = *topmostNonEqualizerLayer(*window.project()).content();
    QCOMPARE(content.frameCount, static_cast<std::uint32_t>(100));    // project's canvasWidth.
    QCOMPARE(content.config.binCount, static_cast<std::uint32_t>(50));  // project's binCount.
}

void MainWindowTest::importImageFileScaleVerticalKeepHorizontalKeepsNativeWidth() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-v.png";
    writeImageScalingTestImage(path);
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-v.smproj";

    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));

    const bool ok = window.importImageFile(path, ImageScalePickerDialog::Mode::ScaleVerticalKeepHorizontal);
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    const auto& content = *topmostNonEqualizerLayer(*window.project()).content();
    QCOMPARE(content.frameCount, static_cast<std::uint32_t>(30));     // the source image's own native width.
    QCOMPARE(content.config.binCount, static_cast<std::uint32_t>(50));  // project's binCount.
}

void MainWindowTest::importImageFileScaleHorizontalKeepVerticalKeepsNativeHeight() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-h.png";
    writeImageScalingTestImage(path);
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-h.smproj";

    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));

    const bool ok = window.importImageFile(path, ImageScalePickerDialog::Mode::ScaleHorizontalKeepVertical);
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    const auto& content = *topmostNonEqualizerLayer(*window.project()).content();
    QCOMPARE(content.frameCount, static_cast<std::uint32_t>(100));    // project's canvasWidth.
    QCOMPARE(content.config.binCount, static_cast<std::uint32_t>(20));  // the source image's own native height.
}

void MainWindowTest::importImageFileScaleVerticalProportionalPreservesAspectRatio() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-proportional.png";
    writeImageScalingTestImage(path);
    const auto projectPath =
        std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-proportional.smproj";

    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));

    const bool ok = window.importImageFile(path, ImageScalePickerDialog::Mode::ScaleVerticalProportional);
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    const auto& content = *topmostNonEqualizerLayer(*window.project()).content();
    QCOMPARE(content.config.binCount, static_cast<std::uint32_t>(50));  // project's binCount.
    QCOMPARE(content.frameCount, static_cast<std::uint32_t>(75));       // 30 * 50 / 20, preserving the 3:2 aspect ratio.
}

void MainWindowTest::importImageFileKeepNativeResolutionDoesNotRescale() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-native.png";
    writeImageScalingTestImage(path);
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-image-scale-native.smproj";

    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));

    const bool ok = window.importImageFile(path, ImageScalePickerDialog::Mode::KeepNativeResolution);
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    const auto& content = *topmostNonEqualizerLayer(*window.project()).content();
    QCOMPARE(content.frameCount, static_cast<std::uint32_t>(30));     // the source image's own native width.
    QCOMPARE(content.config.binCount, static_cast<std::uint32_t>(20));  // the source image's own native height.
}

void MainWindowTest::handleDroppedFilesImportsAWavFile() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-drop.wav";
    writeTestWavFile(path);

    TestMainWindow window;
    createFreshTestProject(window);
    const std::size_t layerCountBefore = window.project()->layers().size();

    window.handleDroppedFiles({path});
    std::filesystem::remove(path);

    QCOMPARE(window.project()->layers().size(), layerCountBefore + 1);
}

void MainWindowTest::handleDroppedFilesImportsAnImageFile() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-drop.png";
    writeImageScalingTestImage(path);

    TestMainWindow window;
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
        TestMainWindow writer;
        QVERIFY(writer.createProjectAt(sound_mind::core::ProjectSettings{}, projectPath));
    }

    TestMainWindow window;
    QVERIFY(window.isShowingLandingPage());

    window.handleDroppedFiles({projectPath});
    std::filesystem::remove(projectPath);

    QVERIFY(!window.isShowingLandingPage());
}

void MainWindowTest::handleDroppedFilesIgnoresUnrecognizedExtensions() {
    TestMainWindow window;
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

    TestMainWindow window;
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
    TestMainWindow window;
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

    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));  // 100x50 canvas.

    window.handleDroppedFiles({path}, ImageScalePickerDialog::Mode::KeepNativeResolution, /*importAsSequence=*/false);
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    const auto& content = *topmostNonEqualizerLayer(*window.project()).content();
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

    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    window.handleDroppedFiles({pathA, pathB}, ImageScalePickerDialog::Mode::RescaleToFitProject,
                               /*importAsSequence=*/true);
    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
    std::filesystem::remove(projectPath);

    QCOMPARE(window.project()->layers().size(), layerCountBefore + 2);
    // Indexed from layerCountBefore - 1 - see
    // importAudioFileImportsEveryComputedSnippetForLongAudio()'s own comment.
    QCOMPARE(window.project()->layers()[layerCountBefore - 1].translationColumns(), static_cast<std::int64_t>(0));
    QCOMPARE(window.project()->layers()[layerCountBefore].translationColumns(), static_cast<std::int64_t>(75));
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

    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    window.handleDroppedFiles({path}, ImageScalePickerDialog::Mode::RescaleToFitProject,
                               /*importAsSequence=*/false, {{path, {0, 2}}});
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QCOMPARE(window.project()->layers().size(), layerCountBefore + 2);  // not all 4.
}

void MainWindowTest::handleDroppedFilesAppliesGivenAudioSnippetOffsets() {
    // Real-world testing pass finding #23 - a separate map from
    // audioSnippetSelections (see handleDroppedFiles()'s own docs), applied
    // alongside it.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-drop-audio-offset.wav";
    constexpr std::size_t loopLengthSamples = 3528;  // 8 * 441 - see smallCanvasProjectSettings()'s docs.
    writeTestWavFileWithFrameCount(path, loopLengthSamples * 4);  // 4 whole snippets at offset 0: 0, 1, 2, 3.
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-drop-audio-offset.smproj";
    const double oneLoopSeconds = static_cast<double>(loopLengthSamples) / 44100.0;

    TestMainWindow window;
    QVERIFY(window.createProjectAt(smallCanvasProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    // Offset by one whole loop length leaves only 3 available (indices
    // 0-2) - requesting all four (0-3) under that offset silently skips
    // the now-out-of-range index 3, proving the offset actually reached
    // the encode step, not just the (never shown, in this headless test)
    // picker dialog.
    window.handleDroppedFiles({path}, ImageScalePickerDialog::Mode::RescaleToFitProject,
                               /*importAsSequence=*/false, {{path, {0, 1, 2, 3}}}, {{path, oneLoopSeconds}});
    std::filesystem::remove(path);
    std::filesystem::remove(projectPath);

    QCOMPARE(window.project()->layers().size(), layerCountBefore + 3);
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

    TestMainWindow window;
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
    // own explicitly-chosen mode (KeepNativeResolution here). Indexed
    // from layerCountBefore - 1 - see
    // importAudioFileImportsEveryComputedSnippetForLongAudio()'s own comment.
    QCOMPARE(window.project()->layers()[layerCountBefore - 1].translationColumns(), static_cast<std::int64_t>(0));
    QCOMPARE(window.project()->layers()[layerCountBefore].translationColumns(), static_cast<std::int64_t>(0));
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

    TestMainWindow window;
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

    // Indexed from layerCountBefore - 1 - see
    // importAudioFileImportsEveryComputedSnippetForLongAudio()'s own comment.
    const auto& layerA = window.project()->layers()[layerCountBefore - 1];
    QCOMPARE(layerA.translationColumns(), static_cast<std::int64_t>(0));
    QCOMPARE(layerA.content()->frameCount, static_cast<std::uint32_t>(75));

    const auto& layerB = window.project()->layers()[layerCountBefore];
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

    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    const std::size_t layerCountBefore = window.project()->layers().size();

    const bool ok = window.importImageFiles({pathA, pathB, pathC}, ImageScalePickerDialog::Mode::RescaleToFitProject,
                                             /*importAsSequence=*/true);
    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
    std::filesystem::remove(pathC);
    std::filesystem::remove(projectPath);

    QVERIFY(ok);
    // Indexed from layerCountBefore - 1 - see
    // importAudioFileImportsEveryComputedSnippetForLongAudio()'s own comment.
    QCOMPARE(window.project()->layers()[layerCountBefore - 1].translationColumns(), static_cast<std::int64_t>(0));
    QCOMPARE(window.project()->layers()[layerCountBefore].translationColumns(), static_cast<std::int64_t>(75));
    QCOMPARE(window.project()->layers()[layerCountBefore + 1].translationColumns(), static_cast<std::int64_t>(0));
}

void MainWindowTest::importImageFilesSortsFilesAlphabeticallyWhenSequential() {
    const auto pathA = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-sort-a.png";
    const auto pathB = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-sort-b.png";
    writeImageScalingTestImage(pathA);
    writeImageScalingTestImage(pathB);
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-imgseq-sort.smproj";

    TestMainWindow window;
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
    // Indexed from layerCountBefore - 1 - see
    // importAudioFileImportsEveryComputedSnippetForLongAudio()'s own comment.
    const auto& firstImported = window.project()->layers()[layerCountBefore - 1];
    const auto& secondImported = window.project()->layers()[layerCountBefore];
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

    TestMainWindow window;
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

    TestMainWindow window;
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
    TestMainWindow window;
    QVERIFY(!window.windowTitle().contains(QStringLiteral(" - ")));  // no project yet.

    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-window-title.smproj";
    QVERIFY(window.createProjectAt(sound_mind::core::ProjectSettings{}, path));

    QVERIFY(window.windowTitle().contains(QStringLiteral("sound-mind-test-window-title")));
}

void MainWindowTest::startPlaybackSetsThePlaybackPanelDuration() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-playback-duration.wav";
    writeTestWavFileWithFrameCount(path, 44100, 44100);  // exactly 1 second.

    // As of v0.Y.27.1 (Multi-layer Compositing): Playback decodes the
    // project's own real composite, which always spans exactly
    // canvasWidth's own duration (padded with silence past whatever real
    // content exists) - not just whichever single layer used to be
    // "topmost". A default-sized (canvasWidth = 1024) project would make
    // this assert against ~10.24s of mostly silence instead of the
    // imported clip's own 1 second, so this test needs its own project
    // sized to actually match - imageScalingTestProjectSettings()'s own
    // canvasWidth (100) times the default 10ms timestep is exactly 1s.
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-playback-duration.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    // See startPlaybackPlaysAnImportedLayer()'s own comment on why this
    // waits for the background composite to finish.
    window.startPlayback();
    auto* compositeCancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(compositeCancelButton != nullptr);
    while (!compositeCancelButton->isHidden()) {
        QTest::qWait(5);
    }

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

    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    // See startPlaybackPlaysAnImportedLayer()'s own comment on why this
    // waits for the background composite to finish.
    window.startPlayback();
    auto* compositeCancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(compositeCancelButton != nullptr);
    while (!compositeCancelButton->isHidden()) {
        QTest::qWait(5);
    }
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

    TestMainWindow window;
    createFreshTestProject(window);
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    // See startPlaybackPlaysAnImportedLayer()'s own comment on why this
    // waits for the background composite to finish.
    window.startPlayback();
    auto* compositeCancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(compositeCancelButton != nullptr);
    while (!compositeCancelButton->isHidden()) {
        QTest::qWait(5);
    }
    window.seekPlayback(5.0);

    window.stopPlayback();

    auto* panel = window.findChild<PlaybackPanel*>();
    QVERIFY(panel != nullptr);
    auto* label = panel->findChild<QLabel*>(QStringLiteral("positionLabel"));
    QVERIFY(label != nullptr);
    QCOMPARE(label->text(), QStringLiteral("0:00 / 0:00"));
}

void MainWindowTest::setPlaybackRepeatAndScopeDoNothingWithNoProjectOpen() {
    TestMainWindow window;
    window.setPlaybackRepeat(true);
    window.setPlaybackScope(PlaybackScope::Delta);
    // If this line is reached at all, neither call crashed with no project
    // open - matching the same no-project-open safety every other
    // engine-adjacent setter in this class already guarantees.
    QVERIFY(!window.isPlaying());
}

void MainWindowTest::paintingWhileRepeatIsOffDoesNotInterruptPlayback() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-repeat-off.wav";
    writeTestWavFile(path);

    // A wider-than-tall canvas (200x50, 10ms/column = 2s total) so a paint
    // click's own column maps to a time distinguishable from a seeked
    // position at the position label's own whole-second resolution - see
    // this test's own assertions below.
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 200;
    settings.canvasHeight = 50;
    settings.binCount = 50;
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-repeat-off.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(settings, projectPath));
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    // See startPlaybackPlaysAnImportedLayer()'s own comment on why this
    // waits for the background composite to finish.
    window.startPlayback();
    auto* compositeCancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(compositeCancelButton != nullptr);
    while (!compositeCancelButton->isHidden()) {
        QTest::qWait(5);
    }
    window.seekPlayback(1.5);  // "0:01 / 0:02".
    QVERIFY(window.isPlaying());

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(200, 50);
    window.setPaintModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));    // ~0.1s - far from 1.5s.
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));

    // Repeat is off by default - the edit shouldn't have reloaded or
    // repositioned playback at all.
    QVERIFY(window.isPlaying());
    auto* panel = window.findChild<PlaybackPanel*>();
    QVERIFY(panel != nullptr);
    auto* label = panel->findChild<QLabel*>(QStringLiteral("positionLabel"));
    QVERIFY(label != nullptr);
    QCOMPARE(label->text(), QStringLiteral("0:01 / 0:02"));

    window.stopPlayback();
}

void MainWindowTest::paintingWhileRepeatIsOnWithDeltaScopeJumpsPlaybackToTheEditedRegion() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-repeat-delta.wav";
    writeTestWavFile(path);

    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 200;
    settings.canvasHeight = 50;
    settings.binCount = 50;
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-repeat-delta.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(settings, projectPath));
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    window.setPlaybackRepeat(true);
    window.setPlaybackScope(PlaybackScope::Delta);
    // See startPlaybackPlaysAnImportedLayer()'s own comment on why this
    // waits for the background composite to finish.
    window.startPlayback();  // starts at "0:00 / 0:02" - never seeked.
    auto* compositeCancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(compositeCancelButton != nullptr);
    while (!compositeCancelButton->isHidden()) {
        QTest::qWait(5);
    }

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(200, 50);
    window.setPaintModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(150, 10));  // ~1.5s.
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(150, 10));

    // Delta scope jumps playback to the just-painted stroke's own start -
    // ~1.5s, not the "0:00" it started at.
    QVERIFY(window.isPlaying());
    auto* panel = window.findChild<PlaybackPanel*>();
    QVERIFY(panel != nullptr);
    auto* label = panel->findChild<QLabel*>(QStringLiteral("positionLabel"));
    QVERIFY(label != nullptr);
    QCOMPARE(label->text(), QStringLiteral("0:01 / 0:02"));

    window.stopPlayback();
}

void MainWindowTest::paintingWhileRepeatIsOnWithTrackScopeKeepsTheSamePosition() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-repeat-track.wav";
    writeTestWavFile(path);

    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 200;
    settings.canvasHeight = 50;
    settings.binCount = 50;
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-repeat-track.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(settings, projectPath));
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    window.setPlaybackRepeat(true);
    window.setPlaybackScope(PlaybackScope::Track);  // the default, set explicitly for clarity.
    // See startPlaybackPlaysAnImportedLayer()'s own comment on why this
    // waits for the background composite to finish.
    window.startPlayback();
    auto* compositeCancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(compositeCancelButton != nullptr);
    while (!compositeCancelButton->isHidden()) {
        QTest::qWait(5);
    }
    window.seekPlayback(1.5);  // "0:01 / 0:02".

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(200, 50);
    window.setPaintModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));  // ~0.1s.
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));

    // Track scope re-renders in place - unlike Delta, it never jumps to
    // the edited region, even with Repeat on.
    QVERIFY(window.isPlaying());
    auto* panel = window.findChild<PlaybackPanel*>();
    QVERIFY(panel != nullptr);
    auto* label = panel->findChild<QLabel*>(QStringLiteral("positionLabel"));
    QVERIFY(label != nullptr);
    QCOMPARE(label->text(), QStringLiteral("0:01 / 0:02"));

    window.stopPlayback();
}

void MainWindowTest::paintingWhileRepeatIsOnWithReviewScopeWrapsToTheTrackStartNotTheEdit() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-repeat-review.wav";
    writeTestWavFile(path);

    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 200;
    settings.canvasHeight = 50;
    settings.binCount = 50;
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-repeat-review.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(settings, projectPath));
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    window.setPlaybackRepeat(true);
    window.setPlaybackScope(PlaybackScope::Review);
    // See startPlaybackPlaysAnImportedLayer()'s own comment on why this
    // waits for the background composite to finish.
    window.startPlayback();  // starts at "0:00 / 0:02" - never seeked.
    auto* compositeCancelButton = window.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"));
    QVERIFY(compositeCancelButton != nullptr);
    while (!compositeCancelButton->isHidden()) {
        QTest::qWait(5);
    }

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(200, 50);
    window.setPaintModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(150, 10));  // ~1.5s.
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(150, 10));

    // Review jumps to the edit's own start, same as Delta - confirmed
    // first, since the wrap-at-end behavior below only matters once this
    // part still holds.
    auto* panel = window.findChild<PlaybackPanel*>();
    QVERIFY(panel != nullptr);
    auto* label = panel->findChild<QLabel*>(QStringLiteral("positionLabel"));
    QVERIFY(label != nullptr);
    QCOMPARE(label->text(), QStringLiteral("0:01 / 0:02"));

    // Reaching the end of the track (unlike Delta, Review's own range runs
    // to the whole track's end, not just the edited region's) should wrap
    // back to the start of the whole track - see docs/sound-mind-design.md's
    // "Repeat Playback" section - not back to the edit's own start.
    window.seekPlayback(2.0);
    QCOMPARE(label->text(), QStringLiteral("0:00 / 0:02"));

    window.stopPlayback();
}

void MainWindowTest::paintingWithRepeatOffAndDeltaScopeStartsAOneShotPlaybackAutomatically() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-oneshot-delta.wav";
    writeTestWavFile(path);

    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 200;
    settings.canvasHeight = 50;
    settings.binCount = 50;
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-oneshot-delta.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(settings, projectPath));
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    // Repeat is off (the default) and Play was never pressed - per
    // docs/sound-mind-design.md's "Repeat Playback" section, Delta/Review's
    // own "restarts audio playback... the moment the canvas is updated" is
    // driven by Scope alone, not gated behind Repeat or an already-playing
    // session.
    window.setPlaybackScope(PlaybackScope::Delta);
    QVERIFY(!window.isPlaying());

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(200, 50);
    window.setPaintModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(150, 10));  // ~1.5s.
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(150, 10));

    QVERIFY(window.isPlaying());
    auto* panel = window.findChild<PlaybackPanel*>();
    QVERIFY(panel != nullptr);
    auto* label = panel->findChild<QLabel*>(QStringLiteral("positionLabel"));
    QVERIFY(label != nullptr);
    QCOMPARE(label->text(), QStringLiteral("0:01 / 0:02"));

    window.stopPlayback();
}

void MainWindowTest::oneShotDeltaPlaybackHaltsAtTheEditsEndWithoutLooping() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-oneshot-delta-halt.wav";
    writeTestWavFile(path);

    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 200;
    settings.canvasHeight = 50;
    settings.binCount = 50;
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-oneshot-delta-halt.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(settings, projectPath));
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    window.setPlaybackScope(PlaybackScope::Delta);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(200, 50);
    window.setPaintModeEnabled(true);
    // A single-column click (not a dragged stroke) so the edited region's
    // own end is a known, narrow point well before the track's own end -
    // see checkRepeatPlaybackRange()'s own docs on why a genuinely
    // zero-width Delta/Review range is instead treated as "nothing to loop
    // over" (no halt/loop at all, straight through) - a couple of columns
    // wide avoids that edge case while keeping a known, narrow range.
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(150, 10));
    QTest::mouseMove(canvas, QPoint(152, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(152, 10));
    QVERIFY(window.isPlaying());

    // Repeat is off, so per docs/sound-mind-design.md's "it continues to
    // the last modified column, and then halts... depending on the repeat
    // checkbox" - reaching the edited region's own end should halt there,
    // not loop back to its start and not continue playing past it.
    window.seekPlayback(1.52);
    QVERIFY(!window.isPlaying());

    window.stopPlayback();
}

void MainWindowTest::paintingWithRepeatOffAndReviewScopeStartsAOneShotPlaybackAutomatically() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-oneshot-review.wav";
    writeTestWavFile(path);

    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 200;
    settings.canvasHeight = 50;
    settings.binCount = 50;
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-oneshot-review.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(settings, projectPath));
    QVERIFY(window.importAudioFile(path));
    std::filesystem::remove(path);

    window.setPlaybackScope(PlaybackScope::Review);
    QVERIFY(!window.isPlaying());

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(200, 50);
    window.setPaintModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(150, 10));  // ~1.5s.
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(150, 10));

    QVERIFY(window.isPlaying());
    auto* panel = window.findChild<PlaybackPanel*>();
    QVERIFY(panel != nullptr);
    auto* label = panel->findChild<QLabel*>(QStringLiteral("positionLabel"));
    QVERIFY(label != nullptr);
    QCOMPARE(label->text(), QStringLiteral("0:01 / 0:02"));

    // Unlike Delta, Review's own range runs to the whole track's end - with
    // Repeat off, reaching that end should still halt (not loop back to
    // the track's start the way paintingWhileRepeatIsOnWithReviewScope...
    // does with Repeat on).
    window.seekPlayback(2.0);
    QVERIFY(!window.isPlaying());

    window.stopPlayback();
}

void MainWindowTest::paintModeIsOffByDefault() {
    const TestMainWindow window;
    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::None);
}

void MainWindowTest::setPaintModeEnabledTogglesTheCanvasToolMode() {
    TestMainWindow window;
    createFreshTestProject(window);
    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);

    window.setPaintModeEnabled(true);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::Paint);

    window.setPaintModeEnabled(false);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::None);
}

void MainWindowTest::paintingOnTheCanvasAppendsAPaintOperationToTheProjectsLog() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-paint-append.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->resize(100, 50);
    window.setPaintModeEnabled(true);

    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseMove(canvas, QPoint(50, 20));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(70, 25));

    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});
}

void MainWindowTest::undoAndRedoDelegateToThePaintController() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-paint-undo-redo.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->resize(100, 50);
    window.setPaintModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));

    QVERIFY(window.project()->operationLog().canUndo());
    window.undo();
    QVERIFY(!window.project()->operationLog().canUndo());
    QVERIFY(window.project()->operationLog().canRedo());

    window.redo();
    QVERIFY(window.project()->operationLog().canUndo());
    QVERIFY(!window.project()->operationLog().canRedo());
}

void MainWindowTest::undoInterleavesPaintStrokesAndLayerPropertyChangesInChronologicalOrder() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-mixed-undo-redo.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->resize(100, 50);
    window.setPaintModeEnabled(true);

    const sound_mind::core::LayerId backgroundId = window.project()->layers().front().id();
    const float originalOpacity = window.project()->layers().front().opacity();

    // 1. Paint a stroke (a content operation, undone via OperationLog).
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});

    // 2. Change the background layer's own opacity (a property change,
    // undone via UndoStack directly) - strictly *after* the stroke above.
    window.setLayerOpacity(backgroundId, 0.25f);
    QCOMPARE(window.project()->layers().front().opacity(), 0.25f);

    // Undo must reverse the *opacity change* first - it happened more
    // recently than the paint stroke, regardless of which of the two
    // independent mechanisms (OperationLog vs UndoStack) actually
    // recorded it.
    window.undo();
    QCOMPARE(window.project()->layers().front().opacity(), originalOpacity);
    QVERIFY(window.project()->operationLog().canUndo());  // The stroke itself is still active.

    // A second undo now reverses the stroke.
    window.undo();
    QVERIFY(!window.project()->operationLog().canUndo());

    // Redo must restore both, in the same original order: stroke first,
    // then opacity.
    window.redo();
    QVERIFY(window.project()->operationLog().canUndo());
    QCOMPARE(window.project()->layers().front().opacity(), originalOpacity);

    window.redo();
    QCOMPARE(window.project()->layers().front().opacity(), 0.25f);
}

void MainWindowTest::settingANewProjectResetsPaintModeToOff() {
    const auto firstPath = std::filesystem::temp_directory_path() / "sound-mind-test-paint-reset-1.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), firstPath));
    std::filesystem::remove(firstPath);
    window.setPaintModeEnabled(true);

    const auto secondPath = std::filesystem::temp_directory_path() / "sound-mind-test-paint-reset-2.smproj";
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), secondPath));
    std::filesystem::remove(secondPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::None);
}

void MainWindowTest::paintingWithTheDefaultToolConfigurationActuallyPaintsSomethingVisible() {
    const auto imagePath = std::filesystem::temp_directory_path() / "sound-mind-test-paint-visible.png";
    writeImageScalingTestImage(imagePath);  // 30x20.
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-paint-visible.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));  // 100x50 canvas.
    std::filesystem::remove(projectPath);

    // Painting targets paintTargetLayerId()'s own choice - with no row
    // selected in the LayersPanel, that falls back to the Background
    // layer (not the imported one - see paintTargetLayerId()'s own docs),
    // so the imported layer is explicitly selected here to give *that*
    // layer real content to paint onto, exactly matching canvas->resize()
    // below.
    QVERIFY(window.importImageFile(imagePath, ImageScalePickerDialog::Mode::RescaleToFitProject));
    std::filesystem::remove(imagePath);
    auto* layersPanel = window.findChild<LayersPanel*>();
    QVERIFY(layersPanel != nullptr);
    layersPanel->selectLayer(topmostNonEqualizerLayer(*window.project()).id());

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    // canvas is layout-managed (MainWindow's own central QStackedWidget) -
    // a plain resize() here wouldn't reliably stick without a real layout
    // pass, which headless tests that never show() the window don't get.
    // setFixedSize() pins both the minimum and maximum size, which a
    // layout must respect regardless of whether it ever actually runs.
    canvas->setFixedSize(100, 50);
    window.setPaintModeEnabled(true);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::Paint);
    QCOMPARE(canvas->size(), QSize(100, 50));

    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});

    // Painted with the ToolConfigurationPanel's own real, opaque default
    // (0 dB / 100% - see its own docs) - unlike before this panel
    // existed, this should now actually change stored pixel data, not
    // just append a no-visible-effect logged operation. Checked on the
    // *right* channel specifically: the imported image is pure red (see
    // writeImageScalingTestImage()'s own docs), meaning its left channel
    // already starts at 0 dB - painting *to* 0 dB there would converge to
    // the same value it started at and prove nothing either way. The
    // right channel starts near the silent floor instead, so painting it
    // to 0 dB is the one channel guaranteed to show a real numeric change.
    const auto& content = *topmostNonEqualizerLayer(*window.project()).content();
    const bool anyPainted = std::any_of(content.rightMagnitudeDb.begin(), content.rightMagnitudeDb.end(),
                                         [](float value) { return value > -50.0f; });
    QVERIFY(anyPainted);
}

void MainWindowTest::toggleToolConfigurationPanelShowsAndHidesIt() {
    TestMainWindow window;
    createFreshTestProject(window);
    auto* panel = window.findChild<ToolConfigurationPanel*>();
    QVERIFY(panel != nullptr);
    QVERIFY(panel->isHidden());  // off by default - see its own docs.

    QAction* toggleAction = panel->toggleViewAction();
    QVERIFY(toggleAction != nullptr);
    auto* toolBar = window.findChild<QToolBar*>();
    QVERIFY(toolBar != nullptr);
    QVERIFY(toolBar->actions().contains(toggleAction));

    toggleAction->trigger();
    QVERIFY(!panel->isHidden());

    toggleAction->trigger();
    QVERIFY(panel->isHidden());
}

void MainWindowTest::paintingTargetsTheSelectedLayerNotNecessarilyTheTopmostOne() {
    const auto imagePathA = std::filesystem::temp_directory_path() / "sound-mind-test-paint-target-a.png";
    const auto imagePathB = std::filesystem::temp_directory_path() / "sound-mind-test-paint-target-b.png";
    writeImageScalingTestImage(imagePathA);
    writeImageScalingTestImage(imagePathB);
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-paint-target.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    QVERIFY(window.importImageFile(imagePathA, ImageScalePickerDialog::Mode::RescaleToFitProject));
    const auto firstLayerId = topmostNonEqualizerLayer(*window.project()).id();
    QVERIFY(window.importImageFile(imagePathB, ImageScalePickerDialog::Mode::RescaleToFitProject));
    std::filesystem::remove(imagePathA);
    std::filesystem::remove(imagePathB);
    const auto secondLayerId = topmostNonEqualizerLayer(*window.project()).id();
    QVERIFY(firstLayerId != secondLayerId);

    // The first-imported layer is no longer topmost (the second import sits
    // above it) - selecting its row should still make it the paint target,
    // not the now-topmost second layer.
    auto* panel = window.findChild<LayersPanel*>();
    QVERIFY(panel != nullptr);
    // Lets each of the two imports' own refreshLayersPanel() -> setLayers()
    // calls actually deleteLater() its predecessor's row widgets first -
    // see refreshLayersPanelReflectsTheCurrentLayers()'s own comment for
    // why, and setLayers()'s own docs for the underlying Qt gotcha.
    QTest::qWait(0);
    const auto nameLabels = panel->findChildren<QLabel*>(QStringLiteral("nameLabel"));
    QCOMPARE(nameLabels.size(), 4);  // Equalizer, Background, plus the two imported layers.
    // Top-to-bottom: 0 = Equalizer, 1 = second import (topmost import), 2 = first import, 3 = Background.
    QTest::mouseClick(nameLabels.at(2), Qt::LeftButton);
    QVERIFY(panel->selectedLayerId().has_value());
    QCOMPARE(*panel->selectedLayerId(), firstLayerId);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    window.setPaintModeEnabled(true);

    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));

    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});
    const auto targetLayer = window.project()->operationLog().at(0).targetLayer();
    QVERIFY(targetLayer.has_value());
    QCOMPARE(*targetLayer, firstLayerId);
}

void MainWindowTest::addEmptyLayerAddsASilentLayerAndSelectsIt() {
    TestMainWindow window;
    createFreshTestProject(window);
    const auto layerCountBefore = window.project()->layers().size();

    auto* panel = window.findChild<LayersPanel*>();
    QVERIFY(panel != nullptr);
    auto* addButton = panel->findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    QVERIFY(addButton != nullptr);
    addButton->click();

    QCOMPARE(window.project()->layers().size(), layerCountBefore + 1);
    const auto& newLayer = topmostNonEqualizerLayer(*window.project());
    QCOMPARE(newLayer.type(), sound_mind::core::LayerType::Normal);
    QVERIFY(newLayer.content().has_value());  // a real, silent placeholder - see addEmptyLayer()'s own docs.

    // Selected immediately - ready to paint into without an extra click.
    QVERIFY(panel->selectedLayerId().has_value());
    QCOMPARE(*panel->selectedLayerId(), newLayer.id());
}

void MainWindowTest::addEmptyLayerIsANoOpWithNoProjectOpen() {
    TestMainWindow window;
    // Nothing to find/click a real addLayerButton through with no project
    // (and thus no LayersPanel content) yet - calling the testable core
    // directly, the same way other "no project open" guards are tested
    // elsewhere in this file, confirms it doesn't crash.
    window.addEmptyLayer();
    QVERIFY(window.project() == nullptr);
}

void MainWindowTest::addFilterLayerAddsAFilterTypeLayerAndSelectsIt() {
    TestMainWindow window;
    createFreshTestProject(window);
    const auto layerCountBefore = window.project()->layers().size();

    auto* panel = window.findChild<LayersPanel*>();
    QVERIFY(panel != nullptr);
    auto* addFilterButton = panel->findChild<QPushButton*>(QStringLiteral("addFilterLayerButton"));
    QVERIFY(addFilterButton != nullptr);
    addFilterButton->click();

    QCOMPARE(window.project()->layers().size(), layerCountBefore + 1);
    const auto& newLayer = topmostNonEqualizerLayer(*window.project());
    QCOMPARE(newLayer.type(), sound_mind::core::LayerType::Filter);
    QVERIFY(!newLayer.content().has_value());  // never painted onto - see addFilterLayer()'s own docs.

    QVERIFY(panel->selectedLayerId().has_value());
    QCOMPARE(*panel->selectedLayerId(), newLayer.id());
}

void MainWindowTest::selectingAFilterLayerLoadsAndEnablesFilterConfigurationPanel() {
    TestMainWindow window;
    createFreshTestProject(window);
    window.addFilterLayer();  // Selects it immediately.
    const auto filterLayerId = topmostNonEqualizerLayer(*window.project()).id();

    auto* layersPanel = window.findChild<LayersPanel*>();
    QVERIFY(layersPanel != nullptr);
    auto* filterPanel = window.findChild<FilterConfigurationPanel*>();
    QVERIFY(filterPanel != nullptr);

    // Set a distinctive value via the normal write path, then deselect
    // and reselect - confirming the reload genuinely pulls from the
    // layer's own current data, not just leftover UI state.
    sound_mind::core::FilterConfiguration config;
    config.frequencyGradient().setStopValues(0, {0.0f, -20.0f, -30.0f, 0.5f, 0.6f});
    window.applyFilterConfiguration(config);
    layersPanel->clearSelection();
    // Finding #13 (real-world testing pass, 2026-09-20): the panel stays
    // enabled with nothing selected, now showing/editing the *pending*
    // configuration the next "+ Add Filter Layer" will use - see
    // addFilterLayerSeedsTheNewLayerFromThePendingFilterConfiguration()'s
    // own docs. It's still a fresh default here (that -20.0f edit above
    // went to the real, already-added filter layer, not the pending slot).
    QVERIFY(filterPanel->isEnabled());
    QCOMPARE(filterPanel->filterConfiguration().frequencyGradient().stops().front().leftIntensity,
              sound_mind::core::FilterConfiguration{}.frequencyGradient().stops().front().leftIntensity);

    layersPanel->selectLayer(filterLayerId);

    QVERIFY(filterPanel->isEnabled());
    QCOMPARE(filterPanel->filterConfiguration().frequencyGradient().stops().front().leftIntensity, -20.0f);
}

void MainWindowTest::selectingANormalLayerShowsThePendingFilterConfigurationInsteadOfDisablingThePanel() {
    // Finding #13 (real-world testing pass, 2026-09-20): selecting a
    // Normal-type layer used to disable the panel entirely - it now stays
    // enabled, showing/editing the *pending* configuration (the one the
    // next "+ Add Filter Layer" will use), so a filter can be dialed in
    // before any Filter layer exists at all.
    TestMainWindow window;
    createFreshTestProject(window);
    window.addFilterLayer();
    const auto filterLayerId = topmostNonEqualizerLayer(*window.project()).id();
    auto* layersPanel = window.findChild<LayersPanel*>();
    QVERIFY(layersPanel != nullptr);
    layersPanel->selectLayer(filterLayerId);
    auto* filterPanel = window.findChild<FilterConfigurationPanel*>();
    QVERIFY(filterPanel != nullptr);
    QVERIFY(filterPanel->isEnabled());

    sound_mind::core::FilterConfiguration config;
    config.frequencyGradient().setStopValues(0, {0.0f, -20.0f, -30.0f, 0.5f, 0.6f});
    window.applyFilterConfiguration(config);  // Written to the real, selected filter layer.

    layersPanel->selectLayer(window.project()->layers().front().id());  // Background - a Normal-ish, non-Filter type.

    QVERIFY(filterPanel->isEnabled());
    // Shows the (still-default) pending configuration, not the filter
    // layer's own just-edited -20.0f - the two are deliberately separate.
    QCOMPARE(filterPanel->filterConfiguration().frequencyGradient().stops().front().leftIntensity,
              sound_mind::core::FilterConfiguration{}.frequencyGradient().stops().front().leftIntensity);

    const auto* filterLayer = window.project()->layerById(filterLayerId);
    QVERIFY(filterLayer != nullptr);
    QCOMPARE(filterLayer->filterConfiguration().frequencyGradient().stops().front().leftIntensity, -20.0f);
}

void MainWindowTest::addFilterLayerSeedsTheNewLayerFromThePendingFilterConfiguration() {
    // Finding #13's own motivating case: configure the filter *before*
    // adding one, so "+ Add Filter Layer" lands already set up the way
    // the user wants, rather than forcing an unwanted default filter to
    // land on the canvas first.
    TestMainWindow window;
    createFreshTestProject(window);

    auto* filterPanel = window.findChild<FilterConfigurationPanel*>();
    QVERIFY(filterPanel != nullptr);
    QVERIFY(filterPanel->isEnabled());  // Nothing selected yet - still enabled.

    sound_mind::core::FilterConfiguration config;
    config.frequencyGradient().setStopValues(0, {0.0f, -20.0f, -30.0f, 0.5f, 0.6f});
    window.applyFilterConfiguration(config);

    window.addFilterLayer();

    const auto* newLayer = &topmostNonEqualizerLayer(*window.project());
    QCOMPARE(newLayer->filterConfiguration().frequencyGradient().stops().front().leftIntensity, -20.0f);
}

void MainWindowTest::pendingFilterConfigurationPersistsAcrossMultipleAddedFilterLayers() {
    TestMainWindow window;
    createFreshTestProject(window);

    sound_mind::core::FilterConfiguration config;
    config.frequencyGradient().setStopValues(0, {0.0f, -20.0f, -30.0f, 0.5f, 0.6f});
    window.applyFilterConfiguration(config);

    window.addFilterLayer();
    const auto firstFilterId = topmostNonEqualizerLayer(*window.project()).id();

    // Deselecting (rather than leaving the just-added filter layer
    // selected) confirms the *pending* value survived being consumed once
    // - not just that the still-selected layer's own real config didn't
    // change.
    auto* layersPanel = window.findChild<LayersPanel*>();
    QVERIFY(layersPanel != nullptr);
    layersPanel->clearSelection();

    window.addFilterLayer();
    const auto* secondLayer = &topmostNonEqualizerLayer(*window.project());
    QVERIFY(secondLayer->id() != firstFilterId);
    QCOMPARE(secondLayer->filterConfiguration().frequencyGradient().stops().front().leftIntensity, -20.0f);
}

void MainWindowTest::pendingFilterConfigurationResetsForAFreshProject() {
    TestMainWindow window;
    createFreshTestProject(window);

    sound_mind::core::FilterConfiguration config;
    config.frequencyGradient().setStopValues(0, {0.0f, -20.0f, -30.0f, 0.5f, 0.6f});
    window.applyFilterConfiguration(config);

    createFreshTestProject(window);  // A brand new project in the same window.

    window.addFilterLayer();
    const auto* newLayer = &topmostNonEqualizerLayer(*window.project());
    QCOMPARE(newLayer->filterConfiguration().frequencyGradient().stops().front().leftIntensity,
              sound_mind::core::FilterConfiguration{}.frequencyGradient().stops().front().leftIntensity);
}

void MainWindowTest::selectingTheEqualizerLayerSwitchesTheFilterConfigurationPanelToCutMode() {
    TestMainWindow window;
    createFreshTestProject(window);
    auto* layersPanel = window.findChild<LayersPanel*>();
    QVERIFY(layersPanel != nullptr);
    auto* filterPanel = window.findChild<FilterConfigurationPanel*>();
    QVERIFY(filterPanel != nullptr);
    // A regular Filter layer first, to confirm switching *back* out of
    // Equalizer mode also works, not just into it.
    window.addFilterLayer();
    const auto ordinaryFilterId = topmostNonEqualizerLayer(*window.project()).id();
    layersPanel->selectLayer(ordinaryFilterId);
    // Real-world testing pass, 2026-09-20, finding #17: Cut mode now
    // reuses the same gradient editor a regular Filter layer shows,
    // switched into Cut mode (intensity fields hidden) - see
    // FilterConfigurationPanel::setEqualizerMode()'s own docs - rather
    // than a second, separate "Cut" group.
    QVERIFY(!filterPanel->findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftIntensitySpinBox"))->isHidden());

    const auto equalizerId = window.project()->layers().back().id();  // Always the topmost layer.
    layersPanel->selectLayer(equalizerId);

    QVERIFY(filterPanel->isEnabled());
    QVERIFY(filterPanel->findChild<QComboBox*>(QStringLiteral("filterTypeCombo"))->isHidden());
    QVERIFY(filterPanel->findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftIntensitySpinBox"))->isHidden());

    layersPanel->selectLayer(ordinaryFilterId);

    QVERIFY(!filterPanel->findChild<QComboBox*>(QStringLiteral("filterTypeCombo"))->isHidden());
    QVERIFY(!filterPanel->findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftIntensitySpinBox"))->isHidden());
}

void MainWindowTest::editingFilterConfigurationPanelWritesBackToTheSelectedLayer() {
    TestMainWindow window;
    createFreshTestProject(window);
    window.addFilterLayer();
    const auto filterLayerId = topmostNonEqualizerLayer(*window.project()).id();
    auto* layersPanel = window.findChild<LayersPanel*>();
    QVERIFY(layersPanel != nullptr);
    layersPanel->selectLayer(filterLayerId);
    auto* filterPanel = window.findChild<FilterConfigurationPanel*>();
    QVERIFY(filterPanel != nullptr);
    auto* spinBox = filterPanel->findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftIntensitySpinBox"));
    QVERIFY(spinBox != nullptr);

    spinBox->setValue(-15.0);

    const auto* layer = window.project()->layerById(filterLayerId);
    QVERIFY(layer != nullptr);
    QCOMPARE(layer->filterConfiguration().frequencyGradient().stops().front().leftIntensity, -15.0f);
}

void MainWindowTest::movingTheMouseOverTheCanvasUpdatesTheCursorPositionLabel() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-cursor-position.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));  // 100x50 canvas.
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    auto* label = window.findChild<QLabel*>(QStringLiteral("cursorPositionLabel"));
    QVERIFY(label != nullptr);
    QVERIFY(label->text().isEmpty());  // nothing shown before the mouse ever moves over it.

    // A hand-built QMouseEvent, sent directly to canvas - see
    // CanvasWidgetTest::mouseMoveEmitsCursorMovedRegardlessOfToolMode()'s
    // own comment for why QTest::mouseMove() alone (with no button held,
    // and no window ever shown()) wouldn't actually reach canvas.
    QMouseEvent moveEvent(QEvent::MouseMove, QPointF(30, 10), canvas->mapToGlobal(QPoint(30, 10)), Qt::NoButton,
                          Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(canvas, &moveEvent);

    QVERIFY(!label->text().isEmpty());
    QVERIFY(label->text().contains(QStringLiteral("30")));
    QVERIFY(label->text().contains(QStringLiteral("px")));
    QVERIFY(label->text().contains(QStringLiteral("s,")));  // the time/frequency half is present too.
}

void MainWindowTest::leavingTheCanvasClearsTheCursorPositionLabel() {
    TestMainWindow window;
    createFreshTestProject(window);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    auto* label = window.findChild<QLabel*>(QStringLiteral("cursorPositionLabel"));
    QVERIFY(label != nullptr);

    QMouseEvent moveEvent(QEvent::MouseMove, QPointF(5, 5), canvas->mapToGlobal(QPoint(5, 5)), Qt::NoButton,
                          Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(canvas, &moveEvent);
    QVERIFY(!label->text().isEmpty());

    QEvent leaveEvent(QEvent::Leave);
    QCoreApplication::sendEvent(canvas, &leaveEvent);

    QVERIFY(label->text().isEmpty());
}

void MainWindowTest::paintingTheBackgroundLayerActuallyPaintsSomethingVisible() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-paint-background.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));  // 100x50 canvas.
    std::filesystem::remove(projectPath);
    // The Background layer - content-less by construction
    // (Project::createNew()'s own docs) - is the only *paintable* layer
    // here (an Equalizer also exists now, but is never a paint target -
    // see paintTargetLayerId()'s own docs), so it's paintTargetLayerId()'s
    // own default with nothing ever selected.
    QVERIFY(!topmostNonEqualizerLayer(*window.project()).content().has_value());

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    window.setPaintModeEnabled(true);

    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));

    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});
    QVERIFY(topmostNonEqualizerLayer(*window.project()).content().has_value());
    const auto& content = *topmostNonEqualizerLayer(*window.project()).content();
    // Unlike an imported image (which can already start at the brush's
    // own target on one channel - see paintingWithTheDefaultTool
    // ConfigurationActuallyPaintsSomethingVisible()'s own comment), the
    // Background's synthesized silent base (silentContentFor()) starts
    // near the floor on *both* channels, so either one shows a real
    // change here.
    const bool anyPainted = std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                                         [](float value) { return value > -50.0f; });
    QVERIFY(anyPainted);
}

void MainWindowTest::pickAndPaintToolbarActionsAreMutuallyExclusive() {
    TestMainWindow window;
    createFreshTestProject(window);
    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);

    window.setPaintModeEnabled(true);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::Paint);

    window.setPickModeEnabled(true);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::Pick);

    window.setPaintModeEnabled(true);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::Paint);
}

void MainWindowTest::pickingAPaintedStrokeLoadsItsSettingsIntoThePanel() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-pick-load-panel.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));  // 100x50 canvas.
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    // Painting the Background layer directly - no import needed, it's a
    // real paintable canvas now (see docs/sound-mind-architecture.md's
    // Decisions Made on why).
    window.setPaintModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});

    auto* panel = window.findChild<ToolConfigurationPanel*>();
    QVERIFY(panel != nullptr);
    auto* sizeSpinBox = panel->findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"));
    QVERIFY(sizeSpinBox != nullptr);
    // Changes the "current brush" default away from what the stroke above
    // was actually painted with (ToolConfiguration's own default, 0.2) -
    // so reverting to 0.2 below can only mean the pick genuinely loaded
    // the stroke's own stored settings back in, not just left the panel
    // showing whatever it already had.
    sizeSpinBox->setValue(5.0);

    window.setPaintModeEnabled(false);
    window.setPickModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));

    QCOMPARE(sizeSpinBox->value(), 0.2);
}

void MainWindowTest::movingAPickedStrokeCommitsATranslatedOperation() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-pick-move.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    window.setPaintModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});

    window.setPaintModeEnabled(false);
    window.setPickModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseMove(canvas, QPoint(60, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 10));

    QCOMPARE(window.project()->operationLog().size(), std::size_t{2});
    const auto layerId = topmostNonEqualizerLayer(*window.project()).id();
    const auto active = window.project()->operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{1});  // the original is superseded, not still active alongside the move.
}

void MainWindowTest::deletingAPickedStrokeLeavesAnEmptyTombstone() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-pick-delete.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    window.setPaintModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});

    window.setPaintModeEnabled(false);
    window.setPickModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));

    window.deletePickedObject();

    QCOMPARE(window.project()->operationLog().size(), std::size_t{2});
    const auto layerId = topmostNonEqualizerLayer(*window.project()).id();
    const auto active = window.project()->operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{1});
    const auto* tombstone = dynamic_cast<const sound_mind::core::PaintOperation*>(active.front());
    QVERIFY(tombstone != nullptr);
    QVERIFY(tombstone->path().nodes().empty());
}

void MainWindowTest::settingANewProjectResetsPickModeToOff() {
    const auto firstPath = std::filesystem::temp_directory_path() / "sound-mind-test-pick-reset-1.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), firstPath));
    std::filesystem::remove(firstPath);
    window.setPickModeEnabled(true);

    const auto secondPath = std::filesystem::temp_directory_path() / "sound-mind-test-pick-reset-2.smproj";
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), secondPath));
    std::filesystem::remove(secondPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::None);
}

void MainWindowTest::pickingTheSameSpotTwiceSelectsTheOccludedStrokeUnderneath() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-pick-occluded.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    auto* panel = window.findChild<ToolConfigurationPanel*>();
    QVERIFY(panel != nullptr);
    auto* sizeSpinBox = panel->findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"));
    QVERIFY(sizeSpinBox != nullptr);

    // Two taps at the exact same spot - the second (larger-brushed) one
    // fully occludes the first, the scenario a plain "topmost always
    // wins" pick() could never select past.
    window.setPaintModeEnabled(true);
    sizeSpinBox->setValue(0.5);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    sizeSpinBox->setValue(2.0);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QCOMPARE(window.project()->operationLog().size(), std::size_t{2});

    window.setPaintModeEnabled(false);
    window.setPickModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QCOMPARE(sizeSpinBox->value(), 2.0);  // the topmost (most recently painted) stroke, first.

    // Clicking the exact same, already-selected spot again cycles to the
    // occluded stroke underneath.
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QCOMPARE(sizeSpinBox->value(), 0.5);
}

void MainWindowTest::paintPickAndSelectToolbarActionsAreAllMutuallyExclusive() {
    TestMainWindow window;
    createFreshTestProject(window);
    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);

    window.setPaintModeEnabled(true);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::Paint);

    window.setSelectModeEnabled(true);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::Select);

    window.setPickModeEnabled(true);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::Pick);

    window.setPathModeEnabled(true);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::Path);

    window.setSelectModeEnabled(true);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::Select);
}

void MainWindowTest::drawingASelectionAndFillingItChangesTheLayersContent() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-select-fill.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));  // 100x50 canvas.
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    // Drawing onto the Background layer directly - no import needed, it's
    // a real paintable/fillable canvas now (see docs/sound-mind-
    // architecture.md's Decisions Made on why).
    window.setSelectModeEnabled(true);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::Select);

    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));
    QTest::mouseMove(canvas, QPoint(60, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 30));

    window.fillSelectionWith(QColor(255, 0, 0));  // pure red - loud left channel.

    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});
    const auto& content = *topmostNonEqualizerLayer(*window.project()).content();
    const bool anyFilled = std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                                        [](float value) { return value > -50.0f; });
    QVERIFY(anyFilled);
}

void MainWindowTest::selectionPersistsAfterSwitchingAwayFromSelectMode() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-select-persist.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    window.setSelectModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));
    QTest::mouseMove(canvas, QPoint(60, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 30));

    // A selection scopes Fill independent of whichever tool is currently
    // active - switching to Paint shouldn't clear it.
    window.setPaintModeEnabled(true);
    window.fillSelectionWith(QColor(0, 255, 0));

    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});
}

void MainWindowTest::deselectClearsTheCurrentSelectionSoFillBecomesANoOp() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-deselect.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    window.setSelectModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));
    QTest::mouseMove(canvas, QPoint(60, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 30));

    window.deselect();
    window.fillSelectionWith(QColor(0, 0, 255));

    QCOMPARE(window.project()->operationLog().size(), std::size_t{0});
}

void MainWindowTest::fillSelectionWithIsANoOpWithNoSelection() {
    TestMainWindow window;
    createFreshTestProject(window);

    window.fillSelectionWith(QColor(255, 0, 0));

    QCOMPARE(window.project()->operationLog().size(), std::size_t{0});
}

void MainWindowTest::settingANewProjectResetsSelectModeToOff() {
    const auto firstPath = std::filesystem::temp_directory_path() / "sound-mind-test-select-reset-1.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), firstPath));
    std::filesystem::remove(firstPath);
    window.setSelectModeEnabled(true);

    const auto secondPath = std::filesystem::temp_directory_path() / "sound-mind-test-select-reset-2.smproj";
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), secondPath));
    std::filesystem::remove(secondPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::None);
}

namespace {

/// @brief Whether any pixel in `content`'s own left channel is loud
/// (above -50dB) - the same threshold drawingASelectionAndFillingItChanges
/// TheLayersContent() already uses to confirm a Fill actually wrote
/// something, reused here to confirm Cut/Copy/Paste moved real pixels.
bool anyLoudLeftChannelPixel(const sound_mind::codec::StreamImage& content) {
    return std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                        [](float value) { return value > -50.0f; });
}

/// @brief The left channel's own dB value at a specific widget pixel -
/// for tests needing to check one exact spot (e.g. "is this specific
/// region still silent"), not just "is anything loud anywhere". Assumes
/// a 100x50 canvas shown at a 1:1 (100x50) widget size, matching every
/// other test in this file that uses imageScalingTestProjectSettings()
/// with canvas->setFixedSize(100, 50) - and the same Y-flip
/// (`CanvasWidget::widgetPointToTimeFrequency()`'s own docs) a real click
/// at this same widget position would be converted through.
float leftDbAtWidgetPixel(const sound_mind::codec::StreamImage& content, int widgetX, int widgetY) {
    const int frame = widgetX;
    const int bin = 50 - widgetY;
    return content.leftMagnitudeDb[static_cast<std::size_t>(bin) * content.frameCount + static_cast<std::size_t>(frame)];
}

}  // namespace

void MainWindowTest::fillSelectionWithGradientAppliesARealMultiStopGradientAcrossTheSelection() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-fill-gradient.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));  // 100x50 canvas.
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    window.setSelectModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));
    QTest::mouseMove(canvas, QPoint(60, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 30));

    // Loud - quiet - loud across the selection's own time axis (t=0/0.5/1)
    // - applyFillOperation() evaluates the gradient left-to-right across the
    // selection (see its own docs); a flat, single-color fill (this
    // method's own prior shape) couldn't produce this at all.
    Gradient gradient;
    auto loud = gradient.stops().front();
    loud.leftIntensity = 0.0f;
    loud.rightIntensity = 0.0f;
    loud.leftOpacity = 1.0f;
    loud.rightOpacity = 1.0f;
    gradient.setStopValues(0, loud);
    gradient.setStopValues(1, loud);
    auto quiet = loud;
    quiet.leftIntensity = -80.0f;
    quiet.rightIntensity = -80.0f;
    const std::size_t midIndex = gradient.insertStop(0.5f);
    gradient.setStopValues(midIndex, quiet);

    window.fillSelectionWithGradient(gradient);

    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});
    const auto& content = *topmostNonEqualizerLayer(*window.project()).content();
    // x=20/60 are the selection's own start/end (t=0/1, loud); x=40 is its
    // midpoint (t=0.5, quiet).
    QVERIFY(leftDbAtWidgetPixel(content, 20, 20) > -10.0f);
    QVERIFY(leftDbAtWidgetPixel(content, 60, 20) > -10.0f);
    QVERIFY(leftDbAtWidgetPixel(content, 40, 20) < -50.0f);
}

void MainWindowTest::fillSelectionWithGradientIsANoOpWithNoSelection() {
    TestMainWindow window;
    createFreshTestProject(window);

    window.fillSelectionWithGradient(Gradient{});

    QCOMPARE(window.project()->operationLog().size(), std::size_t{0});
}

void MainWindowTest::copyThenPasteOnTheSameLayerReproducesTheSelection() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-copy-paste-same-layer.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    window.setSelectModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));
    QTest::mouseMove(canvas, QPoint(60, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 30));

    window.fillSelectionWith(QColor(255, 0, 0));  // pure red - loud left channel.
    window.copySelection();
    window.fillSelectionWith(QColor(0, 0, 0));  // black - silences the same region again.

    QVERIFY(!anyLoudLeftChannelPixel(*topmostNonEqualizerLayer(*window.project()).content()));

    window.paste();

    QVERIFY(anyLoudLeftChannelPixel(*topmostNonEqualizerLayer(*window.project()).content()));
    QCOMPARE(window.project()->operationLog().size(), std::size_t{3});  // fill, fill, paste - copy logs nothing.
}

void MainWindowTest::pasteUsesTheSelectionConfigurationPanelsOwnBlendMode() {
    const auto projectPath =
        std::filesystem::temp_directory_path() / "sound-mind-test-paste-blend-mode.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    window.setSelectModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));
    QTest::mouseMove(canvas, QPoint(60, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 30));
    window.copySelection();

    auto* panel = window.findChild<SelectionConfigurationPanel*>();
    QVERIFY(panel != nullptr);
    auto* combo = panel->findChild<QComboBox*>(QStringLiteral("pasteBlendModeCombo"));
    QVERIFY(combo != nullptr);
    combo->setCurrentIndex(combo->findText(QStringLiteral("Multiply")));

    window.paste();

    const auto layerId = topmostNonEqualizerLayer(*window.project()).id();
    const auto active = window.project()->operationLog().activeOperationsTargeting(layerId);
    const auto* pasteOp =
        dynamic_cast<const PasteOperation*>(*std::find_if(active.begin(), active.end(), [](const auto* op) {
            return dynamic_cast<const PasteOperation*>(op) != nullptr;
        }));
    QVERIFY(pasteOp != nullptr);
    QCOMPARE(pasteOp->blendMode(), BlendMode::Multiply);
}

void MainWindowTest::cutClearsTheSourceRegionButPasteStillReproducesIt() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-cut-paste-same-layer.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    window.setSelectModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));
    QTest::mouseMove(canvas, QPoint(60, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 30));

    window.fillSelectionWith(QColor(255, 0, 0));
    window.cutSelection();  // captures the loud pixels onto the clipboard, then silences them in place.

    QVERIFY(!anyLoudLeftChannelPixel(*topmostNonEqualizerLayer(*window.project()).content()));

    window.paste();

    QVERIFY(anyLoudLeftChannelPixel(*topmostNonEqualizerLayer(*window.project()).content()));
}

void MainWindowTest::pasteCanTargetADifferentLayerThanItWasCopiedFrom() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-paste-cross-layer.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    const auto sourceLayerId = topmostNonEqualizerLayer(*window.project()).id();  // Background - nothing else added yet.

    window.setSelectModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));
    QTest::mouseMove(canvas, QPoint(60, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 30));
    window.fillSelectionWith(QColor(255, 0, 0));
    window.cutSelection();

    // Switch the active layer via a real "+ Add Layer" gesture - the same
    // one addEmptyLayerAddsASilentLayerAndSelectsIt() covers - so
    // paintTargetLayerId() (and therefore paste()) now resolves to a
    // genuinely different layer than the one the clip was captured from.
    auto* panel = window.findChild<LayersPanel*>();
    QVERIFY(panel != nullptr);
    auto* addButton = panel->findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    QVERIFY(addButton != nullptr);
    addButton->click();
    const auto targetLayerId = topmostNonEqualizerLayer(*window.project()).id();
    QVERIFY(targetLayerId != sourceLayerId);

    window.paste();

    // Landed on the *target* layer, not back on the source - and the
    // source's own region stays silenced (Cut's own clear), not restored
    // by pasting elsewhere.
    QVERIFY(!anyLoudLeftChannelPixel(*window.project()->layerById(sourceLayerId)->content()));
    QVERIFY(anyLoudLeftChannelPixel(*window.project()->layerById(targetLayerId)->content()));
}

void MainWindowTest::pasteIsANoOpWithNoClipboard() {
    TestMainWindow window;
    createFreshTestProject(window);

    window.paste();

    QCOMPARE(window.project()->operationLog().size(), std::size_t{0});
}

void MainWindowTest::copySelectionIsANoOpWithNoSelection() {
    TestMainWindow window;
    createFreshTestProject(window);

    window.copySelection();
    window.paste();  // nothing was ever copied, so this is a no-op too.

    QCOMPARE(window.project()->operationLog().size(), std::size_t{0});
}

void MainWindowTest::captureMindShotAddsANamedEntryToTheProjectsMindShotLibrary() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-capture-mind-shot.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    window.setSelectModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));
    QTest::mouseMove(canvas, QPoint(60, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 30));

    window.captureMindShot();

    QCOMPARE(window.project()->mindShots().size(), std::size_t{1});
    QCOMPARE(window.project()->mindShots().front().name, std::string("Mind Shot 1"));
    // A capture never logs an Operation - it's a read, not an edit.
    QCOMPARE(window.project()->operationLog().size(), std::size_t{0});
}

void MainWindowTest::captureMindShotIsANoOpWithNoSelection() {
    TestMainWindow window;
    createFreshTestProject(window);

    window.captureMindShot();

    QVERIFY(window.project()->mindShots().empty());
}

void MainWindowTest::clickingInPathModePlacesNodesAndFinishPathCommitsANewPaintObject() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-path-tool.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    window.setPathModeEnabled(true);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::Path);

    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(40, 20));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(40, 20));
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 30));

    window.finishPath();

    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});
    const auto* painted = dynamic_cast<const PaintOperation*>(&window.project()->operationLog().at(0));
    QVERIFY(painted != nullptr);
    QCOMPARE(painted->path().nodes().size(), std::size_t{3});
}

void MainWindowTest::cancelPathDiscardsInProgressPlacementWithoutCommittingAnything() {
    TestMainWindow window;
    createFreshTestProject(window);
    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    window.setPathModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));

    window.cancelPath();
    window.finishPath();  // nothing left to finish - a no-op.

    QCOMPARE(window.project()->operationLog().size(), std::size_t{0});
}

void MainWindowTest::smoothNodesToggleAffectsSubsequentlyPlacedNodes() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-path-smooth.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    window.setPathModeEnabled(true);

    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));  // still Corner.
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 10));
    window.setPathPlacesSmoothNodes(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(40, 20));  // now Smooth.
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(40, 20));

    window.finishPath();

    const auto* painted = dynamic_cast<const PaintOperation*>(&window.project()->operationLog().at(0));
    QVERIFY(painted != nullptr);
    QCOMPARE(painted->path().nodes().size(), std::size_t{2});
    QCOMPARE(painted->path().nodes().at(0).type, PathNodeType::Corner);
    QCOMPARE(painted->path().nodes().at(1).type, PathNodeType::Smooth);
}

void MainWindowTest::finishPathWithNoNodesPlacedIsANoOp() {
    TestMainWindow window;
    createFreshTestProject(window);

    window.finishPath();

    QCOMPARE(window.project()->operationLog().size(), std::size_t{0});
}

void MainWindowTest::settingANewProjectResetsPathModeToOff() {
    const auto firstPath = std::filesystem::temp_directory_path() / "sound-mind-test-path-reset-1.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), firstPath));
    std::filesystem::remove(firstPath);
    window.setPathModeEnabled(true);

    const auto secondPath = std::filesystem::temp_directory_path() / "sound-mind-test-path-reset-2.smproj";
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), secondPath));
    std::filesystem::remove(secondPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::None);
}

void MainWindowTest::pastedContentIsPickableAndMovable() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-paste-pickable.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    auto* panel = window.findChild<ToolConfigurationPanel*>();
    QVERIFY(panel != nullptr);
    auto* sizeSpinBox = panel->findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"));
    QVERIFY(sizeSpinBox != nullptr);

    // Paint, select it, copy, then paste - lands back at the same spot,
    // onto the same active layer (see SelectionController::pasteInto()'s
    // own docs) - a second, independent object on top of the paint. A
    // small, precise brush size keeps the stroke's own *padded* pick
    // bounds from ballooning across the whole tiny test canvas (padding
    // scales with brush size - see PickController::pick()'s own docs),
    // which would otherwise make it indistinguishable from the paste
    // below.
    window.setPaintModeEnabled(true);
    sizeSpinBox->setValue(0.01);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseMove(canvas, QPoint(60, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 30));

    window.setPaintModeEnabled(false);
    window.setSelectModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 5));
    QTest::mouseMove(canvas, QPoint(70, 35));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(70, 35));
    window.copySelection();
    window.paste();
    QCOMPARE(window.project()->operationLog().size(), std::size_t{2});  // paint, paste.

    // paste() itself already switched to Pick and selected the pasted
    // region (see MainWindow::paste()'s own docs) - no separate switch-
    // to-Pick-and-click-it step needed to move it, unlike before. This is
    // the actual reported bug: a paste used to be entirely invisible to
    // Pick, let alone already selected there.
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(45, 20));
    QTest::mouseMove(canvas, QPoint(80, 5));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(80, 5));

    QCOMPARE(window.project()->operationLog().size(), std::size_t{3});  // paint, paste, moved-paste.
    const auto layerId = topmostNonEqualizerLayer(*window.project()).id();
    const auto active = window.project()->operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{2});  // paint + the moved paste (original paste now superseded).
    const bool anyPasteActive =
        std::any_of(active.begin(), active.end(), [](const auto* op) { return dynamic_cast<const PasteOperation*>(op) != nullptr; });
    QVERIFY(anyPasteActive);
}

void MainWindowTest::movingAPastedRegionPreservesItsOwnVerticalShapeAndOrientation() {
    // The frequency axis is log-scaled (see translateFrequencyByBins()'s
    // own docs) - translating a rect/path by a raw Hz delta doesn't
    // preserve its own on-screen (bin-space) shape, and can even drive a
    // bound negative near minFrequencyHz. Reported as "the vertical
    // position of the top corners (and top edge) of a pasted object is
    // still wrong" after moving it.
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-paste-move-vertical.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);

    // A thin loud strip at the *top* (widgetY 5-9) of an otherwise-silent
    // block (widgetY 5-35) - lets a move preserve or distort the strip's
    // own position *within* the pasted block be checked directly, not
    // just the block's own outer bounds.
    window.setSelectModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 5));
    QTest::mouseMove(canvas, QPoint(70, 9));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(70, 9));
    window.fillSelectionWith(QColor(255, 0, 0));

    // Select the *whole* block and copy it.
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 5));
    QTest::mouseMove(canvas, QPoint(70, 35));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(70, 35));
    window.copySelection();

    window.paste();  // lands back at (20,5)-(70,35); auto-Picked.

    // Move it straight down by 10 widget pixels.
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(45, 20));
    QTest::mouseMove(canvas, QPoint(45, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(45, 30));

    // The loud strip should now sit at widgetY 15-19 - still 10 pixels
    // from the pasted block's own top (widgetY 15), exactly tracking the
    // drag - not distorted or shifted to some other position by the
    // log-scaled frequency axis.
    const auto& content = *topmostNonEqualizerLayer(*window.project()).content();
    QVERIFY(leftDbAtWidgetPixel(content, 45, 14) < -50.0f);  // just above the strip: still silent.
    QVERIFY(leftDbAtWidgetPixel(content, 45, 15) > -50.0f);
    QVERIFY(leftDbAtWidgetPixel(content, 45, 19) > -50.0f);
    QVERIFY(leftDbAtWidgetPixel(content, 45, 20) < -50.0f);  // just below the strip: silent again.
}

void MainWindowTest::pasteSwitchesToPickModeAndSelectsThePastedRegionImmediately() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-paste-auto-pick.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);

    window.setSelectModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 5));
    QTest::mouseMove(canvas, QPoint(70, 35));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(70, 35));
    window.copySelection();

    window.paste();

    // Switched out of Select and into Pick on its own - the user never
    // touched the Pick toolbar button.
    QCOMPARE(canvas->toolMode(), CanvasWidget::ToolMode::Pick);
}

void MainWindowTest::modifyingAPaintedStrokeAfterCuttingOverItKeepsTheCutRegionSilenced() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-cut-then-modify.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    auto* panel = window.findChild<ToolConfigurationPanel*>();
    QVERIFY(panel != nullptr);
    auto* sizeSpinBox = panel->findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"));
    QVERIFY(sizeSpinBox != nullptr);

    // A single wide stroke spanning most of the canvas.
    window.setPaintModeEnabled(true);
    sizeSpinBox->setValue(5.0);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 25));
    QTest::mouseMove(canvas, QPoint(90, 25));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(90, 25));
    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});
    QVERIFY(anyLoudLeftChannelPixel(*topmostNonEqualizerLayer(*window.project()).content()));

    // Cut a sub-region out of the middle of the stroke.
    window.setPaintModeEnabled(false);
    window.setSelectModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(40, 20));
    QTest::mouseMove(canvas, QPoint(60, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 30));
    window.cutSelection();
    QCOMPARE(window.project()->operationLog().size(), std::size_t{2});
    QVERIFY(leftDbAtWidgetPixel(*topmostNonEqualizerLayer(*window.project()).content(), 50, 25) < -50.0f);

    // Pick the paint stroke *outside* the cut region (unambiguous - the
    // Fill's own bounds don't extend there) and modify it via Tool
    // Configuration - no geometry change, the exact reported scenario
    // ("if I then move or modify the thing I cut away from").
    window.setSelectModeEnabled(false);
    window.setPickModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(15, 25));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(15, 25));
    sizeSpinBox->setValue(6.0);  // triggers applyToolConfiguration() on the picked stroke.
    QCOMPARE(window.project()->operationLog().size(), std::size_t{3});

    // The cut region must still be silent - modifying the stroke it was
    // cut from must not undo the cut.
    QVERIFY(leftDbAtWidgetPixel(*topmostNonEqualizerLayer(*window.project()).content(), 50, 25) < -50.0f);

    const auto layerId = topmostNonEqualizerLayer(*window.project()).id();
    const auto active = window.project()->operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{2});
    QVERIFY(dynamic_cast<const PaintOperation*>(active[0]) != nullptr);  // the modified stroke, still...
    QVERIFY(dynamic_cast<const FillOperation*>(active[1]) != nullptr);   // ...below the cut's own silence fill.
}

void MainWindowTest::cutRegionIsPickableAndMovable() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-cut-pickable.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);

    // A stroke, then a Cut carved out of the middle of it - the exact
    // reported scenario ("Cut areas are not Pickable").
    window.setPaintModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QTest::mouseMove(canvas, QPoint(60, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(60, 30));

    window.setPaintModeEnabled(false);
    window.setSelectModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 5));
    QTest::mouseMove(canvas, QPoint(70, 35));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(70, 35));
    window.cutSelection();
    QCOMPARE(window.project()->operationLog().size(), std::size_t{2});  // paint, cut-fill.

    // Pick and move the (invisible, silenced) cut region itself, not the
    // stroke underneath it.
    window.setSelectModeEnabled(false);
    window.setPickModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(45, 20));
    QTest::mouseMove(canvas, QPoint(80, 5));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(80, 5));

    QCOMPARE(window.project()->operationLog().size(), std::size_t{3});  // paint, cut-fill, moved-cut-fill.
    const auto layerId = topmostNonEqualizerLayer(*window.project()).id();
    const auto active = window.project()->operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{2});  // paint (untouched) + the moved cut-fill (original now superseded).
    const bool anyFillActive =
        std::any_of(active.begin(), active.end(), [](const auto* op) { return dynamic_cast<const FillOperation*>(op) != nullptr; });
    QVERIFY(anyFillActive);
}

void MainWindowTest::bringPickedObjectToFrontMovesItAboveLaterStrokesOnTheSameLayer() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-bring-to-front.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);
    auto* panel = window.findChild<ToolConfigurationPanel*>();
    QVERIFY(panel != nullptr);
    auto* sizeSpinBox = panel->findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"));
    QVERIFY(sizeSpinBox != nullptr);
    sizeSpinBox->setValue(0.01);  // small and precise - see pickPadsHitTestingByTheOperationsOwnBrushSize's own docs.

    // Two separate, non-overlapping strokes - A painted first, B second.
    window.setPaintModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(90, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(90, 10));
    QCOMPARE(window.project()->operationLog().size(), std::size_t{2});

    window.setPaintModeEnabled(false);
    window.setPickModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));  // A.
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));

    window.bringPickedObjectToFront();

    const auto layerId = topmostNonEqualizerLayer(*window.project()).id();
    const auto active = window.project()->operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{2});
    QCOMPARE(active.front()->id(), window.project()->operationLog().at(1).id());  // B, now at the back.
    QCOMPARE(active.back()->id(), window.project()->operationLog().at(0).id());   // A, now on top.
}

void MainWindowTest::editingAPickedStrokesPathMovesANodeAndCommitsOnApply() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-edit-path-apply.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);

    // A straight-line stroke simplifies to exactly two nodes, at exactly
    // its own start/end click positions (see fitPathToPoints()'s own
    // "simplifies a straight line down to just its two endpoints").
    window.setPaintModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 25));
    QTest::mouseMove(canvas, QPoint(90, 25));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(90, 25));
    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});

    window.setPaintModeEnabled(false);
    window.setPickModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 25));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 25));

    window.editPickedPath();

    // Selects and drags the node at (10, 25) up to (10, 10).
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 25));
    QTest::mouseMove(canvas, QPoint(10, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));

    window.applyPickedPathEdit();

    QCOMPARE(window.project()->operationLog().size(), std::size_t{2});
    const auto layerId = topmostNonEqualizerLayer(*window.project()).id();
    const auto active = window.project()->operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{1});
    const auto* edited = dynamic_cast<const PaintOperation*>(active.front());
    QVERIFY(edited != nullptr);
    QVERIFY(edited->supersedes().has_value());
    QCOMPARE(edited->path().nodes().size(), std::size_t{2});
    // The moved node's own frequency changed - the other end didn't.
    QVERIFY(edited->path().nodes().front().anchor.frequencyHz != edited->path().nodes().back().anchor.frequencyHz);
}

void MainWindowTest::cancelingAPickedStrokesPathEditDiscardsTheDragWithoutCommitting() {
    const auto projectPath = std::filesystem::temp_directory_path() / "sound-mind-test-edit-path-cancel.smproj";
    TestMainWindow window;
    QVERIFY(window.createProjectAt(imageScalingTestProjectSettings(), projectPath));
    std::filesystem::remove(projectPath);

    auto* canvas = window.findChild<CanvasWidget*>();
    QVERIFY(canvas != nullptr);
    canvas->setFixedSize(100, 50);

    window.setPaintModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 25));
    QTest::mouseMove(canvas, QPoint(90, 25));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(90, 25));
    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});

    window.setPaintModeEnabled(false);
    window.setPickModeEnabled(true);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 25));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 25));

    window.editPickedPath();
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 25));
    QTest::mouseMove(canvas, QPoint(10, 10));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));

    window.cancelPickedPathEdit();

    QCOMPARE(window.project()->operationLog().size(), std::size_t{1});  // nothing committed.
}

void MainWindowTest::openUserDocIfBundledOpensItAndReturnsTrueWhenPresent() {
    const QString docsDir = QCoreApplication::applicationDirPath() + QStringLiteral("/docs");
    QVERIFY(QDir().mkpath(docsDir));
    const QString docPath = docsDir + QStringLiteral("/sound_mind_test_doc.html");
    QFile file(docPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("<html></html>");
    file.close();

    TestMainWindow window;
    const bool opened = window.openUserDocIfBundled(QStringLiteral("sound_mind_test_doc.html"));

    QFile::remove(docPath);

    QVERIFY(opened);
    QVERIFY(window.lastOpenedUrl.has_value());
    QCOMPARE(window.lastOpenedUrl->toLocalFile(), docPath);
}

void MainWindowTest::openUserDocIfBundledReturnsFalseWithoutOpeningAnythingWhenMissing() {
    const QString docPath = QCoreApplication::applicationDirPath() +
                             QStringLiteral("/docs/sound_mind_test_doc_missing.html");
    QFile::remove(docPath);  // in case a previous run left it behind.

    TestMainWindow window;
    const bool opened = window.openUserDocIfBundled(QStringLiteral("sound_mind_test_doc_missing.html"));

    QVERIFY(!opened);
    QVERIFY(!window.lastOpenedUrl.has_value());
}

void MainWindowTest::setHardwareAccelerationEnabledForwardsToCore() {
    TestMainWindow window;

    window.setHardwareAccelerationEnabled(false);
    QVERIFY(!sound_mind::core::hardwareAccelerationEnabled());

    // Restores the documented default - this process-wide flag is re-
    // applied fresh (from settings_) by every later TestMainWindow's own
    // constructor, but leaving it false here would still affect any test
    // that runs before the next TestMainWindow is constructed.
    window.setHardwareAccelerationEnabled(true);
    QVERIFY(sound_mind::core::hardwareAccelerationEnabled());
}
