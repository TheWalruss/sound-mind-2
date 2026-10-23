#include "sound_mind/studio/main_window.h"

#include <atomic>
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
#include <string>

#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QThread>
#include <QProgressDialog>
#include <QApplication>
#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStringList>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/stream_incremental_encoder.h"
#include "sound_mind/core/background_task.h"
#include "sound_mind/core/filter_preview.h"
#include "sound_mind/core/loudness_meter.h"
#include "sound_mind/core/pitch_detection.h"
#include "sound_mind/studio/remaster.h"
#include "sound_mind/studio/remaster_dialog.h"
#include "sound_mind/studio/background_job.h"
#include "sound_mind/studio/export_controller.h"
#include "sound_mind/studio/import_controller.h"
#include "sound_mind/studio/playback_preparer.h"
#include "sound_mind/studio/analysis_controller.h"
#include "sound_mind/studio/recording_controller.h"
#include "sound_mind/studio/loop_controller.h"
#include "sound_mind/studio/macro_controller.h"
#include "sound_mind/studio/base_recipes.h"
#include "sound_mind/studio/video_export_dialog.h"
#include "sound_mind/studio/macro_manager_dialog.h"
#include "sound_mind/studio/macro_player.h"
#include "sound_mind/studio/text_controller.h"
#include "sound_mind/codec/video_export.h"
#include "sound_mind/core/compositor.h"
#include "sound_mind/core/gpu_compute_availability.h"
#include "sound_mind/core/gradient.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project_export.h"
#include "sound_mind/core/mind_grain.h"
#include "sound_mind/core/mind_wave_dependencies.h"
#include "sound_mind/core/operation.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/resonant_instrument.h"
#include "sound_mind/core/tool_configuration_preview.h"
#include "sound_mind/studio/about_dialog.h"
#include "sound_mind/studio/audio_snippet_picker_dialog.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/color_conversion.h"
#include "sound_mind/studio/composer_panel.h"
#include "sound_mind/studio/create_project_wizard.h"
#include "sound_mind/studio/device_configuration_widget.h"
#include "sound_mind/studio/fill_gradient_dialog.h"
#include "sound_mind/studio/generator_dialog.h"
#include "sound_mind/studio/history_panel.h"
#include "sound_mind/studio/image_scale_picker_dialog.h"
#include "sound_mind/studio/import_export.h"
#include "sound_mind/studio/midi_import.h"
#include "sound_mind/studio/midi_import_dialog.h"
#include "sound_mind/studio/import_helpers.h"
#include "sound_mind/studio/landing_page.h"
#include "sound_mind/studio/layers_panel.h"
#include "sound_mind/studio/loop_panel.h"
#include "sound_mind/studio/mind_capture_dialog.h"
#include "sound_mind/studio/mind_waves_panel.h"
#include "sound_mind/studio/name_collision_dialog.h"
#include "sound_mind/studio/numeric_field.h"
#include "sound_mind/studio/playback_controller.h"
#include "sound_mind/studio/playback_panel.h"
#include "sound_mind/studio/qt_image_conversion.h"
#include "sound_mind/studio/record_panel.h"
#include "sound_mind/studio/resource_browser_controller.h"
#include "sound_mind/studio/resource_browser_panel.h"
#include "sound_mind/studio/theme.h"

#ifndef SOUND_MIND_VERSION
#define SOUND_MIND_VERSION "unknown"
#endif

namespace sound_mind::studio {

namespace {
const char* kProjectFileFilter = "Sound Mind Projects (*.smproj)";
const char* kAudioFileFilter = "Audio (*.wav *.mp3 *.flac *.ogg *.aiff *.aif *.m4a *.opus)";
const char* kImageFileFilter = "Images (*.png *.jpg *.jpeg *.bmp *.tga *.webp)";
const char* kMidiFileFilter = "MIDI Files (*.mid *.midi)";
const char* kExportAudioFileFilter = "FLAC Audio (*.flac);;Ogg Vorbis Audio (*.ogg);;MP3 Audio (*.mp3)";
const char* kExportVideoFileFilter = "MP4 Video (*.mp4)";

/// @brief The opacity step increaseSelectedLayerOpacity()/
/// decreaseSelectedLayerOpacity() apply per keypress - `v0.Y.46.1`
/// Installment A ("Layers Panel & Editing Enhancements v2"), confirmed
/// with the user.
constexpr float kLayerOpacityNudgeStep = 0.05f;

/// @brief How many `CurveGraph` nodes createResonanceFromPickedPath()
/// resamples a Picked path into - generous enough to let a reasonably
/// detailed hand-drawn curve's own real shape actually influence the
/// resulting spectrum (`computeWaveKernelSignature()`'s own eigenmode
/// count is bounded by this), while staying small enough for the dense
/// generalized eigensolve to stay effectively instant.
constexpr std::size_t kResonanceNodeCount = 128;

/// @brief How many samples createResonanceFromPickedPath() asks
/// `computeWaveKernelSignature()` for - the stored `NamedResonanceProfile::spectrum`'s
/// own resolution, generous enough for a smooth-looking profile without
/// an excessively large saved project file.
constexpr std::size_t kResonanceSpectrumSize = 64;

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

/// @brief Formats a cursor position for the status bar's own
/// cursorPositionLabel_ - both widget pixels and, when a project is open
/// (`domainPoint` has a value), the same point in time/frequency space.
/// Frequency switches from Hz to kHz above 1000 Hz purely for
/// readability - the underlying value is unaffected.
[[nodiscard]] QString formatCursorPosition(QPointF widgetPixel,
                                            std::optional<sound_mind::core::TimeFrequencyPoint> domainPoint) {
    QString text = QStringLiteral("%1, %2 px")
                        .arg(static_cast<int>(std::lround(widgetPixel.x())))
                        .arg(static_cast<int>(std::lround(widgetPixel.y())));
    if (domainPoint.has_value()) {
        const QString frequencyText = domainPoint->frequencyHz >= 1000.0
                                           ? QStringLiteral("%1 kHz").arg(domainPoint->frequencyHz / 1000.0, 0, 'f', 2)
                                           : QStringLiteral("%1 Hz").arg(domainPoint->frequencyHz, 0, 'f', 0);
        text += QStringLiteral("   |   %1 s, %2").arg(domainPoint->timeSeconds, 0, 'f', 3).arg(frequencyText);
    }
    return text;
}

/// @brief Shows `message` in `bar` and forces an immediate repaint.
///
/// Import/Export Audio are still synchronous, blocking calls -
/// without the explicit processEvents() call, Qt wouldn't actually paint
/// the status bar's new text until *after* the blocking call already
/// returned, defeating the whole point of showing progress before a slow
/// operation starts. Export Video is the first exception (real-world
/// testing pass, 2026-09-20, finding #12, Installment C) - see
/// exportCompositeVideoAsync()'s own docs for why it doesn't need this
/// helper at all: its own status bar message is shown right after actually
/// starting the background encode, by which point Qt's event loop is about
/// to run again on its own regardless.
void showBusyStatus(QStatusBar* bar, const QString& message) {
    bar->showMessage(message);
    QCoreApplication::processEvents();
}

/// @brief Converts a codec::RgbImage to a QImage, copying the pixel data
/// so the result stays valid independent of the source's own lifetime
/// (unlike CanvasWidget's paintEvent(), where the source stays alive for
/// the whole synchronous paint call and a copy would be wasted work).
[[nodiscard]] QImage toQImage(const sound_mind::codec::RgbImage& image) {
    return toQImageView(image).copy();
}

}  // namespace

MainWindow::MainWindow(QWidget* parent, sound_mind::core::AudioDeviceMode audioDeviceMode)
    : QMainWindow(parent),
      audioDeviceMode_(audioDeviceMode),
      recordEngine_(sound_mind::codec::StreamCodecConfig{}.sampleRateHz, audioDeviceMode),
      deviceTestRecordEngine_(sound_mind::codec::StreamCodecConfig{}.sampleRateHz, audioDeviceMode),
      deviceTestTonePlayer_(audioDeviceMode) {
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
    // v0.1.47.3: an edit under a heavy filter layer never stalls the window -
    // see CanvasWidget::setBackgroundCompositing().
    canvas_->setBackgroundCompositing(true);

    // Canvas Navigation's own Zoom feature (docs/sound-mind-design.md) -
    // canvas_ needs to be able to grow larger than the visible area once
    // zoomed in, with this QScrollArea clipping/scrolling it, instead of
    // sitting directly in stack_ and always being stretched to fill
    // whatever space is available (the pre-Zoom behavior, and still
    // exactly what setWidgetResizable(true) below reproduces while
    // canvas_'s own zoomMode() is FitToWindow).
    canvasScrollArea_ = new QScrollArea(this);
    canvasScrollArea_->setWidget(canvas_);
    canvasScrollArea_->setWidgetResizable(true);
    canvasScrollArea_->setAlignment(Qt::AlignCenter);
    connect(canvas_, &CanvasWidget::zoomModeChanged, this, [this](CanvasWidget::ZoomMode) { updateCanvasFit(); });
    // Image Mode's square-cell fit tracks the viewport's own size - see
    // updateCanvasFit().
    canvasScrollArea_->viewport()->installEventFilter(this);

    stack_ = new QStackedWidget(this);
    stack_->addWidget(landingPage_);      // index 0 - shown first, see setProject().
    stack_->addWidget(canvasScrollArea_);  // index 1
    setCentralWidget(stack_);

    layersPanel_ = new LayersPanel(this);
    layersPanel_->hide();  // nothing to show until setProject() - see refreshLayersPanel()'s docs.
    addDockWidget(Qt::RightDockWidgetArea, layersPanel_);
    connect(layersPanel_, &LayersPanel::visibilityCycleRequested, this, &MainWindow::cycleLayerVisibilityState);
    connect(layersPanel_, &LayersPanel::hideOtherLayersRequested, this, &MainWindow::hideOtherLayers);
    connect(layersPanel_, &LayersPanel::unhideOtherLayersRequested, this, &MainWindow::unhideOtherLayers);
    connect(layersPanel_, &LayersPanel::opacityChanged, this, &MainWindow::setLayerOpacity);
    connect(layersPanel_, &LayersPanel::balanceChanged, this, &MainWindow::setLayerBalance);
    connect(layersPanel_, &LayersPanel::translationChanged, this, &MainWindow::setLayerTranslation);
    connect(layersPanel_, &LayersPanel::rescaleChanged, this, &MainWindow::setLayerRescale);
    connect(layersPanel_, &LayersPanel::opacityMindWaveChanged, this, &MainWindow::setLayerOpacityMindWave);
    connect(layersPanel_, &LayersPanel::blendModeChanged, this, &MainWindow::setLayerBlendMode);
    connect(layersPanel_, &LayersPanel::renameRequested, this, &MainWindow::renameLayer);
    connect(layersPanel_, &LayersPanel::deleteRequested, this, &MainWindow::deleteLayer);
    connect(layersPanel_, &LayersPanel::duplicateRequested, this, &MainWindow::duplicateLayer);
    connect(layersPanel_, &LayersPanel::cleanUpPhaseRequested, this, &MainWindow::cleanUpLayerPhase);
    connect(layersPanel_, &LayersPanel::editFilterRequested, this, &MainWindow::editFilterLayer);
    connect(layersPanel_, &LayersPanel::reorderRequested, this, &MainWindow::reorderLayers);
    connect(layersPanel_, &LayersPanel::addLayerRequested, this, &MainWindow::addEmptyLayer);
    connect(layersPanel_, &LayersPanel::addFilterLayerRequested, this, &MainWindow::addFilterLayer);
    connect(layersPanel_, &LayersPanel::generateLayerRequested, this, &MainWindow::generateLayer);

    // The MindWave library management panel (v0.Y.31.1 Installment C2) -
    // hidden by default, the same "off until shown" convention
    // filterConfigurationPanel_ already follows (unlike layersPanel_'s own
    // special "shown automatically once" treatment - a MindWave library
    // is a much rarer, opt-in thing to touch than the layer stack every
    // project has from the start).
    mindWavesPanel_ = new MindWavesPanel(this);
    mindWavesPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, mindWavesPanel_);
    connect(mindWavesPanel_, &MindWavesPanel::addRequested, this, &MainWindow::addMindWave);
    connect(mindWavesPanel_, &MindWavesPanel::deleteRequested, this, &MainWindow::removeMindWave);
    connect(mindWavesPanel_, &MindWavesPanel::renameRequested, this, &MainWindow::renameMindWave);
    connect(mindWavesPanel_, &MindWavesPanel::mindWaveChanged, this, &MainWindow::updateMindWave);

    // The Resource Browser panel (docs/sound-mind-roadmap.md's v0.Y.57.1
    // Installment C, folding in the roadmap backlog's own "resource
    // browser panel"/"SoundMind Toolkit collections" entries) - hidden by
    // default, the same reasoning mindWavesPanel_ above already gives.
    // ResourceBrowserController (constructed later, once every panel it
    // might need to react through already exists) owns its actual
    // behavior - this panel is purely presentational.
    resourceBrowserPanel_ = new ResourceBrowserPanel(this);
    resourceBrowserPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, resourceBrowserPanel_);

    // The History Panel (v0.Y.46.1 Installment D, "Layers Panel & Editing
    // Enhancements v2") - hidden by default, the same reasoning
    // mindWavesPanel_ above already gives.
    historyPanel_ = new HistoryPanel(this);
    historyPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, historyPanel_);
    connect(historyPanel_, &HistoryPanel::jumpRequested, this, &MainWindow::jumpToHistoryIndex);
    // The project's History (v0.1.43.1): the panel follows every change to
    // it; an undo/redo/jump refreshes what the controllers don't.
    connect(&history_, &ProjectHistory::historyChanged, this, &MainWindow::refreshHistoryPanel);
    connect(&history_, &ProjectHistory::entryCommitted, this, [this]() { hasUnsavedChanges_ = true; });
    connect(&history_, &ProjectHistory::changed, this, &MainWindow::handleHistoryChange);

    // Composer Mode (v0.Y.48.1 Installment A) - bottom-docked, not
    // right-docked, confirmed with the user (see composerPanel_'s own
    // docs); hidden by default, the same reasoning historyPanel_ above
    // already gives.
    composerPanel_ = new ComposerPanel(this);
    composerPanel_->hide();
    addDockWidget(Qt::BottomDockWidgetArea, composerPanel_);

    // Playback/Record/Loop each get their own dockable panel (v0.Y.16.1) -
    // hidden until setProject(), matching layersPanel_'s own "nothing to
    // control yet" treatment, even though playbackController_/recordEngine_
    // themselves exist regardless of project state.
    playbackPanel_ = new PlaybackPanel(this);
    playbackPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, playbackPanel_);
    connect(playbackPanel_, &PlaybackPanel::playRequested, this, &MainWindow::startPlayback);
    connect(playbackPanel_, &PlaybackPanel::pauseRequested, this, &MainWindow::pausePlayback);
    connect(playbackPanel_, &PlaybackPanel::stopRequested, this, &MainWindow::stopPlayback);
    connect(playbackPanel_, &PlaybackPanel::seekRequested, this, &MainWindow::seekPlayback);

    // Extracted as its own class (v0.Y.23.1, Refactor & Clean Up) - see its
    // own docs. MainWindow's job is just wiring its signals to whatever
    // needs to reflect them: durationChanged() -> playbackPanel_'s own
    // slot directly (an exact signature match, no lambda needed);
    // positionChanged() -> a lambda, since the canvas playhead needs a
    // *fraction* (position/total), not the raw position alone.
    playbackController_ = new PlaybackController(this, audioDeviceMode_);
    connect(playbackController_, &PlaybackController::durationChanged, playbackPanel_, &PlaybackPanel::setDuration);
    connect(playbackController_, &PlaybackController::positionChanged, this, [this](double positionSeconds) {
        if (macroPlayer_ != nullptr && macroPlayer_->isActive()) {
            return;  // a performance is playing, not the project - MacroPlayer shows it.
        }
        playbackPanel_->setPositionSeconds(positionSeconds);
        currentPlaybackPositionSeconds_ = positionSeconds;
        const double total = playbackController_->totalSeconds();
        canvas_->setPlayheadFraction(total > 0.0 ? std::optional<double>(positionSeconds / total) : std::nullopt);
        // Layers Panel's own per-layer loudness indicator (v0.Y.52.1,
        // Analysis Tools v1) - "live during Playback/Loop, the value at the
        // playback position when paused" (both share this one call - see
        // updateLoudnessDisplays()'s own docs on why pausing needs nothing
        // further here).
        layerController_->updateLoudnessDisplays(total > 0.0 ? positionSeconds / total : 0.0);
        analysisPanel_->setPlaybackPosition(positionSeconds);
        noteMacroPlayhead();
    });
    connect(playbackPanel_, &PlaybackPanel::repeatChanged, this, &MainWindow::setPlaybackRepeat);
    connect(playbackPanel_, &PlaybackPanel::scopeChanged, this, &MainWindow::setPlaybackScope);
    connect(playbackPanel_, &PlaybackPanel::configureDevicesRequested, this, &MainWindow::showConfigureDevicesPanel);

    toolConfigurationPanel_ = new ToolConfigurationPanel(this);
    toolConfigurationPanel_->setHistory(&history_);
    toolConfigurationPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, toolConfigurationPanel_);

    // MIDI Configuration panel (v0.Y.55.1's own last installment) - see its
    // own class docs on why it mutates project_ directly, the same
    // exception toolConfigurationPanel_'s own Tool Preset combo already
    // establishes.
    midiConfigurationPanel_ = new MidiConfigurationPanel(this);
    midiConfigurationPanel_->setHistory(&history_);
    midiConfigurationPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, midiConfigurationPanel_);
    // Refreshes each row's own Tool Preset combo whenever the panel is
    // actually shown, rather than needing a dedicated signal from
    // toolConfigurationPanel_'s own Save/Delete actions - simpler, and
    // this panel's own Tool Preset choices are only ever looked at while
    // it's visible anyway.
    connect(midiConfigurationPanel_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible) {
            midiConfigurationPanel_->refreshToolPresets();
        }
    });

    // Chords/Arpeggiator/Sequencer, Installment B (v0.0.40.2) - see
    // ChordGeneratorPanel's own docs. toolPaletteController_ isn't
    // constructed until just below, but this lambda only ever runs later,
    // on a real control edit - safe by the time it fires, the same
    // reasoning selectionConfigurationPanel_'s own lambdas below rely on.
    chordGeneratorPanel_ = new ChordGeneratorPanel(this);
    chordGeneratorPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, chordGeneratorPanel_);
    // Real-world testing pass, 2026-09-20, finding #15 - see
    // CanvasWidget::setChordGeneratorPanelVisible()'s own docs.
    connect(chordGeneratorPanel_, &ChordGeneratorPanel::visibilityChanged, canvas_,
            &CanvasWidget::setChordGeneratorPanelVisible);
    connect(chordGeneratorPanel_, &ChordGeneratorPanel::paramsChanged, this,
            [this](const sound_mind::core::ChordGeneratorParams& params) {
                toolPaletteController_->setChordParams(params);
            });
    // Chords/Arpeggiator/Sequencer, Installment C (v0.0.40.3) - the Custom
    // Notation half of the same panel; see ChordGeneratorPanel's own docs.
    connect(chordGeneratorPanel_, &ChordGeneratorPanel::notationChanged, this,
            [this](const QString& notation, double referenceHz, double bpm) {
                toolPaletteController_->setChordNotation(notation.toStdString(), referenceHz, bpm);
            });

    // Workflow & Device Polish, Installment A (v0.0.42.1) - see
    // ConfigureDevicesPanel's own class docs.
    configureDevicesPanel_ = new ConfigureDevicesPanel(this);
    configureDevicesPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, configureDevicesPanel_);
    connect(configureDevicesPanel_, &ConfigureDevicesPanel::refreshRequested, this,
            &MainWindow::refreshConfiguredDevices);
    connect(configureDevicesPanel_, &ConfigureDevicesPanel::inputDeviceChanged, this,
            &MainWindow::setConfiguredInputDevice);
    connect(configureDevicesPanel_, &ConfigureDevicesPanel::outputDeviceChanged, this,
            &MainWindow::setConfiguredOutputDevice);
    connect(configureDevicesPanel_, &ConfigureDevicesPanel::inputGainPercentChanged, this,
            &MainWindow::setConfiguredInputGain);
    connect(configureDevicesPanel_, &ConfigureDevicesPanel::outputGainPercentChanged, this,
            &MainWindow::setConfiguredOutputGain);
    connect(configureDevicesPanel_, &ConfigureDevicesPanel::testInputToggled, this,
            &MainWindow::toggleTestInputDevice);
    connect(configureDevicesPanel_, &ConfigureDevicesPanel::testOutputToggled, this,
            &MainWindow::toggleTestOutputDevice);
    connect(configureDevicesPanel_, &ConfigureDevicesPanel::masterLimiterChanged, this, &MainWindow::setMasterLimiter);
    connect(configureDevicesPanel_, &ConfigureDevicesPanel::loudnessNormalizationChanged, this,
            &MainWindow::setLoudnessNormalization);

    // v0.Y.62.1 Installment H - the Landing Page's own embedded
    // DeviceConfigurationWidget is a second, independent instance of the
    // exact same controls (see its own docs); wired identically to the
    // dock's own, so either one can be used interchangeably and both stay
    // in sync. refreshConfiguredDevices() (below) is what actually keeps
    // both populated with the same device list from here on - this is
    // just the first population plus the same signal forwarding the dock
    // already has.
    auto* landingDeviceConfig = landingPage_->deviceConfiguration();
    connect(landingDeviceConfig, &DeviceConfigurationWidget::refreshRequested, this,
            &MainWindow::refreshConfiguredDevices);
    connect(landingDeviceConfig, &DeviceConfigurationWidget::inputDeviceChanged, this,
            &MainWindow::setConfiguredInputDevice);
    connect(landingDeviceConfig, &DeviceConfigurationWidget::outputDeviceChanged, this,
            &MainWindow::setConfiguredOutputDevice);
    connect(landingDeviceConfig, &DeviceConfigurationWidget::inputGainPercentChanged, this,
            &MainWindow::setConfiguredInputGain);
    connect(landingDeviceConfig, &DeviceConfigurationWidget::outputGainPercentChanged, this,
            &MainWindow::setConfiguredOutputGain);
    connect(landingDeviceConfig, &DeviceConfigurationWidget::testInputToggled, this,
            &MainWindow::toggleTestInputDevice);
    connect(landingDeviceConfig, &DeviceConfigurationWidget::testOutputToggled, this,
            &MainWindow::toggleTestOutputDevice);
    connect(landingDeviceConfig, &DeviceConfigurationWidget::masterLimiterChanged, this, &MainWindow::setMasterLimiter);
    connect(landingDeviceConfig, &DeviceConfigurationWidget::loudnessNormalizationChanged, this,
            &MainWindow::setLoudnessNormalization);
    // Output processing (v0.Y.80.1), remembered across sessions.
    setMasterLimiter(settings_.value(QStringLiteral("masterLimiterEnabled"), true).toBool(),
                     settings_.value(QStringLiteral("masterLimiterCeilingDb"), -1.0).toDouble());
    setLoudnessNormalization(settings_.value(QStringLiteral("loudnessNormalizationEnabled"), false).toBool(),
                             settings_.value(QStringLiteral("loudnessTargetLufs"), -14.0).toDouble());

    refreshConfiguredDevices();

    testInputLevelTimer_ = new QTimer(this);
    testInputLevelTimer_->setInterval(100);  // keeps its ring buffer from filling, as Record's drain does.
    connect(testInputLevelTimer_, &QTimer::timeout, this, &MainWindow::pollTestInputLevel);

    // Exports (finding #12's cancellable long operations) live in their own
    // controller since v0.1.48.2 - see ExportController.
    exportController_ = std::make_unique<ExportController>(
        ExportController::Context{
            [this]() -> const sound_mind::core::Project* { return project_ ? &*project_ : nullptr; },
            [this]() {
                return limiterEnabled_ ? std::optional<float>(static_cast<float>(limiterCeilingDb_)) : std::nullopt;
            },
            [this](sound_mind::core::MacroId id, QString* error) { return macroRendererFor(id, error); },
            [this](const QString& title, const QString& message) { QMessageBox::critical(this, title, message); }},
        statusBar());

    // Finding #12, Installment F - see importAudioSnippetsAsync()'s own
    // docs. A separate slot from export's own, not shared - unlike
    // Video/Audio export (both reading the same topmost layer, never
    // meaningfully run together), importing new content and exporting
    // existing content are independent operations with no reason to block
    // each other.
    // Imports live in their own controller since v0.1.48.3 - see ImportController.
    importController_ = std::make_unique<ImportController>(
        ImportController::Context{
            [this]() -> sound_mind::core::Project* { return project_ ? &*project_ : nullptr; },
            [this](const QString& description, nlohmann::json metadata) {
                if (project_) {
                    history_.commit(*project_, description, std::move(metadata));
                }
            },
            [this](sound_mind::core::LayerId layer) { toolPaletteController_->rebuildLayerContent(layer); },
            [this](const ProjectChange& change) { applyProjectChange(change); },
            [this](const QString& title, const QString& message) { QMessageBox::critical(this, title, message); }},
        statusBar());

    // Finding #12, Installment H - see startPlayback()'s own docs. A
    // third, independent slot - not shared with export's/import's own.
    // Preparing audio for Playback, Repeat and Delta/Review lives in its own
    // controller since v0.1.48.4 - see PlaybackPreparer.
    playbackPreparer_ = std::make_unique<PlaybackPreparer>(
        PlaybackPreparer::Context{
            [this]() -> const sound_mind::core::Project* { return project_ ? &*project_ : nullptr; },
            [this]() { return currentPlaybackPositionSeconds_; },
            [this](double lufs) { noteLoadedPlaybackLoudness(lufs); },
            [this]() { noteMacroPlayhead(); },
            [this](const QString& title, const QString& message) { QMessageBox::critical(this, title, message); }},
        playbackController_, statusBar());
    filterPreviewJob_ = std::make_unique<BackgroundJob>();
    connect(playbackController_, &PlaybackController::invalidated, this, &MainWindow::scheduleRepeatRefresh);
    // Anything that changes what's heard also changes what the analysis
    // measured (v0.1.40.10) - one hook for every such edit, including ones
    // that don't emit layersChanged()/contentChanged(), like MindWave edits.
    connect(playbackController_, &PlaybackController::invalidated, this, &MainWindow::noteProjectChangedForAnalysis);
    connect(playbackController_, &PlaybackController::invalidated, this, [this]() {
        if (loopController_) {
            loopController_->projectEdited();  // a running loop plays along with the new audio.
        }
    });

    // Selection & Fill (v0.Y.25.1), Selection Type (v0.Y.35.1 Installment
    // A) - toolPaletteController_ isn't constructed until just below, but
    // this lambda only ever runs later, on a real dropdown change - safe
    // by the time it fires, the same reasoning every other lambda in this
    // constructor relies on.
    selectionConfigurationPanel_ = new SelectionConfigurationPanel(this);
    selectionConfigurationPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, selectionConfigurationPanel_);

    // The Text tool's panel (v0.Y.78.1) - wired to its controller once
    // toolPaletteController_ exists, below.
    textToolPanel_ = new TextToolPanel(this);
    textToolPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, textToolPanel_);
    connect(selectionConfigurationPanel_, &SelectionConfigurationPanel::selectionShapeChanged, this,
            [this](SelectionShape shape) { toolPaletteController_->setSelectionShape(shape); });
    connect(selectionConfigurationPanel_, &SelectionConfigurationPanel::wandToleranceChanged, this,
            [this](double tolerancePercent) { toolPaletteController_->setWandTolerance(tolerancePercent); });
    connect(selectionConfigurationPanel_, &SelectionConfigurationPanel::wandHarmonicsAwareChanged, this,
            [this](bool harmonicsAware) { toolPaletteController_->setWandHarmonicsAware(harmonicsAware); });
    // Real-world testing pass, 2026-09-20, finding #5: also applies live to
    // whatever's currently Picked, if it's a pasted region -
    // ToolPaletteController::applyPasteBlendMode() itself no-ops otherwise.
    connect(selectionConfigurationPanel_, &SelectionConfigurationPanel::pasteBlendModeChanged, this,
            [this](sound_mind::core::BlendMode mode) { toolPaletteController_->applyPasteBlendMode(mode); });
    connect(selectionConfigurationPanel_, &SelectionConfigurationPanel::deselectRequested, this,
            &MainWindow::deselect);
    // Mind Grain ordering-rule guardrail (v0.Y.33.1 Installment B) - a type
    // switch, a different Mind Grain picked, or any other edit could change
    // whether the active layer is currently paintable with it. layerController_/
    // paintAction_ aren't constructed yet at this exact point in the
    // constructor, but this lambda only ever runs later, on a real signal -
    // safe by the time either one fires, the same reasoning
    // toolPaletteController_'s own lambdas just above rely on.
    connect(toolConfigurationPanel_, &ToolConfigurationPanel::toolConfigurationChanged, this,
            [this](const sound_mind::core::ToolConfiguration&) { updateMindGrainGuardrails(); });
    // Audio preview (direct user feedback: "practically wherever there
    // is a visual preview of something, give the user the ability to
    // play an audio preview of whatever it is") - the live counterpart
    // to the Resource Browser's own Tool Preset preview: the same
    // representative-stroke synthesis (sound_mind::core::
    // toolConfigurationPreviewStreamImage()), just fed this panel's own
    // current, possibly-unsaved configuration instead of a saved
    // preset's.
    connect(toolConfigurationPanel_, &ToolConfigurationPanel::previewRequested, this,
            &MainWindow::previewToolConfiguration);
    connect(toolConfigurationPanel_, &ToolConfigurationPanel::stopPreviewRequested, this,
            &MainWindow::stopToolConfigurationPreview);

    // Basic Painting/Pick/Selection & Fill/Paths & Grids (Phase 3,
    // v0.Y.24.1-v0.Y.26.1) - extracted as its own class (Refactor & Clean
    // Up, v0.Y.29.1, Installment C); see its own docs. Owns the four tool
    // controllers and all of their wiring to canvas_/
    // toolConfigurationPanel_ internally. This class's own remaining job:
    // resolving "which layer" a freehand gesture targets
    // (layerController_->paintTargetLayerId() - Installment D) and
    // forwarding it to whichever begin*() call applies - a
    // LayersPanel-selection concept toolPaletteController_ has no reason
    // to know about. layerController_ itself isn't constructed until
    // later in this same constructor, but these lambdas only run later
    // still, on a real gesture - safe by the time any of them fire.
    toolPaletteController_ = new ToolPaletteController(canvas_, toolConfigurationPanel_, &history_, this);
    // chordGeneratorPanel_'s own constructor already emitted paramsChanged()
    // once, before the connection above existed - this explicit initial
    // sync is the same "the panel's own constructed-with defaults are
    // already real, applied here" precedent ToolPaletteController's own
    // constructor follows for toolConfigurationPanel_ (see its own docs),
    // just done here instead since chordGeneratorPanel_ isn't passed into
    // ToolPaletteController's constructor at all (see that class's own docs
    // on why).
    toolPaletteController_->setChordParams(chordGeneratorPanel_->params());
    {
        TextController* text = toolPaletteController_->textController();
        text->setSettings(textToolPanel_->settings());
        connect(textToolPanel_, &TextToolPanel::settingsChanged, text, &TextController::setSettings);
        connect(textToolPanel_, &TextToolPanel::applyRequested, this, [text]() { text->apply(); });
        connect(textToolPanel_, &TextToolPanel::resetRequested, text, &TextController::reset);
        connect(text, &TextController::previewChanged, this,
                [this, text]() { textToolPanel_->setBoxPlaced(text->box().has_value()); });
        connect(canvas_, &CanvasWidget::textBoxStarted, this, [this, text](sound_mind::core::TimeFrequencyPoint point) {
            if (const auto layerId = layerController_->paintTargetLayerId(); layerId.has_value()) {
                text->beginBox(*layerId, point);
            }
        });
    }
    // Real-world testing pass, 2026-09-20, finding #5: pre-fills Selection
    // Configuration's own Paste Blend Mode combo with a newly-Picked pasted
    // region's own real value, ready to reopen and adjust - the same
    // "reopen and adjust" precedent ToolPaletteController's own internal
    // ToolConfigurationPanel pre-fill already establishes for a picked
    // PaintOperation. Left showing whatever it last displayed when nothing
    // suitable is Picked, matching that same precedent.
    connect(toolPaletteController_, &ToolPaletteController::pickSelectionChanged, this, [this]() {
        if (const auto mode = toolPaletteController_->selectedPasteBlendMode(); mode.has_value()) {
            selectionConfigurationPanel_->setPasteBlendMode(*mode);
        }
        // Picked applied text (v0.1.40.13): its settings fill the Text
        // panel, where changing them edits it - like a Picked stroke and
        // the Tool Configuration panel.
        const auto text = toolPaletteController_->selectedTextSettings();
        if (!pendingPickedTextSettings_.has_value()) {
            if (text.has_value()) {
                textToolPanel_->setSettings(*text);
                if (!textToolPanel_->isEditingPickedText()) {
                    textToolPanel_->show();
                    textToolPanel_->raise();
                }
            }
            textToolPanel_->setEditingPickedText(text.has_value() && !textAction_->isChecked());
        }
    });
    pickedTextEditTimer_ = new QTimer(this);
    pickedTextEditTimer_->setSingleShot(true);
    pickedTextEditTimer_->setInterval(kPickedTextEditDelayMs);
    connect(pickedTextEditTimer_, &QTimer::timeout, this, &MainWindow::flushPickedTextEdit);
    connect(textToolPanel_, &TextToolPanel::settingsChanged, this, [this](const TextSettings& settings) {
        if (textToolPanel_->isEditingPickedText()) {
            pendingPickedTextSettings_ = settings;
            pickedTextEditTimer_->start();  // restarts the pause on each change.
        }
    });
    connect(canvas_, &CanvasWidget::paintStrokeStarted, this, [this](sound_mind::core::TimeFrequencyPoint point) {
        if (const auto layerId = layerController_->paintTargetLayerId(); layerId.has_value()) {
            toolPaletteController_->beginPaintStroke(*layerId, point);
        }
    });
    connect(canvas_, &CanvasWidget::pickStrokeStarted, this, [this](sound_mind::core::TimeFrequencyPoint point) {
        if (const auto layerId = layerController_->paintTargetLayerId(); layerId.has_value()) {
            toolPaletteController_->beginPick(*layerId, point);
            noteBranchCurveGraftCandidate(point);
        }
    });
    connect(canvas_, &CanvasWidget::selectStrokeStarted, this,
            [this](sound_mind::core::TimeFrequencyPoint point, Qt::KeyboardModifiers modifiers) {
                if (const auto layerId = layerController_->paintTargetLayerId(); layerId.has_value()) {
                    // Shift = Add, Alt = Subtract, Shift+Alt = Intersect -
                    // the universal boolean-combination convention,
                    // confirmed with the user: no real conflict with this
                    // project's own Alt/Shift/Ctrl axis-restriction
                    // mnemonic (Canvas Navigation's own docs), which is
                    // scoped to *resizing something that already exists*,
                    // not drawing a brand-new selection from scratch.
                    const bool shift = modifiers.testFlag(Qt::ShiftModifier);
                    const bool alt = modifiers.testFlag(Qt::AltModifier);
                    const SelectionCombineMode combineMode = (shift && alt)   ? SelectionCombineMode::Intersect
                                                              : shift         ? SelectionCombineMode::Add
                                                              : alt           ? SelectionCombineMode::Subtract
                                                                              : SelectionCombineMode::Replace;
                    toolPaletteController_->setSelectionCombineMode(combineMode);
                    toolPaletteController_->beginSelectionDrag(*layerId, point);
                }
            });
    // Rectangle's own rotate handle (v0.Y.35.1 Installment C) - a
    // separate gesture from selectStrokeStarted() above (it transforms
    // an *existing* committed selection, needing no target-layer
    // resolution at all - SelectionController already knows which layer
    // its own committed selection belongs to).
    connect(canvas_, &CanvasWidget::selectionRotateStarted, this,
            [this](sound_mind::core::TimeFrequencyPoint point) { toolPaletteController_->beginRotateDrag(point); });
    connect(canvas_, &CanvasWidget::selectionRotateContinued, this, [this](sound_mind::core::TimeFrequencyPoint point) {
        toolPaletteController_->continueRotateDrag(point);
    });
    connect(canvas_, &CanvasWidget::selectionRotateEnded, this,
            [this]() { toolPaletteController_->endRotateDrag(); });
    // The Path tool draws a silent guide path freehand (v0.Y.77.1) - the
    // same Started/Continued/Ended drag shape as Paint.
    connect(canvas_, &CanvasWidget::pathStrokeStarted, this, [this](sound_mind::core::TimeFrequencyPoint point) {
        if (const auto layerId = layerController_->paintTargetLayerId(); layerId.has_value()) {
            toolPaletteController_->beginPathStroke(*layerId, point);
        }
    });
    connect(canvas_, &CanvasWidget::pathStrokeContinued, this, [this](sound_mind::core::TimeFrequencyPoint point) {
        toolPaletteController_->continuePathStroke(point);
    });
    connect(canvas_, &CanvasWidget::pathStrokeEnded, this, [this]() { toolPaletteController_->endPathStroke(); });
    // Chords/Arpeggiator/Sequencer, Installment B (v0.0.40.2) - only
    // point.timeSeconds is used (see CanvasWidget::chordStampRequested()'s
    // own docs on why point.frequencyHz is deliberately ignored: a chord's
    // own pitches already come entirely from the Chord Generator panel's
    // Root/Octave controls).
    connect(canvas_, &CanvasWidget::chordStampRequested, this, [this](sound_mind::core::TimeFrequencyPoint point) {
        if (const auto layerId = layerController_->paintTargetLayerId(); layerId.has_value()) {
            toolPaletteController_->stampChord(*layerId, point.timeSeconds);
        }
    });
    connect(canvas_, &CanvasWidget::contextMenuRequested, this, &MainWindow::showCanvasContextMenu);
    // Merges all four tool controllers' own contentChanged() into one
    // connection - see ToolPaletteController::contentChanged()'s own
    // docs; it has already called canvas_->update() itself by this point.
    connect(toolPaletteController_, &ToolPaletteController::contentChanged, this,
            [this](sound_mind::core::LayerId layer) {
                // One layer's content changed (paint, fill, cut, paste,
                // filter to selection, chord stamps, path strokes): its
                // composite from here up, its thumbnail, the audio - see
                // applyProjectChange(). Playback is invalidated first, as
                // handleContentChangedForPlayback() may load fresh audio.
                if (project_.has_value()) {
                    if (const auto index = project_->layerIndexById(layer)) {
                        applyProjectChange(ProjectChange::layerContent(*index, layer));
                    }
                }
                handleContentChangedForPlayback(layer);
            });

    gridPanel_ = new GridPanel(this);
    gridPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, gridPanel_);
    connect(gridPanel_, &GridPanel::verticalAxisLabelModeChanged, canvas_, &CanvasWidget::setVerticalAxisLabelMode);
    connect(gridPanel_, &GridPanel::horizontalAxisLabelModeChanged, canvas_,
            &CanvasWidget::setHorizontalAxisLabelMode);
    connect(gridPanel_, &GridPanel::frequencyGridConfigChanged, canvas_, &CanvasWidget::setFrequencyGridConfig);
    connect(gridPanel_, &GridPanel::timingGridConfigChanged, canvas_, &CanvasWidget::setTimingGridConfig);
    // Snap to Grid (see PickController::setGridSnapping()'s/
    // SelectionController::setGridSnapping()'s own docs) needs the
    // panel's own current enabled flag *and* both grid configurations
    // together, regardless of which one just changed - a single shared
    // slot re-reads all three off gridPanel_ itself and re-applies them
    // to both controllers, rather than three separate slots each only
    // updating one piece of state the other two calls already hold.
    connect(gridPanel_, &GridPanel::snapToGridChanged, this, &MainWindow::applyGridSnapping);
    connect(gridPanel_, &GridPanel::frequencyGridConfigChanged, this, &MainWindow::applyGridSnapping);
    connect(gridPanel_, &GridPanel::timingGridConfigChanged, this, &MainWindow::applyGridSnapping);
    // Grid Preset save/delete (direct user feedback: "give the user the
    // option to save/load named grid presets, just like tool
    // configuration presets") - GridPanel holds no Project* of its own
    // (see that class's own docs), so MainWindow owns both.
    connect(gridPanel_, &GridPanel::savePresetRequested, this, &MainWindow::saveGridPreset);
    connect(gridPanel_, &GridPanel::deletePresetRequested, this, &MainWindow::deleteGridPreset);

    filterConfigurationPanel_ = new FilterConfigurationPanel(this);
    filterConfigurationPanel_->hide();
    // Nothing selected yet - see handleLayerSelectionChanged()'s own docs.
    filterConfigurationPanel_->setEnabled(false);
    addDockWidget(Qt::RightDockWidgetArea, filterConfigurationPanel_);
    connect(layersPanel_, &LayersPanel::selectionChanged, this, &MainWindow::handleLayerSelectionChanged);
    connect(filterConfigurationPanel_, &FilterConfigurationPanel::filterConfigurationChanged, this,
            &MainWindow::applyFilterConfiguration);
    connect(filterConfigurationPanel_, &FilterConfigurationPanel::saveConvolutionKernelRequested, this,
            &MainWindow::saveConvolutionKernel);
    connect(filterConfigurationPanel_, &FilterConfigurationPanel::savePresetRequested, this,
            &MainWindow::saveFilterPreset);
    connect(filterConfigurationPanel_, &FilterConfigurationPanel::equalizerPreviewToggled, this,
            &MainWindow::setEqualizerPreviewEnabled);
    connect(filterConfigurationPanel_, &FilterConfigurationPanel::previewToggled, this,
            &MainWindow::setFilterPreviewEnabled);

    // Layer-stack lookup/mutation and Layers/Filter Configuration Panel
    // refresh - extracted as its own class (Refactor & Clean Up,
    // v0.Y.29.1, Installment D); see its own docs.
    layerController_ =
        new LayerController(canvas_, playbackController_, layersPanel_, filterConfigurationPanel_, &history_, this);
    connect(layerController_, &LayerController::layersChanged, this,
            [this]() { applyProjectChange(ProjectChange::layerProperties()); });
    connect(layerController_, &LayerController::layerRemoved, this,
            [this](sound_mind::core::LayerId id) { toolPaletteController_->forgetLayer(id); });
    layerController_->setOperationDeleter([this](const std::vector<sound_mind::core::OperationId>& ids) {
        toolPaletteController_->deleteOperations(ids, tr("Deleted dependent Mind Grain strokes"));
    });

    // The MindWave library itself - add/remove/rename/edit - and keeping
    // mindWavesPanel_/layersPanel_'s own opacity-binding combo in sync
    // with it (v0.Y.31.1 Installment C2); see its own class docs.
    mindWaveController_ = new MindWaveController(mindWavesPanel_, layersPanel_, filterConfigurationPanel_,
                                                  toolConfigurationPanel_, canvas_, &history_, this);
    connect(mindWaveController_, &MindWaveController::mindWavesChanged, this,
            [this]() { applyProjectChange(ProjectChange::resources()); });
    // v0.1.47.4: only what uses the edited MindWave is redone.
    connect(mindWaveController_, &MindWaveController::mindWaveEdited, this, &MainWindow::handleMindWaveEdited);

    // The Resource Browser panel's own Project reads/mutations
    // (docs/sound-mind-roadmap.md's v0.Y.57.1 Installment C) - see its own
    // class docs.
    resourceBrowserController_ = new ResourceBrowserController(resourceBrowserPanel_, this);
    connect(resourceBrowserController_, &ResourceBrowserController::resourcesChanged, this, [this]() {
        commitHistory(tr("Changed resources"));
        applyProjectChange(ProjectChange::resources());
    });

    // "Create New MindWave..." (v0.Y.62.1 Installment G) - one shared
    // callback, injected into every MindWave-binding dropdown's own
    // owning panel: creates the entry (also showing/raising the
    // MindWaves panel - see MindWaveController::addMindWave()'s own
    // docs) and looks up its freshly-assigned display name, so the
    // combo that asked for it can show something meaningful rather than
    // a bare id.
    const auto createMindWaveCallback = [this]() -> std::pair<sound_mind::core::MindWaveId, QString> {
        const sound_mind::core::MindWaveId id = mindWaveController_->addMindWave();
        if (id == sound_mind::core::MindWaveId{0} || !project_.has_value()) {
            return {id, QString()};
        }
        const auto* named = project_->mindWaveById(id);
        return {id, named != nullptr ? QString::fromStdString(named->name) : QString()};
    };
    toolConfigurationPanel_->setCreateMindWaveCallback(createMindWaveCallback);
    filterConfigurationPanel_->setCreateMindWaveCallback(createMindWaveCallback);
    layersPanel_->setCreateMindWaveCallback(createMindWaveCallback);

    // A permanent (not showMessage()'s own temporary-message) label in the
    // status bar's normal (left-hand) area - see cursorPositionLabel_'s
    // own docs for why a temporary status message can still cover it
    // briefly, and why that's an accepted tradeoff rather than a bug.
    cursorPositionLabel_ = new QLabel(this);
    cursorPositionLabel_->setObjectName(QStringLiteral("cursorPositionLabel"));
    statusBar()->addWidget(cursorPositionLabel_);
    connect(canvas_, &CanvasWidget::cursorMoved, this,
            [this](QPointF widgetPixel, std::optional<sound_mind::core::TimeFrequencyPoint> domainPoint) {
                cursorPositionLabel_->setText(formatCursorPosition(widgetPixel, domainPoint));
            });
    connect(canvas_, &CanvasWidget::cursorLeft, this, [this]() { cursorPositionLabel_->clear(); });

    recordPanel_ = new RecordPanel(this);
    recordPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, recordPanel_);
    // Start/Stop and Live-encoding view are wired by recordingController_
    // (v0.1.48.6), made once the canvas exists - see below.
    connect(recordPanel_, &RecordPanel::configureDevicesRequested, this, &MainWindow::showConfigureDevicesPanel);

    // The Analysis panel (v0.Y.80.1) - see analyzeLoudness().
    analysisPanel_ = new AnalysisPanel(this);
    analysisPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, analysisPanel_);
    connect(analysisPanel_, &AnalysisPanel::pitchOverlayToggled, this,
            [this](bool, const std::vector<std::vector<float>>& tracks) { canvas_->setPitchOverlay(tracks); });
    // The canvas's horizontal mouse position feeds the Pitch tab's "At mouse" (v0.1.40.11).
    connect(canvas_, &CanvasWidget::cursorMoved, this,
            [this](QPointF, std::optional<sound_mind::core::TimeFrequencyPoint> point) {
                analysisPanel_->setMouseTime(point.has_value() ? std::optional<double>(point->timeSeconds)
                                                               : std::nullopt);
            });
    connect(canvas_, &CanvasWidget::cursorLeft, this, [this]() { analysisPanel_->setMouseTime(std::nullopt); });
    connect(analysisPanel_, &AnalysisPanel::loudnessOverlayToggled, this,
            [this](bool, const std::vector<float>& momentary, const std::vector<float>& shortTerm) {
                canvas_->setLoudnessOverlay(momentary, shortTerm, sound_mind::core::kLoudnessStepSeconds);
            });
    connect(analysisPanel_, &AnalysisPanel::stereoOverlayToggled, this,
            [this](bool, const std::vector<float>& correlation, const std::vector<float>& width) {
                canvas_->setStereoOverlay(correlation, width, sound_mind::core::kLoudnessStepSeconds);
            });
    // Analyze, Auto-update and the report's settings live in their own
    // controller since v0.1.48.5 - see AnalysisController. (The overlays
    // above draw on the canvas, so they stay here.)
    analysisController_ = std::make_unique<AnalysisController>(
        analysisPanel_, [this]() -> const sound_mind::core::Project* { return project_ ? &*project_ : nullptr; },
        &settings_);

    loopPanel_ = new LoopPanel(this);
    loopPanel_->hide();  // also needs setProject() - there's no Loop engine until then.
    addDockWidget(Qt::RightDockWidgetArea, loopPanel_);
    // Start/Stop, Keep Looping and Live-encoding view, the backing audio and
    // the takes live in their own controller since v0.1.48.7 - see
    // LoopController.
    loopController_ = std::make_unique<LoopController>(
        LoopController::Context{
            [this]() -> sound_mind::core::Project* { return project_ ? &*project_ : nullptr; },
            [this]() { return !recordEngine_.isRecording(); },
            [this]() { stopPlayback(); },
            [this]() { updateConfiguredDeviceLockState(); },
            [this](std::optional<sound_mind::codec::StreamImage> overlay) { canvas_->setLiveOverlay(std::move(overlay)); },
            [this](std::optional<double> fraction) { canvas_->setPlayheadFraction(fraction); },
            [this](const QString& description) { commitHistory(description); },
            [this](const ProjectChange& change) { applyProjectChange(change); },
            [this](sound_mind::core::LayerId layer) {
                if (const auto index = project_ ? project_->layerIndexById(layer) : std::nullopt) {
                    canvas_->invalidateCompositeFrom(*index);
                }
                layerController_->refreshLayersPanel(layer);
            }},
        loopPanel_, statusBar());
    connect(loopPanel_, &LoopPanel::configureDevicesRequested, this, &MainWindow::showConfigureDevicesPanel);

    // Input/Output consolidation (v0.Y.58.1, "Reduce top-level buttons"):
    // only one of Playback/Record/Loop stays open at a time - whichever
    // one just became visible hides the other two. Each toggleViewAction()
    // (reused below as the new dropdown's own three entries) already stays
    // correctly checked/unchecked in sync with its own dock's real
    // visibility, so hiding the other two here is the only bookkeeping
    // this needs; hide() on an already-hidden dock is a no-op (doesn't
    // re-emit visibilityChanged()), so this can't recurse.
    //
    // Re-checks isVisible() directly rather than trusting this signal's
    // own `visible` argument (deliberately ignored, not just unused) -
    // a real regression, found 2026-10-10: since these three panels
    // joined the shared Right-dock-area tab group below (Decision
    // #215), showing one of them while another is already the active
    // tab makes Qt emit this signal with `visible == false` even though
    // the panel's own isVisible() reads `true` moments later - the same
    // "joining an existing tab group" quirk the raise()-wiring below
    // already has to work around (see its own comment), now also
    // breaking this *older* code that predates tabify and still
    // (wrongly) trusted the argument. Confirmed via a temporary qDebug
    // in each lambda before fixing: `visibilityChanged false isVisible()
    // true`, for both playbackPanel_ and recordPanel_, every time.
    connect(playbackPanel_, &QDockWidget::visibilityChanged, this, [this](bool) {
        if (playbackPanel_->isVisible()) {
            recordPanel_->hide();
            loopPanel_->hide();
        }
    });
    connect(recordPanel_, &QDockWidget::visibilityChanged, this, [this](bool) {
        if (recordPanel_->isVisible()) {
            playbackPanel_->hide();
            loopPanel_->hide();
        }
    });
    connect(loopPanel_, &QDockWidget::visibilityChanged, this, [this](bool) {
        if (loopPanel_->isVisible()) {
            playbackPanel_->hide();
            recordPanel_->hide();
        }
    });

    // The menus, the menu bar's top-right dropdowns and the toolbars are
    // built in main_window_menus.cpp (v0.1.48.9).
    buildCornerMenus();
    buildFileMenu();
    buildEditMenu();
    const ZoomActions zoomActions = buildViewMenu();
    buildHelpMenu();
    buildToolBars(zoomActions);

    // ~30fps - frequent enough for each completed loop's spectrogram
    // update to read as prompt, without repainting so often it competes
    // noticeably with the background encode/decode worker thread (see
    // LoopEngine's docs) for CPU time.
    // Play Macro (v0.1.44.3): renders a performance ahead and plays it on
    // the canvas, without touching the project.
    macroPlayer_ = new MacroPlayer(playbackController_, canvas_, this);
    connect(macroPlayer_, &MacroPlayer::activeChanged, this, [this](bool active) {
        macroPlayAction_->setText(active ? tr("Stop &Macro") : tr("&Play Macro"));
        canvas_->setAttribute(Qt::WA_TransparentForMouseEvents, active);
    });
    connect(macroPlayer_, &MacroPlayer::finished, this, [this](const QString& error) {
        playbackController_->invalidate();  // the next Play composites the project again.
        playbackPanel_->setDuration(0.0);
        if (!error.isEmpty()) {
            QMessageBox::critical(this, tr("Play Macro Failed"), error);
        } else {
            statusBar()->showMessage(tr("Macro finished."), 3000);
        }
    });

    // Recording Macros, their renderers and starting playback live in their
    // own controller since v0.1.48.8 - see MacroController.
    macroController_ = std::make_unique<MacroController>(
        MacroController::Context{
            [this]() -> sound_mind::core::Project* { return project_ ? &*project_ : nullptr; },
            [this]() { return macroPlayhead(); },
            [this]() {
                return limiterEnabled_ ? std::optional<float>(static_cast<float>(limiterCeilingDb_)) : std::nullopt;
            },
            [this]() { stopPlayback(); },
            [this](const ProjectChange& change) { applyProjectChange(change); }},
        &history_, macroPlayer_, statusBar());

    // Record's take, draining and save live in their own controller since
    // v0.1.48.6 - see RecordingController.
    recordingController_ = std::make_unique<RecordingController>(
        RecordingController::Context{
            [this]() -> sound_mind::core::Project* { return project_ ? &*project_ : nullptr; },
            [this]() { return !isLoopModeRunning(); },
            [this]() { stopPlayback(); },
            [this]() { updateConfiguredDeviceLockState(); },
            [this](std::optional<sound_mind::codec::StreamImage> overlay) { canvas_->setLiveOverlay(std::move(overlay)); },
            [this](std::optional<double> fraction) { canvas_->setPlayheadFraction(fraction); },
            [this](const QString& description) { commitHistory(description); },
            [this](const ProjectChange& change) { applyProjectChange(change); },
            [this](const QString& title, const QString& message) { QMessageBox::critical(this, title, message); }},
        recordEngine_, recordPanel_, statusBar());

    // No newProject() call here, per the Landing Page milestone (v0.Y.9.1):
    // the window starts with no project open at all, showing the Landing
    // Page (stack_ defaults to index 0) until New/Open Project actually
    // creates or loads one - see setProject()'s and isShowingLandingPage()'s
    // docs.

    // Default dock layout: every panel above was added to
    // Qt::RightDockWidgetArea on its own, which (once more than one is
    // visible at a time) would otherwise stack them vertically, each
    // getting only a thin slice of the available height. Confirmed with
    // the user: opening a panel should instead default to a maximized,
    // tabbed arrangement in that same space - dragging a tab back out
    // into its own stack still works exactly as before, this only
    // changes where a *newly opened* panel starts out. tabifyDockWidget()
    // requires its first argument to already occupy the target dock area
    // (true for layersPanel_, the first one added above), and chaining
    // every other Right-dock-area panel onto it here groups all of them
    // into one shared tab group.
    tabifyDockWidget(layersPanel_, mindWavesPanel_);
    tabifyDockWidget(layersPanel_, resourceBrowserPanel_);
    tabifyDockWidget(layersPanel_, historyPanel_);
    tabifyDockWidget(layersPanel_, playbackPanel_);
    tabifyDockWidget(layersPanel_, toolConfigurationPanel_);
    tabifyDockWidget(layersPanel_, midiConfigurationPanel_);
    tabifyDockWidget(layersPanel_, chordGeneratorPanel_);
    tabifyDockWidget(layersPanel_, configureDevicesPanel_);
    tabifyDockWidget(layersPanel_, selectionConfigurationPanel_);
    tabifyDockWidget(layersPanel_, gridPanel_);
    tabifyDockWidget(layersPanel_, filterConfigurationPanel_);
    tabifyDockWidget(layersPanel_, recordPanel_);
    tabifyDockWidget(layersPanel_, loopPanel_);
    tabifyDockWidget(layersPanel_, analysisPanel_);
    tabifyDockWidget(layersPanel_, textToolPanel_);

    // Showing a tabified panel (via its own toggleViewAction(), or any
    // other show() call) adds its tab without switching to it - Qt
    // leaves whichever tab was already active in front. Confirmed with
    // the user: a newly shown panel should immediately become the
    // visible tab instead. QDockWidget::raise() is exactly "bring this
    // dock to the front of its own tab group", and visibilityChanged()
    // fires for every way a panel can become visible, so one connection
    // per panel covers it regardless of what triggered the show.
    for (QDockWidget* panel : {
             static_cast<QDockWidget*>(layersPanel_),
             static_cast<QDockWidget*>(mindWavesPanel_),
             static_cast<QDockWidget*>(resourceBrowserPanel_),
             static_cast<QDockWidget*>(historyPanel_),
             static_cast<QDockWidget*>(playbackPanel_),
             static_cast<QDockWidget*>(toolConfigurationPanel_),
             static_cast<QDockWidget*>(midiConfigurationPanel_),
             static_cast<QDockWidget*>(chordGeneratorPanel_),
             static_cast<QDockWidget*>(configureDevicesPanel_),
             static_cast<QDockWidget*>(selectionConfigurationPanel_),
             static_cast<QDockWidget*>(gridPanel_),
             static_cast<QDockWidget*>(filterConfigurationPanel_),
             static_cast<QDockWidget*>(recordPanel_),
             static_cast<QDockWidget*>(loopPanel_),
         }) {
        connect(panel, &QDockWidget::visibilityChanged, panel, [this, panel](bool visibleArg) {
            // A real, user-reported bug, fixed here: a tabified
            // `QDockWidget` that merely lost *front-tab* status to a
            // sibling - via a manual tab-bar click, or via raise()'s own
            // cascade below - fires this signal with `visibleArg ==
            // false`, but its own `isVisible()` is *already* `true` at
            // this exact synchronous moment (confirmed empirically via a
            // diagnostic build: unlike a genuine hide, Qt never actually
            // hides a backgrounded-but-still-docked tab's widget). A
            // *genuine* hide->show transition (this panel actually being
            // opened, the real case this whole mechanism exists for)
            // instead starts with `isVisible()` truly `false` at this
            // same synchronous point, only flipping `true` moments later.
            // Filtering on exactly that - `!visibleArg && isVisible()` -
            // ignores every "lost front-tab status, but was never really
            // hidden" notification before it can schedule anything, which
            // is what previously made clicking a different tab to switch
            // *back* to this one silently fail: this panel's own stale
            // deferred raise() (see below), scheduled by that spurious
            // notification, fired a tick later and undid the click.
            if (!visibleArg && panel->isVisible()) {
                return;
            }
            // Re-entrancy guard against a real, user-reported hang (CPU
            // pegged, UI locked, reproducible only with a real display -
            // this environment's own offscreen test platform never
            // triggers it): raise()'s own tab-activation can synchronously
            // fire visibilityChanged() for the *previously* active sibling
            // tab in the same group too (not just the panel being raised).
            // The filter above already catches this exact notification on
            // its own (the sibling's `isVisible()` is also already `true`
            // synchronously), but this flag stays as a second, independent
            // safety net for the same cascade - confirmed-working defense
            // kept rather than removed on the strength of one new fix.
            // Every one of these 14 lambdas shares the single
            // `raisingDockPanel_` flag (not a per-panel one), since the
            // cascade hops *between* different panels, not just within one.
            if (raisingDockPanel_) {
                return;
            }
            // Deferred to the next event-loop turn, and re-checking
            // isVisible() there rather than trusting this signal's own
            // `visible` argument - confirmed empirically that for a dock
            // *joining* an existing tab group (another tab already
            // active), Qt emits this signal with `visible == false` even
            // though the dock's own isVisible() is `true` moments later
            // once show() finishes (it correctly reflects "not hidden",
            // the final widget-level state - see this connection's own
            // filter above for the *different*, front-tab-specific sense
            // of "visible" that isn't safe to read synchronously either).
            // Calling raise() synchronously here is also too early - Qt's
            // own internal tab-group bookkeeping isn't settled yet at the
            // point this signal fires.
            QTimer::singleShot(0, panel, [this, panel]() {
                if (panel->isVisible()) {
                    raisingDockPanel_ = true;
                    panel->raise();
                    raisingDockPanel_ = false;
                }
            });
        });
    }
}

const sound_mind::core::Project* MainWindow::project() const noexcept {
    return project_ ? &*project_ : nullptr;
}

bool MainWindow::isShowingLandingPage() const noexcept { return stack_->currentWidget() == landingPage_; }

bool MainWindow::hasUnsavedChanges() const noexcept { return hasUnsavedChanges_; }

void MainWindow::closeEvent(QCloseEvent* event) {
    flushRecordingSave();  // the take belongs to the project being left (v0.1.47.1).
    if (isLoopModeRunning() || recordEngine_.isRecording()) {
        statusBar()->showMessage(tr("Stop Loop Mode or Recording before closing."), 5000);
        event->ignore();
        return;
    }
    if (isImportRunning()) {
        statusBar()->showMessage(tr("Wait for the import to finish, or cancel it, before closing."), 5000);
        event->ignore();
        return;
    }
    if (isCompositingForPlayback()) {
        statusBar()->showMessage(tr("Wait for playback to finish preparing, or cancel it, before closing."), 5000);
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
    std::vector<std::filesystem::path> midiPaths;
    for (const auto& path : paths) {
        const std::string extension = lowercasedExtension(path);
        if (isImageExtension(extension)) {
            imagePaths.push_back(path);
        } else if (isAudioExtension(extension)) {
            audioPaths.push_back(path);
        } else if (extension == ".mid" || extension == ".midi") {
            midiPaths.push_back(path);
        }
    }

    // Same choices File -> Import Audio/Image would offer, confirmed with
    // the user - see this method's own docs for the full reasoning.
    // Cancelling any one of these dialogs cancels the whole drop.
    ImageScalePickerDialog::Mode imageMode = ImageScalePickerDialog::Mode::RescaleToFitProject;
    bool importImagesAsSequence = false;
    std::optional<PolarImportParams> imagePolarParams;
    if (!imagePaths.empty()) {
        // See importImage()'s own identical reasoning for when a source
        // image is loaded up front for the "Polar" option.
        QImage polarSourceImage;
        if (imagePaths.size() == 1) {
            polarSourceImage = QImage(QString::fromStdString(imagePaths.front().string()));
        }
        const std::uint32_t polarDefaultOutputWidth = project_ ? project_->settings().canvasWidth : 0;
        const double polarTimestepMs = project_ ? project_->settings().timestepMs : 0.0;

        ImageScalePickerDialog dialog(this, /*allowSequential=*/imagePaths.size() > 1, polarSourceImage,
                                      polarDefaultOutputWidth, polarTimestepMs);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        imageMode = dialog.selectedMode();
        importImagesAsSequence = dialog.importAsSequence();
        imagePolarParams = dialog.polarParams();
    }

    std::map<std::filesystem::path, std::vector<std::size_t>> audioSnippetSelections;
    std::map<std::filesystem::path, double> audioSnippetOffsets;
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
        wireSnippetOffsetRecompute(dialog, path);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        audioSnippetSelections[path] = dialog.selectedIndices();
        audioSnippetOffsets[path] = dialog.offsetSeconds();
    }

    // Real-world testing pass, 2026-09-29: MIDI drops previously always
    // hardcoded "every channel, whole file, separate layers, no picker"
    // regardless of whether the file actually had a real choice to make -
    // audio/image drops already prompted correctly above. Mirrors
    // importMidi()'s own "skip the dialog only for the trivial case" logic
    // exactly.
    std::map<std::filesystem::path, MidiDropChoice> midiChoices;
    for (const auto& path : midiPaths) {
        QString previewError;
        const auto preview = midiImportPreviewForFile(path, &previewError);
        if (!preview || (preview->channels.size() <= 1 && preview->snippets.size() <= 1)) {
            // Unreadable (handleDroppedFiles() will report the real error
            // when it actually tries to import) or nothing to choose either
            // way - no picker needed, same as importMidi()'s own logic.
            continue;
        }
        MidiImportDialog dialog(preview->channels, preview->snippets, this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        const auto channelNumbers = dialog.selectedChannelNumbers();
        if (channelNumbers.empty()) {
            return;
        }
        MidiDropChoice choice;
        choice.separateLayerPerChannel = dialog.separateLayerPerChannel();
        choice.channelNumbers = channelNumbers;
        choice.wholeFile = dialog.wholeFile();
        if (!choice.wholeFile) {
            choice.snippetIndices = dialog.selectedSnippetIndices();
            if (choice.snippetIndices.empty()) {
                return;
            }
        }
        midiChoices[path] = choice;
    }

    handleDroppedFiles(paths, imageMode, importImagesAsSequence, audioSnippetSelections, audioSnippetOffsets,
                       imagePolarParams, midiChoices);
}

void MainWindow::handleDroppedFiles(const std::vector<std::filesystem::path>& paths,
                                     ImageScalePickerDialog::Mode imageMode, bool importImagesAsSequence,
                                     const std::map<std::filesystem::path, std::vector<std::size_t>>& audioSnippetSelections,
                                     const std::map<std::filesystem::path, double>& audioSnippetOffsets,
                                     std::optional<PolarImportParams> imagePolarParams,
                                     const std::map<std::filesystem::path, MidiDropChoice>& midiChoices) {
    std::vector<std::filesystem::path> imagePaths;
    for (const auto& path : paths) {
        if (isImageExtension(lowercasedExtension(path))) {
            imagePaths.push_back(path);
        }
    }
    if (!imagePaths.empty()) {
        QString errorMessage;
        if (!importImageFiles(imagePaths, imageMode, importImagesAsSequence, &errorMessage, imagePolarParams)) {
            statusBar()->showMessage(tr("Could not import image(s): %1").arg(errorMessage), 5000);
        }
    }

    for (const std::filesystem::path& path : paths) {
        const std::string extension = lowercasedExtension(path);

        QString errorMessage;
        if (isAudioExtension(extension)) {
            const auto selection = audioSnippetSelections.find(path);
            const auto offsetIt = audioSnippetOffsets.find(path);
            const double offsetSeconds = offsetIt != audioSnippetOffsets.end() ? offsetIt->second : 0.0;
            const bool ok = selection != audioSnippetSelections.end()
                                 ? importAudioSnippets(path, selection->second, &errorMessage, offsetSeconds)
                                 : importAudioFile(path, &errorMessage);
            if (!ok) {
                statusBar()->showMessage(
                    tr("Could not import \"%1\": %2").arg(QString::fromStdString(path.filename().string()), errorMessage),
                    5000);
            }
        } else if (isImageExtension(extension)) {
            continue;  // already handled above, as a batch.
        } else if (extension == ".mid" || extension == ".midi") {
            const auto choiceIt = midiChoices.find(path);
            bool ok = false;
            if (choiceIt == midiChoices.end()) {
                // No entry - the trivial "nothing to choose" case (or a
                // caller that predates this parameter) - same default
                // importMidi()'s own trivial case already uses.
                ok = importMidiFile(path, /*separateLayerPerChannel=*/true, &errorMessage);
            } else if (choiceIt->second.wholeFile) {
                ok = importMidiFile(path, choiceIt->second.separateLayerPerChannel, &errorMessage,
                                     choiceIt->second.channelNumbers);
            } else {
                ok = importMidiSelection(path, choiceIt->second.channelNumbers, choiceIt->second.snippetIndices,
                                          choiceIt->second.separateLayerPerChannel, &errorMessage);
            }
            if (!ok) {
                statusBar()->showMessage(
                    tr("Could not import \"%1\": %2").arg(QString::fromStdString(path.filename().string()), errorMessage),
                    5000);
            }
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
    // no-op when already stopped, or when there's no Loop engine yet -
    // see its own docs), so this is always safe regardless of caller.
    setFilterPreviewEnabled(false);
    playbackPreparer_->projectChanged();  // pending edit playback is the old project's.
    analysisController_->projectChanged();
    loopController_->abandon();
    recordingController_->abandon();
    updateConfiguredDeviceLockState();
    // Also stops any in-progress Configure Devices "test" session - a
    // project switch is exactly the kind of unrelated event every other
    // reset here already treats as "start clean", same reasoning as
    // paintAction_/pickAction_ below.
    testInputLevelTimer_->stop();
    deviceTestRecordEngine_.stop();
    deviceTestTonePlayer_.stop();
    configureDevicesPanel_->setInputLevel(0.0f);
    configureDevicesPanel_->setTestingInput(false);
    configureDevicesPanel_->setTestingOutput(false);
    landingPage_->deviceConfiguration()->setInputLevel(0.0f);
    landingPage_->deviceConfiguration()->setTestingInput(false);
    landingPage_->deviceConfiguration()->setTestingOutput(false);
    playbackController_->stop();
    playbackPanel_->setDuration(0.0);
    canvas_->setPlayheadFraction(std::nullopt);
    // paintAction_/pickAction_/selectAction_/pathAction_->setChecked(false)
    // alone wouldn't reset canvas_'s own tool mode if it was already
    // unchecked (toggled() only fires on a real change) - setToolMode()
    // directly is what actually guarantees this, the same "unconditional
    // and idempotent" reasoning as every other reset above.
    paintAction_->setChecked(false);
    pickAction_->setChecked(false);
    selectAction_->setChecked(false);
    pathAction_->setChecked(false);
    chordAction_->setChecked(false);
    textAction_->setChecked(false);
    // Explicit, for the same "unconditional and idempotent" reasoning -
    // already true via setExclusiveToolMode()'s own fallback whenever one
    // of the five above was actually checked, a no-op otherwise.
    panAction_->setChecked(true);
    // A project switch mid-recording is exactly the kind of unrelated
    // event every other reset above already treats as "start clean" - see
    // macroRecordAction_'s own docs. A no-op (does nothing, triggers no
    // toggled()) when nothing was recording.
    macroRecordAction_->setChecked(false);  // stops (and keeps) a recording in progress.
    macroController_->projectChanged();
    canvas_->setToolMode(CanvasWidget::ToolMode::None);
    canvas_->setPaintPreviewPath(sound_mind::core::Path{});
    canvas_->setPickSelectionBounds(std::nullopt);
    canvas_->setSelectionBounds(std::nullopt);
    // A stale selection from the *previous* project's own LayersPanel rows
    // could otherwise be mistaken for a real one in the new project - each
    // Project's own LayerIds start fresh, so a coincidental id match is a
    // real risk, not a theoretical one - see LayersPanel::clearSelection()'s
    // own docs.
    layersPanel_->clearSelection();
    project_ = std::move(project);
    // The History belongs to the project (v0.1.43.1) - it came with it.
    history_.setProject(&*project_);
    // A base no longer in memory (after reopening) is remade by replaying
    // the project's history from its sources (v0.1.43.3).
    history_.setBaseResolver(sound_mind::studio::replayingBaseResolver(*project_));

    // (Re)constructed fresh for the new project's own settings - loop
    // length is the project's own duration in samples (canvasWidth
    // timeline columns, each hopLength samples wide) - see the class docs'
    // v0.Y.12.1 note for why this replaced a fixed, always-default-
    // constructed member.
    const sound_mind::core::ProjectSettings& settings = project_->settings();
    // Re-synced per-project (v0.Y.47.1, Principal Modes) - unlike Hardware
    // Acceleration's own app-wide toggle, this reflects state that's
    // different for every project; the QSignalBlocker keeps this from
    // re-triggering setPrincipalMode() and needlessly marking
    // hasUnsavedChanges_ (already reset to false a few lines below).
    {
        const QSignalBlocker blocker(principalModeAction_);
        principalModeAction_->setChecked(settings.principalMode == sound_mind::core::PrincipalMode::Image);
    }
    NumericFieldBase::setDecimalSeparator(settings.decimalSeparator);
    refreshRecordTargets();
    toolConfigurationPanel_->setPrincipalMode(settings.principalMode);
    updateCanvasFit();
    (settings.decimalSeparator == sound_mind::core::DecimalSeparator::Comma ? decimalCommaAction_
                                                                             : decimalPeriodAction_)
        ->setChecked(true);
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    const auto loopLengthSamples = static_cast<std::size_t>(settings.canvasWidth) * config.hopLength;
    auto loopEngine = std::make_unique<sound_mind::core::LoopEngine>(config, loopLengthSamples, audioDeviceMode_);
    // Unlike recordEngine_ (a persistent member, never recreated), this is a
    // brand new engine with no memory of whatever was already configured in
    // Configure Devices - re-applying it here is what stops a project
    // switch from silently resetting Loop Mode back to system-default
    // devices at unity gain (real-world testing pass, 2026-09-20,
    // finding #10 - see configuredInputDeviceName_'s own docs).
    loopEngine->setPreferredInputDevice(configuredInputDeviceName_.toStdString());
    loopEngine->setPreferredOutputDevice(configuredOutputDeviceName_.toStdString());
    loopEngine->setInputGain(configuredInputGain_);
    loopEngine->setLimiterEnabled(limiterEnabled_);
    loopEngine->setLimiterCeilingDb(static_cast<float>(limiterCeilingDb_));
    loopController_->setEngine(std::move(loopEngine));

    canvas_->setProject(&*project_);
    toolPaletteController_->setProject(&*project_);
    layerController_->setProject(&*project_);
    mindWaveController_->setProject(&*project_);
    resourceBrowserController_->setProject(&*project_);
    midiConfigurationPanel_->setProject(&*project_);
    // EqualizerCurveWidget's own frequency-axis guides need the new
    // project's own settings; the preview overlay (if currently enabled)
    // needs refreshing against the new project's own Equalizer layer - see
    // updateEqualizerPreview()'s own docs on why this stays enabled across
    // the switch rather than resetting.
    filterConfigurationPanel_->setProjectSettings(settings);
    updateEqualizerPreview();
    refreshComposerPanel();
    hasUnsavedChanges_ = false;
    stack_->setCurrentWidget(canvasScrollArea_);
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
    layerController_->refreshLayersPanel();
    mindWaveController_->refreshMindWavesPanel();
    resourceBrowserController_->refreshPanel();
    refreshConvolutionKernelCombo();
    refreshFilterPresetCombo();
    refreshGridPresetCombo();
    // The new project's own layer stack/active layer are both different
    // from whatever the guardrail last computed - see
    // updateMindGrainGuardrails()'s own docs.
    updateMindGrainGuardrails();
}

void MainWindow::newProject() {
    flushRecordingSave();  // the take belongs to the project being left (v0.1.47.1).
    if (isLoopModeRunning() || recordEngine_.isRecording()) {
        statusBar()->showMessage(tr("Stop Loop Mode or Recording before starting a new project."), 5000);
        return;
    }
    if (isImportRunning()) {
        statusBar()->showMessage(tr("Wait for the import to finish, or cancel it, before starting a new project."),
                                  5000);
        return;
    }
    if (isCompositingForPlayback()) {
        statusBar()->showMessage(
            tr("Wait for playback to finish preparing, or cancel it, before starting a new project."), 5000);
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
    flushRecordingSave();  // the take belongs to the project being left (v0.1.47.1).
    if (isLoopModeRunning() || recordEngine_.isRecording()) {
        statusBar()->showMessage(tr("Stop Loop Mode or Recording before opening a project."), 5000);
        return;
    }
    if (isImportRunning()) {
        statusBar()->showMessage(tr("Wait for the import to finish, or cancel it, before opening a project."), 5000);
        return;
    }
    if (isCompositingForPlayback()) {
        statusBar()->showMessage(tr("Wait for playback to finish preparing, or cancel it, before opening a project."),
                                  5000);
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
    flushRecordingSave();  // the take belongs to the project being left (v0.1.47.1).
    if (isLoopModeRunning() || recordEngine_.isRecording()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Stop Loop Mode or Recording before opening a project.");
        }
        return false;
    }
    if (isImportRunning()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Wait for the import to finish, or cancel it, before opening a project.");
        }
        return false;
    }
    if (isCompositingForPlayback()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Wait for playback to finish preparing, or cancel it, before opening a project.");
        }
        return false;
    }

    try {
        setProject(sound_mind::core::Project::load(path));
        currentPath_ = path;
        updateWindowTitle();
        recentProjects_.add(path);
        landingPage_->setRecentProjects(recentProjects_.list());
        warnAboutMissingContent();
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
    double offsetSeconds = 0.0;
    if (snippets.size() == 1) {
        // The common case - audio no longer than the project's own
        // duration - skips the picker entirely, matching this method's
        // pre-existing behavior exactly (see importAudio()'s own docs).
        indices.push_back(snippets.front().index);
    } else {
        AudioSnippetPickerDialog dialog(snippets, this);
        wireSnippetOffsetRecompute(dialog, path);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        indices = dialog.selectedIndices();
        if (indices.empty()) {
            return;
        }
        offsetSeconds = dialog.offsetSeconds();
    }

    importAudioSnippetsAsync(path, indices, offsetSeconds);
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

    // A single file's own image is loaded up front so ImageScalePickerDialog
    // can offer "Polar" (and its own "Set origin..." preview) - see its
    // own constructor docs on why a multi-file import never gets this.
    QImage polarSourceImage;
    if (paths.size() == 1) {
        polarSourceImage = QImage(QString::fromStdString(paths.front().string()));
    }
    const std::uint32_t polarDefaultOutputWidth = project_ ? project_->settings().canvasWidth : 0;
    const double polarTimestepMs = project_ ? project_->settings().timestepMs : 0.0;

    ImageScalePickerDialog dialog(this, /*allowSequential=*/paths.size() > 1, polarSourceImage,
                                  polarDefaultOutputWidth, polarTimestepMs);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString errorMessage;
    if (!importImageFiles(paths, dialog.selectedMode(), dialog.importAsSequence(), &errorMessage,
                           dialog.polarParams())) {
        QMessageBox::critical(this, tr("Import Image Failed"), errorMessage);
    }
}

void MainWindow::importMidi() {
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Import MIDI"), QString(), tr(kMidiFileFilter));
    if (fileName.isEmpty()) {
        return;
    }
    const std::filesystem::path path(fileName.toStdString());

    QString previewError;
    const auto preview = midiImportPreviewForFile(path, &previewError);
    if (!preview) {
        QMessageBox::critical(this, tr("Import MIDI Failed"),
                               previewError.isEmpty() ? tr("Could not read the MIDI file.") : previewError);
        return;
    }

    QString errorMessage;
    if (preview->channels.size() <= 1 && preview->snippets.size() <= 1) {
        // Nothing to choose either way - matching importAudio()'s own
        // "skip the picker for the trivial case" precedent.
        if (!importMidiFile(path, /*separateLayerPerChannel=*/true, &errorMessage)) {
            QMessageBox::critical(this, tr("Import MIDI Failed"),
                                   errorMessage.isEmpty() ? tr("Could not read the MIDI file.") : errorMessage);
        }
        return;
    }

    MidiImportDialog dialog(preview->channels, preview->snippets, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto channelNumbers = dialog.selectedChannelNumbers();
    if (channelNumbers.empty()) {
        return;
    }

    bool ok = false;
    if (dialog.wholeFile()) {
        // Real-world testing pass, 2026-09-27: a real way to import
        // everything unchopped, even when there was a real channel/
        // separate-layer choice to make too - see MidiImportDialog::
        // wholeFile()'s own docs.
        ok = importMidiFile(path, dialog.separateLayerPerChannel(), &errorMessage, channelNumbers);
    } else {
        const auto snippetIndices = dialog.selectedSnippetIndices();
        if (snippetIndices.empty()) {
            return;
        }
        ok = importMidiSelection(path, channelNumbers, snippetIndices, dialog.separateLayerPerChannel(),
                                  &errorMessage);
    }
    if (!ok) {
        QMessageBox::critical(this, tr("Import MIDI Failed"),
                               errorMessage.isEmpty() ? tr("Could not read the MIDI file.") : errorMessage);
    }
}

bool MainWindow::importMidiFile(const std::filesystem::path& path, bool separateLayerPerChannel,
                                 QString* errorMessage, const std::vector<int>& channelNumbers) {
    return importController_->importMidiFile(path, separateLayerPerChannel, errorMessage, channelNumbers);
}

std::optional<sound_mind::studio::MidiImportPreview> MainWindow::midiImportPreviewForFile(
    const std::filesystem::path& path, QString* errorMessage) const {
    return importController_->midiPreviewForFile(path, errorMessage);
}

bool MainWindow::importMidiSelection(const std::filesystem::path& path, const std::vector<int>& channelNumbers,
                                      const std::vector<std::size_t>& snippetIndices, bool separateLayerPerChannel,
                                      QString* errorMessage) {
    return importController_->importMidiSelection(path, channelNumbers, snippetIndices, separateLayerPerChannel,
                                                  errorMessage);
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
                                                                                 QString* errorMessage,
                                                                                 double offsetSeconds) const {
    return importController_->audioSnippetsForFile(path, errorMessage, offsetSeconds);
}

void MainWindow::wireSnippetOffsetRecompute(AudioSnippetPickerDialog& dialog, const std::filesystem::path& path) {
    connect(&dialog, &AudioSnippetPickerDialog::offsetChanged, &dialog, [this, &dialog, path](double offsetSeconds) {
        QString errorMessage;
        dialog.setSnippets(audioSnippetsForFile(path, &errorMessage, offsetSeconds));
    });
}

bool MainWindow::importAudioSnippets(const std::filesystem::path& path, const std::vector<std::size_t>& snippetIndices,
                                      QString* errorMessage, double offsetSeconds) {
    return importController_->importAudioSnippets(path, snippetIndices, errorMessage, offsetSeconds);
}

void MainWindow::importAudioSnippetsAsync(const std::filesystem::path& path,
                                           const std::vector<std::size_t>& snippetIndices, double offsetSeconds) {
    importController_->importAudioSnippetsAsync(path, snippetIndices, offsetSeconds);
}

bool MainWindow::isImportRunning() const noexcept { return importController_->isRunning(); }

void MainWindow::cancelImport() { importController_->cancel(); }


bool MainWindow::importImageFile(const std::filesystem::path& path, ImageScalePickerDialog::Mode mode,
                                  QString* errorMessage) {
    return importController_->importImageFile(path, mode, errorMessage);
}

bool MainWindow::importImageFiles(const std::vector<std::filesystem::path>& paths, ImageScalePickerDialog::Mode mode,
                                   bool importAsSequence, QString* errorMessage,
                                   std::optional<PolarImportParams> polarParams) {
    return importController_->importImageFiles(paths, mode, importAsSequence, errorMessage, polarParams);
}

void MainWindow::exportAudio() {
    const QString fileName =
        QFileDialog::getSaveFileName(this, tr("Export Audio"), QString(), tr(kExportAudioFileFilter));
    if (fileName.isEmpty()) {
        return;
    }
    const auto path = std::filesystem::path(fileName.toStdString());
    const auto format = sound_mind::studio::audioFormatFromExtension(path);
    if (!format.has_value()) {
        QMessageBox::critical(this, tr("Export Audio Failed"),
                               tr("Unrecognized audio file extension - use .flac, .ogg, or .mp3."));
        return;
    }
    exportCompositeAudioAsync(path, *format);
}

std::filesystem::path MainWindow::suggestedVideoPath(const QString& suffix) const {
    QString stem = currentPath_ ? QString::fromStdWString(currentPath_->stem().wstring()) : tr("Untitled");
    stem += suffix;
    stem.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));
    const std::filesystem::path folder = currentPath_ ? currentPath_->parent_path() : std::filesystem::path();
    return folder / std::filesystem::path((stem + QStringLiteral(".mp4")).toStdWString());
}

void MainWindow::exportVideo() {
    if (!project_) {
        return;
    }
    VideoExportDialog dialog(tr("Export Video"), suggestedVideoPath(tr("_export")), canvas_->polarMode(),
                             project_->settings().canvasWidth, project_->settings().canvasHeight, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    exportCompositeVideoAsync(dialog.outputPath(), dialog.options(), dialog.openWhenDone());
}

void MainWindow::exportMacroVideo() {
    const auto id = chooseMacro(tr("Export Macro Video"));
    if (!id) {
        return;
    }
    const sound_mind::core::Macro* macro = project_->macroById(*id);
    VideoExportDialog dialog(tr("Export Macro Video"),
                             suggestedVideoPath(QStringLiteral("_") + QString::fromStdString(macro->name)),
                             canvas_->polarMode(), project_->settings().canvasWidth,
                             project_->settings().canvasHeight, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    exportMacroVideoAsync(dialog.outputPath(), *id, dialog.options(), dialog.openWhenDone());
}

std::unique_ptr<sound_mind::core::MacroRenderer> MainWindow::macroRendererFor(sound_mind::core::MacroId id,
                                                                               QString* errorMessage) {
    return macroController_->rendererFor(id, errorMessage);
}

bool MainWindow::exportCompositeAudioNow(const std::filesystem::path& path, QString* errorMessage) {
    return exportController_->exportAudioNow(path, errorMessage);
}

bool MainWindow::exportCompositeVideoNow(const std::filesystem::path& path, QString* errorMessage) {
    return exportController_->exportVideoNow(path, errorMessage);
}

bool MainWindow::hasExportableContent() const {
    return ExportController::hasExportableContent(project_ ? &*project_ : nullptr);
}

void MainWindow::exportCompositeVideoAsync(const std::filesystem::path& path,
                                           const sound_mind::codec::VideoExportOptions& options, bool openWhenDone) {
    exportController_->exportVideoAsync(path, options, openWhenDone);
}

void MainWindow::exportCompositeAudioAsync(const std::filesystem::path& path,
                                           sound_mind::codec::CompressedAudioFormat format) {
    exportController_->exportAudioAsync(path, format);
}

void MainWindow::exportMacroVideoAsync(const std::filesystem::path& path, sound_mind::core::MacroId id,
                                       const sound_mind::codec::VideoExportOptions& options, bool openWhenDone) {
    exportController_->exportMacroVideoAsync(path, id, options, openWhenDone);
}

bool MainWindow::isExportRunning() const noexcept { return exportController_->isRunning(); }

void MainWindow::cancelExport() { exportController_->cancel(); }

bool MainWindow::openUserDocIfBundled(const QString& htmlFilename) {
    const QString docPath = QCoreApplication::applicationDirPath() + QStringLiteral("/docs/") + htmlFilename;
    if (!std::filesystem::exists(docPath.toStdString())) {
        return false;
    }
    openExternalUrl(QUrl::fromLocalFile(docPath));
    return true;
}

void MainWindow::openExternalUrl(const QUrl& url) {
    QDesktopServices::openUrl(url);
}

void MainWindow::openUserDocOrShowFallback(const QString& htmlFilename, const QString& friendlyName) {
    if (openUserDocIfBundled(htmlFilename)) {
        return;
    }
    QMessageBox::information(
        this, tr("Documentation Not Found"),
        tr("%1 isn't bundled with this build. You can view it online instead, at:\n"
           "https://github.com/TheWalruss/sound-mind-2")
            .arg(friendlyName));
}

void MainWindow::updateWindowTitle() {
    QString title = QStringLiteral("Sound Mind Studio v" SOUND_MIND_VERSION);
    if (currentPath_) {
        title += QStringLiteral(" - ") + QString::fromStdString(currentPath_->stem().string());
    }
    setWindowTitle(title);
}

void MainWindow::cycleLayerVisibilityState(sound_mind::core::LayerId id) {
    layerController_->cycleLayerVisibilityState(id);
}

void MainWindow::hideOtherLayers(sound_mind::core::LayerId keepVisible) {
    layerController_->hideOtherLayers(keepVisible);
}

void MainWindow::unhideOtherLayers() { layerController_->unhideOtherLayers(); }

void MainWindow::setLayerOpacity(sound_mind::core::LayerId id, float opacity) {
    layerController_->setLayerOpacity(id, opacity);
}

void MainWindow::setLayerBalance(sound_mind::core::LayerId id, float balance) {
    layerController_->setLayerBalance(id, balance);
}

void MainWindow::setLayerOpacityMindWave(sound_mind::core::LayerId id,
                                           std::optional<sound_mind::core::MindWaveId> mindWaveId) {
    layerController_->setLayerOpacityMindWave(id, mindWaveId);
}

void MainWindow::setLayerTranslation(sound_mind::core::LayerId id, std::int64_t translationColumns) {
    layerController_->setLayerTranslation(id, translationColumns);
}

void MainWindow::setLayerRescale(sound_mind::core::LayerId id, double rescaleFactor) {
    layerController_->setLayerRescale(id, rescaleFactor);
}

void MainWindow::setLayerBlendMode(sound_mind::core::LayerId id, sound_mind::core::BlendMode mode) {
    layerController_->setLayerBlendMode(id, mode);
}

void MainWindow::renameLayer(sound_mind::core::LayerId id) {
    sound_mind::core::Layer* layer = layerController_->layerById(id);
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
    return layerController_->renameLayerTo(id, newName);
}

void MainWindow::deleteLayer(sound_mind::core::LayerId id) { layerController_->deleteLayer(id); }

void MainWindow::duplicateLayer(sound_mind::core::LayerId id) { layerController_->duplicateLayer(id); }

void MainWindow::cleanUpLayerPhase(sound_mind::core::LayerId id) { layerController_->cleanUpLayerPhase(id); }

void MainWindow::editFilterLayer(sound_mind::core::LayerId id) {
    layersPanel_->selectLayer(id);
    filterConfigurationPanel_->show();
    filterConfigurationPanel_->raise();
}

void MainWindow::showConfigureDevicesPanel() {
    configureDevicesPanel_->show();
    configureDevicesPanel_->raise();
}

void MainWindow::selectLayerAbove() { layerController_->selectLayerAbove(); }

void MainWindow::selectLayerBelow() { layerController_->selectLayerBelow(); }

void MainWindow::moveSelectedLayerUp() { layerController_->moveSelectedLayerUp(); }

void MainWindow::moveSelectedLayerDown() { layerController_->moveSelectedLayerDown(); }

void MainWindow::increaseSelectedLayerOpacity() { layerController_->nudgeSelectedLayerOpacity(kLayerOpacityNudgeStep); }

void MainWindow::decreaseSelectedLayerOpacity() {
    layerController_->nudgeSelectedLayerOpacity(-kLayerOpacityNudgeStep);
}

void MainWindow::addEmptyLayer() {
    if (!project_) {
        return;
    }
    // The Loop engine is already guaranteed to exist for any open project
    // (constructed fresh in setProject()), and its emptyImage() already
    // knows this project's real dimensions/config - LayerController
    // itself knows nothing about LoopEngine (see its own docs), so this
    // is resolved here and passed in.
    layerController_->addEmptyLayer(loopEngine()->emptyImage());
}

void MainWindow::addFilterLayer() { layerController_->addFilterLayer(); }

void MainWindow::generateLayer() {
    if (!project_) {
        return;
    }
    GeneratorDialog dialog(this);
    std::vector<std::pair<sound_mind::core::MindWaveId, QString>> mindWaves;
    for (const sound_mind::core::NamedMindWave& entry : project_->mindWaves()) {
        mindWaves.emplace_back(entry.id, QString::fromStdString(entry.name));
    }
    dialog.setAvailableMindWaves(mindWaves);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    layerController_->addGeneratedLayer(dialog.configuration());
}

void MainWindow::handleLayerSelectionChanged(std::optional<sound_mind::core::LayerId> id) {
    layerController_->handleLayerSelectionChanged(id);
    // The active layer (paintTargetLayerId()) may have just changed - see
    // updateMindGrainGuardrails()'s own docs.
    updateMindGrainGuardrails();
    // Keeps "Preview on Canvas" reflecting its own sticky, session-level
    // state correctly if the Equalizer layer (where that checkbox lives)
    // is what just got (re)selected - see setEqualizerPreviewEnabled()'s/
    // FilterConfigurationPanel::setEqualizerPreviewChecked()'s own docs.
    filterConfigurationPanel_->setEqualizerPreviewChecked(equalizerPreviewEnabled_);
    // The preview's target (Filter layer, selection, or whole mix) may have changed.
    scheduleFilterPreviewRender();
}

void MainWindow::applyFilterConfiguration(const sound_mind::core::FilterConfiguration& config) {
    layerController_->applyFilterConfiguration(config);
    // A no-op unless the Equalizer's own on-canvas preview is currently
    // enabled and the layer just edited actually was the Equalizer - see
    // updateEqualizerPreview()'s own docs.
    updateEqualizerPreview();
    // Heard at the next loop-back - see setFilterPreviewEnabled().
    scheduleFilterPreviewRender();
}

void MainWindow::setEqualizerPreviewEnabled(bool enabled) {
    equalizerPreviewEnabled_ = enabled;
    updateEqualizerPreview();
}

void MainWindow::updateEqualizerPreview() {
    if (!equalizerPreviewEnabled_ || !project_.has_value()) {
        canvas_->setEqualizerPreview(std::nullopt);
        return;
    }
    const auto equalizerLayer =
        std::find_if(project_->layers().begin(), project_->layers().end(),
                     [](const sound_mind::core::Layer& layer) { return layer.type() == sound_mind::core::LayerType::Equalizer; });
    if (equalizerLayer == project_->layers().end()) {
        canvas_->setEqualizerPreview(std::nullopt);
        return;
    }
    canvas_->setEqualizerPreview(equalizerLayer->filterConfiguration().frequencyGradient());
}

void MainWindow::reorderLayers(const std::vector<sound_mind::core::LayerId>& newOrderBottomToTop) {
    layerController_->reorderLayers(newOrderBottomToTop);
}

void MainWindow::addMindWave() { mindWaveController_->addMindWave(); }

void MainWindow::removeMindWave(sound_mind::core::MindWaveId id) { mindWaveController_->removeMindWave(id); }

void MainWindow::renameMindWave(sound_mind::core::MindWaveId id) {
    if (!project_.has_value()) {
        return;
    }
    const sound_mind::core::NamedMindWave* entry = project_->mindWaveById(id);
    if (entry == nullptr) {
        return;
    }
    const std::string currentName = entry->name;
    const auto result = sound_mind::studio::promptSaveName(
        this, tr("Rename MindWave"), tr("Name:"), QString::fromStdString(currentName),
        [this, currentName](const std::string& candidate) {
            // Keeping the current name unchanged is never a "collision" -
            // it's a no-op rename, even if some other, pre-existing
            // duplicate already happens to share it.
            return candidate != currentName && project_->mindWaveNameExists(candidate);
        });
    if (result.outcome == sound_mind::studio::SaveNameOutcome::Cancelled) {
        return;
    }
    if (result.replacingExisting) {
        // Replace: the *other* entry currently holding this name goes
        // away, replaced by this rename.
        auto& waves = project_->mindWaves();
        const auto otherIt = std::find_if(waves.begin(), waves.end(),
                                           [&](const auto& w) { return w.name == result.name && w.id != id; });
        if (otherIt != waves.end()) {
            project_->removeMindWave(otherIt->id);
        }
    }
    mindWaveController_->renameMindWaveTo(id, QString::fromStdString(result.name));
}

void MainWindow::updateMindWave(sound_mind::core::MindWaveId id, const sound_mind::core::MindWave& wave) {
    mindWaveController_->updateMindWave(id, wave);
}

void MainWindow::setPanModeEnabled(bool enabled) {
    setExclusiveToolMode(panAction_, enabled, CanvasWidget::ToolMode::None);
}

void MainWindow::setPaintModeEnabled(bool enabled) {
    if (!enabled) {
        toolPaletteController_->cancelPaintStroke();
        canvas_->setPaintPreviewPath(sound_mind::core::Path{});
    }
    setExclusiveToolMode(paintAction_, enabled, CanvasWidget::ToolMode::Paint);
}

void MainWindow::setPickModeEnabled(bool enabled) {
    if (!enabled) {
        toolPaletteController_->clearPickSelection();
        canvas_->setPaintPreviewPath(sound_mind::core::Path{});
    }
    setExclusiveToolMode(pickAction_, enabled, CanvasWidget::ToolMode::Pick);
}

void MainWindow::setSelectModeEnabled(bool enabled) {
    if (!enabled) {
        toolPaletteController_->cancelSelectionDrag();
        toolPaletteController_->cancelRotateDrag();
    }
    setExclusiveToolMode(selectAction_, enabled, CanvasWidget::ToolMode::Select);
}

void MainWindow::setPathModeEnabled(bool enabled) {
    if (!enabled) {
        toolPaletteController_->cancelPathStroke();
        canvas_->setPaintPreviewPath(sound_mind::core::Path{});
    }
    setExclusiveToolMode(pathAction_, enabled, CanvasWidget::ToolMode::Path);
}

void MainWindow::setChordModeEnabled(bool enabled) {
    // No cancel call, unlike setPaintModeEnabled()/setPickModeEnabled()/
    // setSelectModeEnabled()/setPathModeEnabled() above - ChordStamp mode's
    // own single-press gesture has no in-progress state to cancel in the
    // first place (see CanvasWidget::ToolMode::ChordStamp's own docs).
    setExclusiveToolMode(chordAction_, enabled, CanvasWidget::ToolMode::ChordStamp);
}

void MainWindow::setMasterLimiter(bool enabled, double ceilingDb) {
    limiterEnabled_ = enabled;
    limiterCeilingDb_ = std::clamp(ceilingDb, -24.0, 0.0);
    playbackController_->setLimiterEnabled(enabled);
    playbackController_->setLimiterCeilingDb(static_cast<float>(limiterCeilingDb_));
    if (loopEngine()) {
        loopEngine()->setLimiterEnabled(enabled);
        loopEngine()->setLimiterCeilingDb(static_cast<float>(limiterCeilingDb_));
    }
    settings_.setValue(QStringLiteral("masterLimiterEnabled"), enabled);
    settings_.setValue(QStringLiteral("masterLimiterCeilingDb"), limiterCeilingDb_);
    configureDevicesPanel_->setMasterLimiter(enabled, limiterCeilingDb_);
    landingPage_->deviceConfiguration()->setMasterLimiter(enabled, limiterCeilingDb_);
}

void MainWindow::setLoudnessNormalization(bool enabled, double targetLufs) {
    normalizationEnabled_ = enabled;
    normalizationTargetLufs_ = std::clamp(targetLufs, -40.0, -5.0);
    settings_.setValue(QStringLiteral("loudnessNormalizationEnabled"), enabled);
    settings_.setValue(QStringLiteral("loudnessTargetLufs"), normalizationTargetLufs_);
    configureDevicesPanel_->setLoudnessNormalization(enabled, normalizationTargetLufs_);
    landingPage_->deviceConfiguration()->setLoudnessNormalization(enabled, normalizationTargetLufs_);
    applyNormalizationGain();
}

bool MainWindow::loopLimiterEnabled() const noexcept { return loopEngine() && loopEngine()->limiterEnabled(); }

double MainWindow::normalizationGainDb() const noexcept { return playbackController_->normalizationGainDb(); }

void MainWindow::noteLoadedPlaybackLoudness(double lufs) {
    loadedMixLoudnessLufs_ =
        lufs > sound_mind::core::kLoudnessFloorLufs ? std::optional<double>(lufs) : std::nullopt;
    applyNormalizationGain();
}

void MainWindow::applyNormalizationGain() {
    double gainDb = 0.0;
    if (normalizationEnabled_ && loadedMixLoudnessLufs_.has_value()) {
        // Bounded, so a near-silent mix isn't turned up into a roar.
        gainDb = std::clamp(normalizationTargetLufs_ - *loadedMixLoudnessLufs_, -24.0, 12.0);
    }
    playbackController_->setNormalizationGainDb(static_cast<float>(gainDb));
}

void MainWindow::noteProjectChangedForAnalysis() {
    if (analysisController_) {  // still constructing: nothing analyzed yet.
        analysisController_->noteProjectChanged();
    }
}


void MainWindow::setAutoAnalyze(bool enabled) { analysisController_->setAutoAnalyze(enabled); }

void MainWindow::analyzeLoudness() { analysisController_->analyze(); }

void MainWindow::flushPickedTextEdit() {
    pickedTextEditTimer_->stop();
    if (!pendingPickedTextSettings_.has_value()) {
        return;
    }
    const TextSettings settings = *pendingPickedTextSettings_;
    pendingPickedTextSettings_.reset();
    toolPaletteController_->applyTextSettingsToPicked(settings);
}

void MainWindow::setTextModeEnabled(bool enabled) {
    flushPickedTextEdit();  // an edit in flight lands on the text it was meant for.
    textToolPanel_->setEditingPickedText(!enabled && toolPaletteController_->selectedTextSettings().has_value());
    if (!enabled) {
        toolPaletteController_->textController()->reset();
    } else {
        textToolPanel_->show();
        textToolPanel_->raise();
    }
    setExclusiveToolMode(textAction_, enabled, CanvasWidget::ToolMode::Text);
}

void MainWindow::setExclusiveToolMode(QAction* activated, bool enabled, CanvasWidget::ToolMode mode) {
    canvas_->setToolMode(enabled ? mode : CanvasWidget::ToolMode::None);

    // Blocked so this doesn't recurse back into setPanModeEnabled()/
    // setPaintModeEnabled()/setPickModeEnabled()/setSelectModeEnabled()/
    // setPathModeEnabled()/setChordModeEnabled() - see this method's own
    // docs for why exclusivity is handled by hand here rather than via a
    // QActionGroup.
    const QSignalBlocker panBlocker(panAction_);
    const QSignalBlocker paintBlocker(paintAction_);
    const QSignalBlocker pickBlocker(pickAction_);
    const QSignalBlocker selectBlocker(selectAction_);
    const QSignalBlocker pathBlocker(pathAction_);
    const QSignalBlocker chordBlocker(chordAction_);
    const QSignalBlocker textBlocker(textAction_);
    // When disabled, Pan (ToolMode::None's own entry) is the one left
    // checked - falls out of the same "exactly one of six checked"
    // invariant as the enabled case below, not a special case.
    QAction* const checkedAction = enabled ? activated : panAction_;
    panAction_->setChecked(checkedAction == panAction_);
    paintAction_->setChecked(checkedAction == paintAction_);
    pickAction_->setChecked(checkedAction == pickAction_);
    selectAction_->setChecked(checkedAction == selectAction_);
    pathAction_->setChecked(checkedAction == pathAction_);
    chordAction_->setChecked(checkedAction == chordAction_);
    textAction_->setChecked(checkedAction == textAction_);
    updateToolButtonLabel();
}

void MainWindow::updateToolButtonLabel() {
    QAction* checkedAction = panAction_;
    for (QAction* candidate :
         {panAction_, paintAction_, pickAction_, selectAction_, pathAction_, chordAction_, textAction_}) {
        if (candidate->isChecked()) {
            checkedAction = candidate;
            break;
        }
    }
    toolButton_->setText(tr("Tool: %1").arg(checkedAction->text()));
}

void MainWindow::updateMindGrainGuardrails() {
    const auto activeLayer = layerController_->paintTargetLayerId();
    if (activeLayer.has_value()) {
        toolConfigurationPanel_->setActiveLayer(*activeLayer);
    }

    // v0.Y.62.1 Installment C - a real-world testing pass finding: a
    // Filter/Equalizer layer can never hold paintable content at all
    // (PaintController::beginStroke()'s own guard already refuses it
    // silently), regardless of which tool is configured - checked here,
    // independent of the Mind-Grain-specific logic below, so selecting
    // one as the active layer visibly disables Paint rather than leaving
    // the Tool Configuration panel showing controls that quietly do
    // nothing.
    const sound_mind::core::Layer* activeLayerPtr =
        (project_.has_value() && activeLayer.has_value()) ? project_->layerById(*activeLayer) : nullptr;
    const bool activeLayerIsFilterType =
        activeLayerPtr != nullptr && sound_mind::core::isFilterLayerType(activeLayerPtr->type());

    const auto* mindGrain =
        dynamic_cast<const sound_mind::core::MindGrainConfiguration*>(&toolConfigurationPanel_->toolConfiguration());

    std::vector<sound_mind::core::LayerId> disallowed;
    bool activeLayerDisallowed = activeLayerIsFilterType;
    if (mindGrain != nullptr && project_.has_value()) {
        const sound_mind::core::LayerId sourceLayer = mindGrain->sourceLayerId();
        for (const sound_mind::core::Layer& layer : project_->layers()) {
            if (!sound_mind::core::isLayerAbove(*project_, layer.id(), sourceLayer)) {
                disallowed.push_back(layer.id());
            }
        }
        activeLayerDisallowed = activeLayerDisallowed || !activeLayer.has_value() ||
                                 !sound_mind::core::isLayerAbove(*project_, *activeLayer, sourceLayer);
    }
    layersPanel_->setDisallowedLayers(disallowed);

    if (activeLayerDisallowed) {
        if (paintAction_->isChecked()) {
            setPaintModeEnabled(false);
        }
        paintAction_->setEnabled(false);
        // The Filter/Equalizer-type reason takes priority in the tooltip's
        // own wording - it's the more fundamental of the two, true
        // regardless of which tool is configured.
        paintAction_->setToolTip(
            activeLayerIsFilterType
                ? tr("Filter and Equalizer layers composite and transform the layers beneath them - they have no "
                     "paintable content of their own. Select a Normal/Background layer to paint.")
                : tr("The configured Mind Grain can't paint onto the active layer - it must stay above its own source "
                     "layer. Select a layer higher in the stack, or reorder the layers, first."));
    } else {
        paintAction_->setEnabled(true);
        // Restores the plain default (Qt only auto-derives a tooltip from
        // an action's own text() as long as setToolTip() has never been
        // called on it at all - once the branch above has called it once,
        // that auto-derivation is gone for good, so this has to be spelled
        // out explicitly from here on rather than cleared to an empty
        // string).
        paintAction_->setToolTip(paintAction_->text());
    }
}

void MainWindow::updateConfiguredDeviceLockState() {
    const bool inputBusy = isLoopModeRunning() || recordEngine_.isRecording();
    const bool outputBusy = isLoopModeRunning();
    configureDevicesPanel_->setInputDeviceSelectionEnabled(!inputBusy);
    configureDevicesPanel_->setOutputDeviceSelectionEnabled(!outputBusy);
    landingPage_->deviceConfiguration()->setInputDeviceSelectionEnabled(!inputBusy);
    landingPage_->deviceConfiguration()->setOutputDeviceSelectionEnabled(!outputBusy);
}

void MainWindow::undo() {
    if (!history_.canUndo()) {
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);  // remaking a base can take a moment.
    const bool undone = history_.undo();
    QApplication::restoreOverrideCursor();
    if (!undone) {
        statusBar()->showMessage(tr("Can't undo further: the earlier version of a layer's content can't be remade "
                                    "from this project's sources."),
                                 8000);
    }
    noteProjectChangedForAnalysis();  // undo can change anything the report measured.
}

void MainWindow::redo() {
    if (!history_.canRedo()) {
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool redone = history_.redo();
    QApplication::restoreOverrideCursor();
    if (!redone) {
        statusBar()->showMessage(tr("Can't redo: a layer's content can't be remade from this project's sources."),
                                 8000);
    }
    noteProjectChangedForAnalysis();
}

void MainWindow::refreshHistoryPanel() {
    QStringList descriptions;
    descriptions.reserve(static_cast<int>(history_.count()));
    for (std::size_t i = 0; i < history_.count(); ++i) {
        descriptions.append(history_.descriptionAt(i));
    }
    historyPanel_->setHistory(descriptions, history_.currentIndex());
}

void MainWindow::jumpToHistoryIndex(std::size_t index) {
    history_.jumpTo(index);
    noteProjectChangedForAnalysis();
}

void MainWindow::warnAboutMissingContent() {
    if (!project_) {
        return;
    }
    QStringList missing;
    for (const sound_mind::core::Layer& layer : project_->layers()) {
        // A recipe says it was made from something; no content means its
        // media file didn't load.
        if (!layer.baseRecipe().is_null() && !layer.content().has_value()) {
            missing.append(QString::fromStdString(layer.name()));
        }
    }
    if (missing.isEmpty()) {
        return;
    }
    QString message = tr("The content of these layers is missing from the project folder:\n\n%1")
                          .arg(missing.join(QStringLiteral("\n")));
    if (project_->history().startsAtCreation()) {
        message += tr("\n\nFile → Remaster... can rebuild it from the project's history and sources (keep the "
                      "settings as they are).");
    }
    QMessageBox::warning(this, tr("Missing Layer Content"), message);
}

bool MainWindow::remasterTo(const sound_mind::studio::RemasterRequest& request, QString* errorMessage) {
    const auto input = project_ ? sound_mind::studio::remasterInputFrom(*project_) : std::nullopt;
    if (!input.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("This project's history doesn't go back to its creation (it was made with a version "
                               "before 0.1.43.1), so it can't be rebuilt from it.");
        }
        return false;
    }
    std::atomic<std::size_t> done{0};
    std::size_t total = input->entries.size();
    QString failure;
    bool succeeded = false;
    sound_mind::core::BackgroundTask task([&](sound_mind::core::CancellationToken& token) {
        const auto result = sound_mind::studio::remasterProject(
            *input, request, [&token]() { return token.cancellationRequested(); },
            [&done](std::size_t count, std::size_t) { done = count; }, &failure);
        succeeded = result.has_value();
    });
    QProgressDialog progress(tr("Remastering..."), tr("Cancel"), 0, static_cast<int>(std::max<std::size_t>(total, 1)),
                             this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(300);
    task.start();
    while (task.isRunning()) {
        progress.setValue(static_cast<int>(done.load()));
        if (progress.wasCanceled()) {
            task.requestCancel();
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(20);
    }
    task.waitForFinished();
    progress.setValue(progress.maximum());
    if (!succeeded && errorMessage != nullptr) {
        *errorMessage = failure;
    }
    return succeeded;
}

void MainWindow::remaster() {
    if (!project_ || !currentPath_) {
        return;
    }
    if (!project_->history().startsAtCreation()) {
        QMessageBox::information(this, tr("Remaster"),
                                 tr("This project's history doesn't go back to its creation (it was made with a "
                                    "version before 0.1.43.1), so it can't be rebuilt from it."));
        return;
    }
    sound_mind::studio::RemasterDialog dialog(*project_, *currentPath_, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const sound_mind::studio::RemasterRequest request = dialog.request();
    QString error;
    if (!remasterTo(request, &error)) {
        QMessageBox::critical(this, tr("Remaster Failed"), error);
        return;
    }
    const auto answer = QMessageBox::question(
        this, tr("Remaster"),
        tr("Saved the remastered project as \"%1\". Open it now?")
            .arg(QString::fromStdWString(request.outputPath.filename().wstring())));
    if (answer == QMessageBox::Yes && confirmDiscardUnsavedChanges()) {
        if (!openProjectAt(request.outputPath, &error)) {
            QMessageBox::critical(this, tr("Open Project Failed"), error);
        }
    }
}

std::optional<sound_mind::core::SourceId> MainWindow::storeAudioSource(const sound_mind::codec::AudioBuffer& audio,
                                                                      const std::string& kind, const QString& name) {
    const auto folder = project_ ? project_->sourcesFolder() : std::nullopt;
    if (!folder.has_value() || audio.frameCount() == 0) {
        return std::nullopt;
    }
    try {
        return project_->addSource(sound_mind::core::storeSourceAudio(*folder, audio, kind, name.toStdString()));
    } catch (const std::exception&) {
        return std::nullopt;  // kept without a source: not replayable from the original.
    }
}

void MainWindow::commitHistory(const QString& description) {
    if (project_) {
        history_.commit(*project_, description);
    }
}

void MainWindow::handleHistoryChange(const sound_mind::core::HistoryChange& change) {
    if (!project_) {
        return;
    }
    hasUnsavedChanges_ = true;
    if (!change.resources.empty()) {
        resourceBrowserController_->refreshPanel();
        refreshConvolutionKernelCombo();
        refreshFilterPresetCombo();
        refreshGridPresetCombo();
        midiConfigurationPanel_->setProject(&*project_);
        toolConfigurationPanel_->setProject(&*project_);
        toolConfigurationPanel_->refreshResonanceProfiles();
    }
    if (change.layerListChanged || !change.layers.empty() || !change.contentLayers.empty()) {
        refreshRecordTargets();
        updateEqualizerPreview();
        updateMindGrainGuardrails();
    }
    refreshComposerPanel();
    canvas_->update();
}

void MainWindow::refreshComposerPanel() {
    if (!project_) {
        composerPanel_->setTracks({});
        return;
    }

    const sound_mind::core::ProjectSettings& settings = project_->settings();
    const double totalDurationSeconds = static_cast<double>(settings.canvasWidth) * settings.timestepMs / 1000.0;

    std::vector<ComposerPanel::TrackData> tracks;
    // Topmost layer first - matching layersPanel_'s own display
    // convention (see LayersPanel's own class docs).
    const auto& layers = project_->layers();
    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        const sound_mind::core::Layer& layer = *it;
        if (!layer.visible()) {
            continue;
        }

        ComposerPanel::TrackData track;
        track.id = layer.id();
        track.name = QString::fromStdString(layer.name());

        if (const auto amplitude = sound_mind::core::renderLayerAmplitudeSummary(layer, 400, 64)) {
            track.amplitudeImage = sound_mind::studio::toQImageView(*amplitude).copy();
        }
        if (const auto thumbnail = sound_mind::core::renderLayerThumbnail(layer, 400, 64)) {
            track.thumbnailImage = sound_mind::studio::toQImageView(*thumbnail).copy();
        }

        if (totalDurationSeconds > 0.0) {
            for (const sound_mind::core::Operation* operation :
                 project_->operationLog().activeOperationsTargeting(layer.id())) {
                const sound_mind::core::TimeFrequencyRect bounds = operation->bounds();
                ComposerPanel::OperationBox box;
                box.startFraction = std::clamp(bounds.startTimeSeconds / totalDurationSeconds, 0.0, 1.0);
                box.endFraction = std::clamp(bounds.endTimeSeconds / totalDurationSeconds, 0.0, 1.0);
                track.operations.push_back(box);
            }
        }

        tracks.push_back(std::move(track));
    }

    composerPanel_->setTracks(tracks);
}

void MainWindow::zoomIn() { canvas_->zoomIn(); }

void MainWindow::zoomOut() { canvas_->zoomOut(); }

void MainWindow::zoomInTimeOnly() { canvas_->zoomInTimeOnly(); }

void MainWindow::zoomOutTimeOnly() { canvas_->zoomOutTimeOnly(); }

void MainWindow::zoomInFrequencyOnly() { canvas_->zoomInFrequencyOnly(); }

void MainWindow::zoomOutFrequencyOnly() { canvas_->zoomOutFrequencyOnly(); }

void MainWindow::zoomInCoarse() { canvas_->zoomInCoarse(); }

void MainWindow::zoomOutCoarse() { canvas_->zoomOutCoarse(); }

void MainWindow::zoomToFit() { canvas_->zoomToFit(); }

void MainWindow::zoomToActualSize() { canvas_->zoomToActualSize(); }

void MainWindow::deletePickedObject() { toolPaletteController_->deleteSelection(); }

void MainWindow::bringPickedObjectToFront() { toolPaletteController_->bringToFront(); }

void MainWindow::sendPickedObjectToBack() { toolPaletteController_->sendToBack(); }

void MainWindow::bringPickedObjectForward() { toolPaletteController_->bringForward(); }

void MainWindow::sendPickedObjectBackward() { toolPaletteController_->sendBackward(); }

void MainWindow::editPickedPath() { toolPaletteController_->beginPathEdit(); }

void MainWindow::togglePickedPathNodeType() { toolPaletteController_->toggleSelectedPathNodeType(); }

void MainWindow::applyPickedPathEdit() { toolPaletteController_->commitPathEdit(); }

void MainWindow::cancelPickedPathEdit() { toolPaletteController_->cancelPathEdit(); }

void MainWindow::updateEditActionStates() {
    for (const auto& [action, applies] : editActionConditions_) {
        action->setEnabled(applies());
    }
    // A submenu with nothing that applies is grayed out as a whole.
    for (QAction* entry : editMenu_->actions()) {
        QMenu* submenu = entry->menu();
        if (submenu == nullptr) {
            continue;
        }
        const auto submenuActions = submenu->actions();
        entry->setEnabled(std::any_of(submenuActions.begin(), submenuActions.end(), [](QAction* action) {
            return !action->isSeparator() && (action->isEnabled() || action->menu() != nullptr);
        }));
    }
}

void MainWindow::restoreEditActions() {
    for (const auto& [action, applies] : editActionConditions_) {
        action->setEnabled(true);
    }
    for (QAction* entry : editMenu_->actions()) {
        entry->setEnabled(true);
    }
}

void MainWindow::mergePickedPaths() {
    if (toolPaletteController_->mergePickedPaths().has_value()) {
        statusBar()->showMessage(tr("Merged the Picked paths into a branching path."), 5000);
    }
}

void MainWindow::strokePickedPath() {
    if (!project_.has_value() || toolPaletteController_->selectedBranches().empty()) {
        return;
    }
    const std::size_t strokes = toolPaletteController_->strokePickedPath();
    if (strokes == 0) {
        statusBar()->showMessage(tr("Can't paint on this layer with the current tool."), 5000);
        return;
    }
    statusBar()->showMessage(tr("Stroked the path with the current tool."), 5000);
}

void MainWindow::showCanvasContextMenu(sound_mind::core::TimeFrequencyPoint point, QPoint globalPos) {
    // "Right-clicking on an object on the canvas, or an empty part of
    // the canvas, shall cause a small menu to appear with likely
    // actions to take" - direct user feedback. Hit-tests via the exact
    // same beginPick() a real left-click-to-Pick already uses (arming a
    // potential drag too, same as that click - harmless here since
    // nothing ever follows up with continueMove()/endMove(), the same
    // "a plain click, not a drag" no-op path a real click-without-
    // dragging already relies on), then builds one of two small, fixed
    // menus depending on whether that found something.
    //
    // Deliberately two fixed menus, not a single one trying to cover
    // both Pick and Selection (two genuinely separate mechanisms in this
    // app, confirmed by reading each action's own doc comment before
    // assuming): Edit Path/Bring to Front/Send to Back/Delete all act on
    // whatever's Picked; Paste/Fill Selection/Apply Filter to
    // Selection/Deselect all act on the separate rectangle/lasso
    // Selection (or the clipboard) and stay exactly as "always present,
    // no-op when inapplicable" as their own Edit menu entries already
    // are - this doesn't try to detect whether a Selection genuinely
    // covers `point` before offering them, the same way the Edit menu
    // itself never has. Graying out whichever ones don't actually apply
    // is next, separate work, confirmed with the user.
    if (const auto layerId = layerController_->paintTargetLayerId(); layerId.has_value()) {
        toolPaletteController_->beginPick(*layerId, point);
    }

    // Pre-Beta Plan v0.Y.77.1: a guide path (regular or branching) gets its
    // own entries, and entries that don't apply here are grayed out.
    QMenu menu(this);
    if (toolPaletteController_->pickedObjectCount() > 1) {
        // A rectangle multi-selection (v0.Y.77.1): what applies to them all.
        QAction* copyObjects = menu.addAction(tr("Copy"));
        connect(copyObjects, &QAction::triggered, this, &MainWindow::copySelection);
        QAction* merge = menu.addAction(tr("Merge into Branching Path"));
        merge->setEnabled(toolPaletteController_->canMergePickedPaths());
        connect(merge, &QAction::triggered, this, &MainWindow::mergePickedPaths);
        menu.addSeparator();
        QAction* deleteObjects = menu.addAction(tr("Delete"));
        connect(deleteObjects, &QAction::triggered, this, &MainWindow::deletePickedObject);
    } else if (toolPaletteController_->selectedOperationId().has_value()) {
        const bool guide = toolPaletteController_->selectedIsGuidePath();
        const bool singlePath = toolPaletteController_->selectedPath().has_value();
        if (!guide || singlePath) {
            QAction* editPath = menu.addAction(tr("Edit Path"));
            editPath->setEnabled(singlePath && !toolPaletteController_->isPathEditActive());
            connect(editPath, &QAction::triggered, this, &MainWindow::editPickedPath);
        }
        if (guide) {
            QAction* stroke = menu.addAction(tr("Stroke Path with Current Tool"));
            connect(stroke, &QAction::triggered, this, &MainWindow::strokePickedPath);
            QAction* resonance = menu.addAction(tr("Create Resonance Profile..."));
            connect(resonance, &QAction::triggered, this, &MainWindow::createResonanceFromPickedPath);
        }
        menu.addSeparator();
        QAction* bringToFront = menu.addAction(tr("Bring to Front"));
        connect(bringToFront, &QAction::triggered, this, &MainWindow::bringPickedObjectToFront);
        QAction* sendToBack = menu.addAction(tr("Send to Back"));
        connect(sendToBack, &QAction::triggered, this, &MainWindow::sendPickedObjectToBack);
        menu.addSeparator();
        QAction* deleteObject = menu.addAction(tr("Delete"));
        connect(deleteObject, &QAction::triggered, this, &MainWindow::deletePickedObject);
    } else {
        const bool selection = toolPaletteController_->hasSelection();
        QAction* pasteAction = menu.addAction(tr("Paste"));
        pasteAction->setEnabled(toolPaletteController_->hasClipboard() || toolPaletteController_->pastesObjects());
        connect(pasteAction, &QAction::triggered, this, &MainWindow::paste);
        menu.addSeparator();
        QAction* fill = menu.addAction(tr("Fill Selection..."));
        fill->setEnabled(selection);
        connect(fill, &QAction::triggered, this, &MainWindow::fillSelection);
        QAction* applyFilter = menu.addAction(tr("Apply Filter to Selection"));
        applyFilter->setEnabled(selection);
        connect(applyFilter, &QAction::triggered, this, &MainWindow::applyFilterToSelection);
        menu.addSeparator();
        QAction* deselectAction = menu.addAction(tr("Deselect"));
        deselectAction->setEnabled(selection);
        connect(deselectAction, &QAction::triggered, this, &MainWindow::deselect);
    }
    menu.exec(globalPos);
}

void MainWindow::previewToolConfiguration() {
    if (!project_) {
        return;
    }
    const auto image =
        sound_mind::core::toolConfigurationPreviewStreamImage(toolConfigurationPanel_->toolConfiguration(), *project_);
    const auto audio = sound_mind::codec::decode(image);
    if (audio.frameCount() == 0) {
        return;
    }
    toolConfigurationPreviewEngine_.loadAudio(audio);
    toolConfigurationPreviewEngine_.play();
    toolConfigurationPanel_->setPreviewPlaying(true);
}

void MainWindow::stopToolConfigurationPreview() {
    toolConfigurationPreviewEngine_.stop();
    toolConfigurationPanel_->setPreviewPlaying(false);
}

void MainWindow::deselect() { toolPaletteController_->clearSelection(); }

void MainWindow::fillSelectionWith(QColor color) {
    sound_mind::core::Gradient gradient;
    auto stop = gradient.stops().front();
    stop.leftIntensity = displayByteToDb(color.red());
    stop.rightIntensity = displayByteToDb(color.green());
    stop.leftOpacity = 1.0f;
    stop.rightOpacity = 1.0f;
    gradient.setStopValues(0, stop);
    gradient.setStopValues(1, stop);
    fillSelectionWithGradient(gradient);
}

void MainWindow::fillSelectionWithGradient(const sound_mind::core::Gradient& gradient) {
    if (!toolPaletteController_->hasSelection()) {
        return;
    }
    toolPaletteController_->fill(gradient);
}

void MainWindow::fillSelection() {
    if (!toolPaletteController_->hasSelection()) {
        return;
    }
    // Seeded with the same fully-opaque yellow default fillSelectionWith()'s
    // own flat-color shortcut always used (0 dB both channels, full
    // opacity) - accepting immediately reproduces that exact prior
    // behavior; the editor lets it be varied into a real gradient instead.
    sound_mind::core::Gradient defaultGradient;
    auto stop = defaultGradient.stops().front();
    stop.leftIntensity = 0.0f;
    stop.rightIntensity = 0.0f;
    stop.leftOpacity = 1.0f;
    stop.rightOpacity = 1.0f;
    defaultGradient.setStopValues(0, stop);
    defaultGradient.setStopValues(1, stop);

    FillGradientDialog dialog(defaultGradient, this);
    if (dialog.exec() == QDialog::Accepted) {
        fillSelectionWithGradient(dialog.gradient());
    }
}

void MainWindow::applyFilterToSelection() {
    if (!toolPaletteController_->hasSelection()) {
        return;
    }
    toolPaletteController_->applyFilterToSelection(filterConfigurationPanel_->filterConfiguration());
}

void MainWindow::setPrincipalMode(bool imageModeEnabled) {
    if (!project_) {
        return;
    }
    project_->setPrincipalMode(imageModeEnabled ? sound_mind::core::PrincipalMode::Image
                                                 : sound_mind::core::PrincipalMode::Sound);
    applyProjectChange(ProjectChange::settingsOnly());
    toolConfigurationPanel_->setPrincipalMode(project_->settings().principalMode);
    updateCanvasFit();
}

void MainWindow::updateCanvasFit() {
    const bool fitToWindow = canvas_->zoomMode() == CanvasWidget::ZoomMode::FitToWindow;
    const bool squareCells = fitToWindow && project_.has_value() &&
                             project_->settings().principalMode == sound_mind::core::PrincipalMode::Image;
    if (!squareCells) {
        canvasScrollArea_->setWidgetResizable(fitToWindow);
        return;
    }
    canvasScrollArea_->setWidgetResizable(false);
    const auto& settings = project_->settings();
    const QSize available = canvasScrollArea_->viewport()->size();
    const double columns = static_cast<double>(std::max<std::uint32_t>(settings.canvasWidth, 1));
    const double rows = static_cast<double>(std::max<std::uint32_t>(settings.binCount, 1));
    const double scale = std::min(available.width() / columns, available.height() / rows);
    canvas_->resize(std::max(1, static_cast<int>(std::lround(columns * scale))),
                    std::max(1, static_cast<int>(std::lround(rows * scale))));
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == canvasScrollArea_->viewport() && event->type() == QEvent::Resize) {
        updateCanvasFit();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setDecimalSeparator(sound_mind::core::DecimalSeparator separator) {
    NumericFieldBase::setDecimalSeparator(separator);
    if (!project_) {
        return;
    }
    project_->setDecimalSeparator(separator);
    applyProjectChange(ProjectChange::settingsOnly());
}

void MainWindow::setMacroRecordingEnabled(bool enabled) { macroController_->setRecordingEnabled(enabled); }

std::pair<double, bool> MainWindow::macroPlayhead() const {
    if (isLoopModeRunning() && project_) {
        const auto config = sound_mind::core::streamCodecConfigFor(project_->settings());
        return {static_cast<double>(loopEngine()->playbackPositionSamples()) / static_cast<double>(config.sampleRateHz),
                true};
    }
    return {currentPlaybackPositionSeconds_, playbackController_->isPlaying()};
}

void MainWindow::noteMacroPlayhead() {
    if (macroController_) {  // still constructing: nothing is being recorded.
        macroController_->notePlayhead();
    }
}

std::optional<sound_mind::core::MacroId> MainWindow::chooseMacro(const QString& title) {
    if (!project_) {
        return std::nullopt;
    }
    std::vector<sound_mind::core::MacroId> ids;
    QStringList names;
    for (const sound_mind::core::Macro& macro : project_->macros()) {
        if (!macro.recording) {
            ids.push_back(macro.id);
            names.append(QString::fromStdString(macro.name));
        }
    }
    if (ids.empty()) {
        QMessageBox::information(this, title, tr("No performance has been recorded yet - use Macro → Record Macro."));
        return std::nullopt;
    }
    if (ids.size() == 1) {
        return ids.front();
    }
    bool ok = false;
    const QString picked = QInputDialog::getItem(this, title, tr("Macro:"), names, names.size() - 1, false, &ok);
    if (!ok) {
        return std::nullopt;
    }
    return ids[static_cast<std::size_t>(names.indexOf(picked))];
}

void MainWindow::manageMacros() {
    if (!project_) {
        return;
    }
    MacroManagerDialog dialog(*project_, this);
    dialog.exec();
    if (dialog.changed()) {
        applyProjectChange(ProjectChange::settingsOnly());
    }
}

void MainWindow::playMacro() {
    if (!project_) {
        return;
    }
    if (macroPlayer_->isActive()) {
        macroPlayer_->stop();  // the action reads "Stop Macro" meanwhile.
        return;
    }
    const auto id = chooseMacro(tr("Play Macro"));
    if (!id) {
        return;
    }
    QString error;
    if (!macroController_->play(*id, &error)) {
        QMessageBox::critical(this, tr("Play Macro Failed"), error);
    }
}

void MainWindow::usePickedPathAsMindWaveShape() {
    const auto curve = toolPaletteController_->selectedPath();
    if (!curve.has_value()) {
        return;
    }
    const auto mindWaveId = mindWavesPanel_->selectedMindWaveId();
    if (!mindWaveId.has_value()) {
        return;
    }
    mindWaveController_->setDrawnPath(*mindWaveId, *curve);
}

std::optional<sound_mind::core::CurveGraph> MainWindow::pickedResonanceGraph() const {
    if (!project_.has_value()) {
        return std::nullopt;
    }
    const auto branches = toolPaletteController_->selectedBranches();
    const double scale = sound_mind::core::frequencyToTimeScaleFor(project_->settings());
    if (branches.size() > 1) {
        return sound_mind::core::curveGraphFromBranches(branches, scale, kResonanceNodeCount);
    }
    if (branches.size() == 1) {
        return sound_mind::core::curveGraphFromPath(branches.front().path, scale, kResonanceNodeCount);
    }
    return std::nullopt;
}

void MainWindow::keepBranchingCurveAsPath() {
    if (!branchCurveSession_.isActive() || branchCurveSession_.branches().empty()) {
        return;
    }
    // The tree lives on whichever layer its trunk was drawn on.
    const auto trunkId = branchCurveSession_.trunkOperationId();
    std::optional<sound_mind::core::LayerId> layer;
    const auto& log = project_->operationLog();
    for (std::size_t i = 0; i < log.size() && trunkId.has_value(); ++i) {
        if (log.at(i).id() == *trunkId) {
            layer = log.at(i).targetLayer();
            break;
        }
    }
    if (!layer.has_value()) {
        layer = layerController_->paintTargetLayerId();
    }
    if (layer.has_value()) {
        toolPaletteController_->addGuidePath(*layer, branchCurveSession_.branches(), tr("Kept branching path"));
    }
}

void MainWindow::createResonanceFromPickedPath() {
    if (!project_.has_value() || toolPaletteController_->selectedBranches().empty()) {
        return;
    }
    const QString defaultName = tr("Resonance %1").arg(project_->resonanceProfiles().size() + 1);
    const auto result = sound_mind::studio::promptSaveName(
        this, tr("Create Resonance"), tr("Name:"), defaultName,
        [this](const std::string& candidate) { return project_->resonanceProfileNameExists(candidate); });
    if (result.outcome == sound_mind::studio::SaveNameOutcome::Cancelled) {
        return;
    }
    if (!result.replacingExisting) {
        createResonanceFromPickedPathNamed(result.name);
        return;
    }
    const auto graph = *pickedResonanceGraph();
    const auto spectrum = sound_mind::core::computeWaveKernelSignature(graph, kResonanceSpectrumSize);
    auto& profiles = project_->resonanceProfiles();
    const auto it = std::find_if(profiles.begin(), profiles.end(),
                                  [&result](const auto& named) { return named.name == result.name; });
    if (it != profiles.end()) {
        it->spectrum = spectrum;
        it->sourceCurve = graph;
    }
    toolConfigurationPanel_->refreshResonanceProfiles();
    statusBar()->showMessage(tr("Created Resonance \"%1\".").arg(QString::fromStdString(result.name)), 5000);
}

void MainWindow::createResonanceFromPickedPathNamed(const std::string& name) {
    if (!project_.has_value()) {
        return;
    }
    const auto pickedGraph = pickedResonanceGraph();
    if (!pickedGraph.has_value()) {
        return;
    }

    const auto& graph = *pickedGraph;
    const auto spectrum = sound_mind::core::computeWaveKernelSignature(graph, kResonanceSpectrumSize);
    project_->addResonanceProfile(name, spectrum, graph);
    commitHistory(tr("Created Resonance \"%1\"").arg(QString::fromStdString(name)));
    // Unlike captureMindShot()/captureMindGrain() (wired through
    // SelectionController's own mindShotCaptured()/mindGrainCaptured()
    // signals, connected once in ToolPaletteController's own constructor),
    // this capture writes directly to project_ from MainWindow - so the
    // panel's own picker needs an explicit nudge here instead.
    toolConfigurationPanel_->refreshResonanceProfiles();
    statusBar()->showMessage(tr("Created Resonance \"%1\".").arg(QString::fromStdString(name)), 5000);
}

void MainWindow::startBranchingCurveFromPickedPath() {
    const auto path = toolPaletteController_->selectedPath();
    const auto operationId = toolPaletteController_->selectedOperationId();
    if (!path.has_value() || !operationId.has_value()) {
        return;
    }
    branchCurveSession_.start(*path, *operationId);
    statusBar()->showMessage(
        tr("Branching Curve started - Pick a point on it, draw a branch, Pick it, then Add Picked Path as Branch."),
        5000);
}

void MainWindow::addPickedPathAsBranch() {
    const auto path = toolPaletteController_->selectedPath();
    const auto operationId = toolPaletteController_->selectedOperationId();
    if (!path.has_value() || !operationId.has_value()) {
        return;
    }
    if (branchCurveSession_.addBranch(*path, *operationId)) {
        statusBar()->showMessage(
            tr("Branch added (%1 total). Pick a graft point for the next one, or finalize as a Resonance.")
                .arg(branchCurveSession_.branches().size()),
            5000);
    }
}

void MainWindow::cancelBranchingCurve() { branchCurveSession_.end(); }

void MainWindow::createResonanceFromPickedGraph() {
    if (!project_.has_value() || !branchCurveSession_.isActive()) {
        return;
    }
    const QString defaultName = tr("Resonance %1").arg(project_->resonanceProfiles().size() + 1);
    const auto result = sound_mind::studio::promptSaveName(
        this, tr("Create Resonance"), tr("Name:"), defaultName,
        [this](const std::string& candidate) { return project_->resonanceProfileNameExists(candidate); });
    if (result.outcome == sound_mind::studio::SaveNameOutcome::Cancelled) {
        return;
    }
    if (!result.replacingExisting) {
        createResonanceFromPickedGraphNamed(result.name);
        return;
    }
    const auto graph = sound_mind::core::curveGraphFromBranches(
        branchCurveSession_.branches(), sound_mind::core::frequencyToTimeScaleFor(project_->settings()),
        kResonanceNodeCount);
    const auto spectrum = sound_mind::core::computeWaveKernelSignature(graph, kResonanceSpectrumSize);
    auto& profiles = project_->resonanceProfiles();
    const auto it = std::find_if(profiles.begin(), profiles.end(),
                                  [&result](const auto& named) { return named.name == result.name; });
    if (it != profiles.end()) {
        it->spectrum = spectrum;
        it->sourceCurve = graph;
    }
    keepBranchingCurveAsPath();
    branchCurveSession_.end();
    toolConfigurationPanel_->refreshResonanceProfiles();
    statusBar()->showMessage(
        tr("Created Resonance \"%1\" from the branching curve.").arg(QString::fromStdString(result.name)), 5000);
}

void MainWindow::createResonanceFromPickedGraphNamed(const std::string& name) {
    if (!project_.has_value() || !branchCurveSession_.isActive()) {
        return;
    }

    const auto graph = sound_mind::core::curveGraphFromBranches(
        branchCurveSession_.branches(), sound_mind::core::frequencyToTimeScaleFor(project_->settings()),
        kResonanceNodeCount);
    const auto spectrum = sound_mind::core::computeWaveKernelSignature(graph, kResonanceSpectrumSize);
    project_->addResonanceProfile(name, spectrum, graph);
    keepBranchingCurveAsPath();
    commitHistory(tr("Created Resonance \"%1\"").arg(QString::fromStdString(name)));
    branchCurveSession_.end();
    // See createResonanceFromPickedPathNamed()'s own identical
    // comment - this capture writes directly to project_ too.
    toolConfigurationPanel_->refreshResonanceProfiles();
    statusBar()->showMessage(tr("Created Resonance \"%1\" from the branching curve.")
                                  .arg(QString::fromStdString(name)),
                              5000);
}

void MainWindow::noteBranchCurveGraftCandidate(sound_mind::core::TimeFrequencyPoint point) {
    if (!branchCurveSession_.isActive()) {
        return;
    }
    const auto operationId = toolPaletteController_->selectedOperationId();
    if (!operationId.has_value()) {
        return;
    }
    branchCurveSession_.noteGraftCandidate(*operationId, point);
}

void MainWindow::copySelection() {
    // A region Selection is copied as pixels, as always; otherwise the
    // Picked objects themselves are copied (v0.Y.77.1).
    if (toolPaletteController_->hasSelection()) {
        toolPaletteController_->copySelection();
        return;
    }
    if (const std::size_t copied = toolPaletteController_->copyPickedObjects(); copied > 0) {
        statusBar()->showMessage(tr("Copied %n object(s).", "", static_cast<int>(copied)), 5000);
    }
}

void MainWindow::cutSelection() { toolPaletteController_->cutSelection(); }

void MainWindow::paste() {
    // The paste target is resolved fresh, right now - independent of
    // whichever layer the clipboard was originally copied from - per
    // SelectionController::pasteInto()'s own docs.
    if (const auto layerId = layerController_->paintTargetLayerId();
        layerId.has_value() && toolPaletteController_->pastesObjects()) {
        // Copied objects paste as duplicates, selected, ready to drag into place.
        setPickModeEnabled(true);
        toolPaletteController_->pastePickedObjects(*layerId);
        return;
    }
    if (const auto layerId = layerController_->paintTargetLayerId(); layerId.has_value()) {
        const auto blendMode = selectionConfigurationPanel_->pasteBlendMode();
        if (const auto pastedId = toolPaletteController_->pasteInto(*layerId, blendMode); pastedId.has_value()) {
            // Immediately Pickable - move/modify/delete/restack all work
            // right away, with no separate switch-to-Pick-and-click-it
            // step needed to find it again.
            setPickModeEnabled(true);
            toolPaletteController_->selectOperation(*layerId, *pastedId);
        }
    }
}

void MainWindow::captureMindShot() {
    if (!project_.has_value() || !toolPaletteController_->hasSelection()) {
        return;
    }
    const QString defaultName =
        tr("Mind Shot %1").arg(QString::number(project_->mindShots().size() + 1));
    MindCaptureDialog dialog(tr("Capture Mind Shot"), defaultName, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto result = sound_mind::studio::resolveNameCollision(
        this, tr("Capture Mind Shot"), tr("Name:"), dialog.name(),
        [this](const std::string& candidate) { return project_->mindShotNameExists(candidate); });
    if (result.outcome == sound_mind::studio::SaveNameOutcome::Cancelled) {
        return;
    }
    if (!result.replacingExisting) {
        captureMindShotWithDetails(result.name, dialog.fundamentalFrequencyHz(), dialog.startTimeOffsetSeconds());
        return;
    }
    // Replace: ToolPaletteController::captureMindShot() always appends a
    // fresh entry (never an in-place update), so the simplest way to
    // reuse that same capture pipeline for Replace is to let it capture
    // under a new id, then fold the result into the pre-existing entry
    // of the same name (preserving that entry's own id) and discard the
    // now-duplicate new one.
    const auto newId = toolPaletteController_->captureMindShot(result.name, dialog.fundamentalFrequencyHz(),
                                                                  dialog.startTimeOffsetSeconds());
    if (!newId.has_value()) {
        return;
    }
    auto& shots = project_->mindShots();
    const auto newIt = std::find_if(shots.begin(), shots.end(), [&](const auto& s) { return s.id == *newId; });
    const auto oldIt = std::find_if(shots.begin(), shots.end(),
                                     [&](const auto& s) { return s.name == result.name && s.id != *newId; });
    if (newIt != shots.end() && oldIt != shots.end()) {
        const auto oldId = oldIt->id;
        *oldIt = *newIt;
        oldIt->id = oldId;
        project_->removeMindShot(*newId);
    }
    commitHistory(tr("Captured Mind Shot \"%1\"").arg(QString::fromStdString(result.name)));
    statusBar()->showMessage(tr("Captured as \"%1\".").arg(QString::fromStdString(result.name)), 5000);
}

void MainWindow::captureMindShotWithDetails(const std::string& name, double fundamentalFrequencyHz,
                                              double startTimeOffsetSeconds) {
    if (!project_.has_value()) {
        return;
    }
    if (const auto id = toolPaletteController_->captureMindShot(name, fundamentalFrequencyHz, startTimeOffsetSeconds);
        id.has_value()) {
        commitHistory(tr("Captured Mind Shot \"%1\"").arg(QString::fromStdString(name)));
        statusBar()->showMessage(tr("Captured as \"%1\".").arg(QString::fromStdString(name)), 5000);
    }
}

void MainWindow::captureMindGrain() {
    if (!project_.has_value() || !toolPaletteController_->hasSelection()) {
        return;
    }
    const QString defaultName =
        tr("Mind Grain %1").arg(QString::number(project_->mindGrains().size() + 1));
    MindCaptureDialog dialog(tr("Capture Mind Grain"), defaultName, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto result = sound_mind::studio::resolveNameCollision(
        this, tr("Capture Mind Grain"), tr("Name:"), dialog.name(),
        [this](const std::string& candidate) { return project_->mindGrainNameExists(candidate); });
    if (result.outcome == sound_mind::studio::SaveNameOutcome::Cancelled) {
        return;
    }
    if (!result.replacingExisting) {
        captureMindGrainWithDetails(result.name, dialog.fundamentalFrequencyHz(), dialog.startTimeOffsetSeconds());
        return;
    }
    // Replace - see captureMindShot()'s own identical reasoning.
    const auto newId = toolPaletteController_->captureMindGrain(result.name, dialog.fundamentalFrequencyHz(),
                                                                   dialog.startTimeOffsetSeconds());
    if (!newId.has_value()) {
        return;
    }
    auto& grains = project_->mindGrains();
    const auto newIt = std::find_if(grains.begin(), grains.end(), [&](const auto& g) { return g.id == *newId; });
    const auto oldIt = std::find_if(grains.begin(), grains.end(),
                                     [&](const auto& g) { return g.name == result.name && g.id != *newId; });
    if (newIt != grains.end() && oldIt != grains.end()) {
        const auto oldId = oldIt->id;
        *oldIt = *newIt;
        oldIt->id = oldId;
        project_->removeMindGrain(*newId);
    }
    commitHistory(tr("Captured Mind Grain \"%1\"").arg(QString::fromStdString(result.name)));
    statusBar()->showMessage(tr("Captured as \"%1\".").arg(QString::fromStdString(result.name)), 5000);
}

void MainWindow::captureMindGrainWithDetails(const std::string& name, double fundamentalFrequencyHz,
                                               double startTimeOffsetSeconds) {
    if (!project_.has_value()) {
        return;
    }
    if (const auto id =
            toolPaletteController_->captureMindGrain(name, fundamentalFrequencyHz, startTimeOffsetSeconds);
        id.has_value()) {
        commitHistory(tr("Captured Mind Grain \"%1\"").arg(QString::fromStdString(name)));
        statusBar()->showMessage(tr("Captured as \"%1\".").arg(QString::fromStdString(name)), 5000);
    }
}

void MainWindow::saveConvolutionKernel(int size, std::vector<float> coefficients, bool normalize) {
    if (!project_.has_value()) {
        return;
    }
    const QString defaultName = tr("Kernel %1").arg(project_->convolutionKernels().size() + 1);
    const auto result = sound_mind::studio::promptSaveName(
        this, tr("Save Kernel"), tr("Name:"), defaultName,
        [this](const std::string& candidate) { return project_->convolutionKernelNameExists(candidate); });
    if (result.outcome == sound_mind::studio::SaveNameOutcome::Cancelled) {
        return;
    }
    if (result.replacingExisting) {
        auto& kernels = project_->convolutionKernels();
        const auto it = std::find_if(kernels.begin(), kernels.end(),
                                      [&result](const auto& named) { return named.name == result.name; });
        if (it != kernels.end()) {
            it->size = size;
            it->coefficients = std::move(coefficients);
            it->normalize = normalize;
        }
    } else {
        project_->addConvolutionKernel(result.name, size, std::move(coefficients), normalize);
    }
    commitHistory(tr("Saved kernel \"%1\"").arg(QString::fromStdString(result.name)));
    refreshConvolutionKernelCombo();
    applyProjectChange(ProjectChange::resources());
    statusBar()->showMessage(tr("Saved as \"%1\".").arg(QString::fromStdString(result.name)), 5000);
}

void MainWindow::refreshConvolutionKernelCombo() {
    filterConfigurationPanel_->setAvailableConvolutionKernels(
        project_.has_value() ? project_->convolutionKernels() : std::vector<sound_mind::core::NamedConvolutionKernel>{});
}

void MainWindow::saveFilterPreset(const sound_mind::core::FilterConfiguration& config) {
    if (!project_.has_value()) {
        return;
    }
    const QString defaultName = tr("Filter Preset %1").arg(project_->filterPresets().size() + 1);
    const auto result = sound_mind::studio::promptSaveName(
        this, tr("Save Filter Preset"), tr("Name:"), defaultName,
        [this](const std::string& candidate) { return project_->filterPresetNameExists(candidate); });
    if (result.outcome == sound_mind::studio::SaveNameOutcome::Cancelled) {
        return;
    }
    if (result.replacingExisting) {
        auto& presets = project_->filterPresets();
        const auto it = std::find_if(presets.begin(), presets.end(),
                                      [&result](const auto& named) { return named.name == result.name; });
        if (it != presets.end()) {
            it->config = config;
        }
    } else {
        project_->addFilterPreset(result.name, config);
    }
    commitHistory(tr("Saved filter preset \"%1\"").arg(QString::fromStdString(result.name)));
    refreshFilterPresetCombo();
    filterConfigurationPanel_->setActivePresetName(QString::fromStdString(result.name));
    applyProjectChange(ProjectChange::resources());
    statusBar()->showMessage(tr("Saved as \"%1\".").arg(QString::fromStdString(result.name)), 5000);
}

void MainWindow::saveGridPreset() {
    if (!project_.has_value()) {
        return;
    }
    const QString defaultName = tr("Grid Preset %1").arg(project_->gridPresets().size() + 1);
    const auto result = sound_mind::studio::promptSaveName(
        this, tr("Save Grid Preset"), tr("Name:"), defaultName,
        [this](const std::string& candidate) { return project_->gridPresetNameExists(candidate); });
    if (result.outcome == sound_mind::studio::SaveNameOutcome::Cancelled) {
        return;
    }
    auto frequencyGrid = sound_mind::studio::toFrequencyGridPresetConfig(gridPanel_->frequencyGridConfig());
    auto timingGrid = sound_mind::studio::toTimingGridPresetConfig(gridPanel_->timingGridConfig());
    const bool snapToGridEnabled = gridPanel_->snapToGridEnabled();
    if (result.replacingExisting) {
        auto& presets = project_->gridPresets();
        const auto it = std::find_if(presets.begin(), presets.end(),
                                      [&result](const auto& named) { return named.name == result.name; });
        if (it != presets.end()) {
            it->frequencyGrid = std::move(frequencyGrid);
            it->timingGrid = std::move(timingGrid);
            it->snapToGridEnabled = snapToGridEnabled;
        }
    } else {
        project_->addGridPreset(result.name, std::move(frequencyGrid), std::move(timingGrid), snapToGridEnabled);
    }
    commitHistory(tr("Saved grid preset \"%1\"").arg(QString::fromStdString(result.name)));
    refreshGridPresetCombo();
    gridPanel_->setActivePresetName(QString::fromStdString(result.name));
    applyProjectChange(ProjectChange::resources());
    statusBar()->showMessage(tr("Saved as \"%1\".").arg(QString::fromStdString(result.name)), 5000);
}

void MainWindow::deleteGridPreset(sound_mind::core::GridPresetId id) {
    if (!project_.has_value()) {
        return;
    }
    project_->removeGridPreset(id);
    commitHistory(tr("Deleted grid preset"));
    refreshGridPresetCombo();
    applyProjectChange(ProjectChange::resources());
}

void MainWindow::refreshGridPresetCombo() {
    gridPanel_->setAvailableGridPresets(project_.has_value() ? project_->gridPresets()
                                                               : std::vector<sound_mind::core::NamedGridPreset>{});
}

void MainWindow::refreshFilterPresetCombo() {
    filterConfigurationPanel_->setAvailableFilterPresets(
        project_.has_value() ? project_->filterPresets() : std::vector<sound_mind::core::NamedFilterPreset>{});
}

void MainWindow::startPlayback() {
    if (macroPlayer_->isActive()) {
        if (macroPlayer_->hasStartedPlaying()) {
            playbackController_->play();  // resumes a paused performance.
        }
        return;
    }
    if (filterPreviewActive_) {
        setFilterPreviewEnabled(false);  // one thing heard at a time.
    }
    if (!project_) {
        return;
    }
    if (isLoopModeRunning() || recordEngine_.isRecording()) {
        // Mutually exclusive with Loop Mode and Recording - see
        // toggleLoopMode()'s/toggleRecording()'s docs.
        return;
    }

    if (!playbackController_->isLoaded()) {
        playbackPreparer_->prepareAndPlay();  // composites in the background, then plays.
        return;
    }

    // Recovers a lost/never-opened output device if it can, and otherwise
    // says plainly why nothing (or the wrong thing) will be heard - playback
    // starting "successfully" with no sound and no explanation is the
    // failure mode this guards against.
    const QString outputWarning = playbackController_->ensureOutputReady(configuredOutputDeviceName_);
    playbackController_->play();
    if (!outputWarning.isEmpty()) {
        statusBar()->showMessage(outputWarning, 20000);
    }
    noteMacroPlayhead();
}

void MainWindow::pausePlayback() {
    // Position bar/playhead stay where they are - only stopPlayback()
    // resets them, matching "startPlayback() resumes from the same
    // position" - no point polling a position that isn't moving.
    playbackController_->pause();
}

void MainWindow::stopPlayback() {
    if (macroPlayer_->isActive()) {
        macroPlayer_->stop();
        return;
    }
    noteMacroPlayhead();  // where it stopped, before the position resets.
    playbackController_->stop();
    playbackPanel_->setDuration(0.0);
    canvas_->setPlayheadFraction(std::nullopt);
    // Layers Panel's own per-layer loudness indicator switches back to its
    // "stopped" value (average/peak over the whole layer) - v0.Y.52.1.
    layerController_->resetLoudnessDisplaysToStoppedValue();
}

void MainWindow::seekPlayback(double positionSeconds) {
    // Immediate feedback rather than waiting for the next timer tick (seek()
    // emits positionChanged() itself, synchronously) - a drag that ends
    // while paused should still show the new position right away.
    playbackController_->seek(positionSeconds);
    // A manual seek is deliberate user override - widen back to the whole
    // track, the same "forget any delta-range restriction" reasoning
    // setPlaybackRepeat(false)'s own docs give, so a subsequent edit's own
    // Delta/Review jump isn't fighting a range the user just moved away
    // from by hand.
    playbackPreparer_->widenToWholeTrack();
}

void MainWindow::setPlaybackOutputDevice(const QString& deviceName) {
    // Unlike Loop/Record's device preferences (only applied on their next
    // start()), PlaybackEngine's device is open for its whole lifetime, so
    // this switches immediately - see PlaybackEngine::setPreferredOutputDevice()'s
    // own docs.
    if (!playbackController_->setOutputDevice(deviceName)) {
        statusBar()->showMessage(
            tr("Could not switch to the selected output device: %1").arg(playbackController_->outputDeviceProblem()),
            15000);
    }
}

void MainWindow::setPlaybackVolume(int percent) {
    playbackController_->setVolume(percent);
}

void MainWindow::setHardwareAccelerationEnabled(bool enabled) {
    settings_.setValue(QStringLiteral("hardwareAccelerationEnabled"), enabled);
    sound_mind::core::setHardwareAccelerationEnabled(enabled);
    canvas_->invalidateCompositeAll();
}

void MainWindow::setSoundFlowerModeEnabled(bool enabled) {
    canvas_->setPolarMode(enabled);
    applyGridSnapping();
    if (enabled) {
        statusBar()->showMessage(tr("Sound Flower - polar view active"), 3000);
    }
}

void MainWindow::applyGridSnapping() {
    toolPaletteController_->setGridSnapping(
        gridPanel_->snapToGridEnabled() && !canvas_->polarMode(), gridPanel_->frequencyGridConfig(),
        gridPanel_->timingGridConfig());
}

void MainWindow::setPlaybackRepeat(bool enabled) { playbackPreparer_->setRepeat(enabled); }

void MainWindow::setPlaybackScope(sound_mind::studio::PlaybackScope scope) { playbackPreparer_->setScope(scope); }

void MainWindow::toggleLoopMode() { loopController_->toggle(); }



void MainWindow::setKeepLooping(bool keepLooping) { loopController_->setKeepLooping(keepLooping); }


bool MainWindow::isPlaying() const noexcept {
    return playbackController_->isPlaying();
}

sound_mind::core::LoopEngine* MainWindow::loopEngine() const noexcept {
    return loopController_ ? loopController_->engine() : nullptr;
}

bool MainWindow::isLoopModeRunning() const noexcept {
    return loopController_ && loopController_->isRunning();  // still constructing: not running.
}

bool MainWindow::keepLooping() const noexcept { return loopController_->keepLooping(); }

bool MainWindow::isRecording() const noexcept {
    return recordEngine_.isRecording();
}

QString MainWindow::loopInputDevice() const {
    return loopEngine() ? QString::fromStdString(loopEngine()->preferredInputDevice()) : QString();
}

QString MainWindow::loopOutputDevice() const {
    return loopEngine() ? QString::fromStdString(loopEngine()->preferredOutputDevice()) : QString();
}

QString MainWindow::recordInputDevice() const {
    return QString::fromStdString(recordEngine_.preferredInputDevice());
}

float MainWindow::loopInputGain() const noexcept {
    return loopEngine() ? loopEngine()->inputGain() : 1.0f;
}

float MainWindow::playbackVolume() const noexcept {
    return playbackController_->volume();
}

void MainWindow::toggleRecording() { recordingController_->toggle(); }

void MainWindow::refreshConfiguredDevices() {
    const QStringList inputDevices = toQStringList(recordEngine_.availableInputDeviceNames());
    const QStringList outputDevices = toQStringList(playbackController_->availableOutputDeviceNames());
    configureDevicesPanel_->setInputDevices(inputDevices);
    configureDevicesPanel_->setOutputDevices(outputDevices);
    // v0.Y.62.1 Installment H - the Landing Page's own independent
    // DeviceConfigurationWidget instance needs the same device list -
    // see its own docs on why there are two instances to keep in sync.
    landingPage_->deviceConfiguration()->setInputDevices(inputDevices);
    landingPage_->deviceConfiguration()->setOutputDevices(outputDevices);
}

void MainWindow::setConfiguredInputDevice(const QString& deviceName) {
    configuredInputDeviceName_ = deviceName;
    recordEngine_.setPreferredInputDevice(deviceName.toStdString());
    if (loopEngine()) {
        loopEngine()->setPreferredInputDevice(deviceName.toStdString());
    }
    // v0.Y.62.1 Installment H - keeps whichever DeviceConfigurationWidget
    // instance *didn't* originate this change showing the same selection
    // (see toggleTestInputDevice()'s own docs on this same "sync both,
    // unconditionally" shape - setSelectedInputDevice() blocks its own
    // signal, so this is a safe no-op on the originating instance too).
    configureDevicesPanel_->setSelectedInputDevice(deviceName);
    landingPage_->deviceConfiguration()->setSelectedInputDevice(deviceName);
}

void MainWindow::setConfiguredOutputDevice(const QString& deviceName) {
    configuredOutputDeviceName_ = deviceName;
    setPlaybackOutputDevice(deviceName);
    if (loopEngine()) {
        loopEngine()->setPreferredOutputDevice(deviceName.toStdString());
    }
    // See setConfiguredInputDevice()'s own docs.
    configureDevicesPanel_->setSelectedOutputDevice(deviceName);
    landingPage_->deviceConfiguration()->setSelectedOutputDevice(deviceName);
}

void MainWindow::setConfiguredInputGain(int percent) {
    const float gain = static_cast<float>(percent) / 100.0f;
    configuredInputGain_ = gain;
    recordEngine_.setInputGain(gain);
    // Kept consistent with the real configured gain, so "test" actually
    // reflects what real recording would sound/level like.
    deviceTestRecordEngine_.setInputGain(gain);
    if (loopEngine()) {
        loopEngine()->setInputGain(gain);
    }
    // See setConfiguredInputDevice()'s own docs on syncing both
    // DeviceConfigurationWidget instances' own displayed state.
    configureDevicesPanel_->setInputGainPercent(percent);
    landingPage_->deviceConfiguration()->setInputGainPercent(percent);
}

void MainWindow::setConfiguredOutputGain(int percent) {
    setPlaybackVolume(percent);
    deviceTestTonePlayer_.setGain(static_cast<float>(percent) / 100.0f);
    // See setConfiguredInputDevice()'s own docs.
    configureDevicesPanel_->setOutputGainPercent(percent);
    landingPage_->deviceConfiguration()->setOutputGainPercent(percent);
}

void MainWindow::toggleTestInputDevice(bool testing) {
    if (testing) {
        deviceTestRecordEngine_.setPreferredInputDevice(configuredInputDeviceName_.toStdString());
        deviceTestRecordEngine_.start();
        if (!deviceTestRecordEngine_.deviceProblem().empty()) {
            // A "Test" whose meter just sits at zero is indistinguishable
            // from a quiet room - say outright that the device didn't open.
            const QString problem = QString::fromStdString(deviceTestRecordEngine_.deviceProblem());
            deviceTestRecordEngine_.stop();
            testing = false;
            QMessageBox::warning(this, tr("Input device test"),
                                 tr("The input device could not be opened, so nothing can be tested.\n\n%1\n\n"
                                    "Pick a different input device, or check that no other program is using it "
                                    "and that microphone access is allowed in your system privacy settings.")
                                     .arg(problem));
        } else {
            testInputLevelTimer_->start();
        }
    } else {
        testInputLevelTimer_->stop();
        deviceTestRecordEngine_.stop();
        configureDevicesPanel_->setInputLevel(0.0f);
        landingPage_->deviceConfiguration()->setInputLevel(0.0f);
    }
    // v0.Y.62.1 Installment H - this may have been triggered by either
    // DeviceConfigurationWidget instance (the dock's own, or the Landing
    // Page's own); both need their own "Test" button's checked state
    // kept in sync regardless of which one the user actually clicked.
    // setTestingInput() blocks its own signal, so calling it on the
    // originating widget too is a safe, idempotent no-op rather than a
    // feedback loop.
    configureDevicesPanel_->setTestingInput(testing);
    landingPage_->deviceConfiguration()->setTestingInput(testing);
}

void MainWindow::toggleTestOutputDevice(bool testing) {
    if (testing) {
        deviceTestTonePlayer_.start(configuredOutputDeviceName_.toStdString());
        if (!deviceTestTonePlayer_.deviceProblem().empty()) {
            const QString problem = QString::fromStdString(deviceTestTonePlayer_.deviceProblem());
            deviceTestTonePlayer_.stop();
            testing = false;
            QMessageBox::warning(this, tr("Output device test"),
                                 tr("The output device could not be opened, so no test tone can play.\n\n%1\n\n"
                                    "Pick a different output device, or check that no other program is using "
                                    "it exclusively.")
                                     .arg(problem));
        }
    } else {
        deviceTestTonePlayer_.stop();
    }
    // See toggleTestInputDevice()'s own docs on why both instances are
    // synced unconditionally.
    configureDevicesPanel_->setTestingOutput(testing);
    landingPage_->deviceConfiguration()->setTestingOutput(testing);
}


bool MainWindow::commitRecording(const sound_mind::codec::AudioBuffer& captured) {
    return recordingController_->commit(captured);
}



void MainWindow::commitRecordingAsync(sound_mind::codec::AudioBuffer captured) {
    recordingController_->commitAsync(std::move(captured));
}

bool MainWindow::isSavingRecording() const noexcept { return recordingController_->isSaving(); }

void MainWindow::flushRecordingSave() { recordingController_->flushSave(); }

void MainWindow::refreshRecordTargets() {
    if (recordingController_) {  // still constructing: no project yet.
        recordingController_->refreshTargets();
    }
}

void MainWindow::pollTestInputLevel() {
    // Keeps deviceTestRecordEngine_'s own ring buffer from overflowing -
    // the accumulated capturedAudio() itself is never used for testing,
    // only currentInputLevel() below, but letting the ring fill up would
    // start dropping samples mid-block, same reasoning as Record's drain.
    deviceTestRecordEngine_.drainAvailable();
    const float level = deviceTestRecordEngine_.currentInputLevel();
    configureDevicesPanel_->setInputLevel(level);
    landingPage_->deviceConfiguration()->setInputLevel(level);
}

void MainWindow::applyProjectChange(const ProjectChange& change) {
    hasUnsavedChanges_ = true;
    switch (change.canvas) {
        case ProjectChange::Canvas::Unchanged:
            break;
        case ProjectChange::Canvas::FromLayer:
            canvas_->invalidateCompositeFrom(change.fromLayerIndex);
            break;
        case ProjectChange::Canvas::All:
            canvas_->invalidateCompositeAll();
            break;
    }
    if (change.layerList) {
        layerController_->refreshLayersPanel();
    } else if (change.thumbnail.has_value()) {
        layerController_->refreshLayersPanel(*change.thumbnail);
    }
    if (change.audible) {
        playbackController_->invalidate();  // also marks the analysis stale (invalidated()).
    } else if (change.analysis) {
        noteProjectChangedForAnalysis();
    }
    if (change.history) {
        refreshHistoryPanel();
    }
    if (change.composer) {
        refreshComposerPanel();
    }
    if (change.recordTargets) {
        refreshRecordTargets();
    }
}

void MainWindow::handleMindWaveEdited(sound_mind::core::MindWaveId id) {
    if (!project_) {
        return;
    }
    const sound_mind::core::MindWaveDependents dependents = sound_mind::core::mindWaveDependents(*project_, id);
    if (dependents.layers.empty()) {
        return;  // nothing seen or heard uses it.
    }
    {
        const QScopedValueRollback<bool> replaying(replayingForMindWave_, true);
        for (const sound_mind::core::LayerId layer : dependents.paintedLayers) {
            // Strokes resolve their MindWaves live, so their replay cache is
            // stale; replaying refreshes the layer (and its Mind Grain users).
            toolPaletteController_->invalidateContentCache(layer);
            toolPaletteController_->rebuildLayerContent(layer);
        }
    }
    ProjectChange change;
    change.canvas = ProjectChange::Canvas::FromLayer;
    change.fromLayerIndex = *dependents.lowestStackIndex;
    change.audible = true;
    applyProjectChange(change);
}

void MainWindow::handleContentChangedForPlayback(sound_mind::core::LayerId layer) {
    if (!project_ || replayingForMindWave_) {
        return;
    }
    playbackPreparer_->contentChanged(layer);
}

bool MainWindow::isPreparingEditPlayback() const noexcept { return playbackPreparer_->isPreparingEdit(); }


bool MainWindow::isCompositingForPlayback() const noexcept { return playbackPreparer_->isCompositing(); }

void MainWindow::scheduleRepeatRefresh() { playbackPreparer_->scheduleRepeatRefresh(); }

void MainWindow::setFilterPreviewEnabled(bool enabled) {
    if (!enabled || !project_) {
        filterPreviewActive_ = false;
        filterPreviewDirty_ = false;
        filterPreviewJob_->requestCancel();  // its completion drops the render.
        if (filterPreviewEngine_) {
            filterPreviewEngine_->stop();
        }
        if (filterConfigurationPanel_ != nullptr) {
            filterConfigurationPanel_->setPreviewPlaying(false);
        }
        return;
    }
    if (filterPreviewActive_) {
        return;
    }
    if (playbackController_->isPlaying()) {
        pausePlayback();
    }
    const auto config = sound_mind::core::streamCodecConfigFor(project_->settings());
    const double canvasSeconds = static_cast<double>(project_->settings().canvasWidth) *
                                 static_cast<double>(config.hopLength) / static_cast<double>(config.sampleRateHz);
    const auto selection = toolPaletteController_->committedSelection();
    filterPreviewWindow_ = sound_mind::core::filterPreviewWindow(
        currentPlaybackPositionSeconds_,
        selection.has_value() ? std::optional<sound_mind::core::TimeFrequencyRect>(selection->bounds) : std::nullopt,
        canvasSeconds);
    if (!filterPreviewEngine_) {
        filterPreviewEngine_ = std::make_unique<sound_mind::core::PlaybackEngine>(audioDeviceMode_);
    }
    filterPreviewEngine_->stop();
    filterPreviewActive_ = true;
    filterConfigurationPanel_->setPreviewPlaying(true);
    statusBar()->showMessage(tr("Preparing filter preview..."), 3000);
    scheduleFilterPreviewRender();
}

std::size_t MainWindow::filterPreviewLengthSamples() const noexcept {
    return filterPreviewEngine_ ? filterPreviewEngine_->totalSamples() : 0;
}

void MainWindow::scheduleFilterPreviewRender() {
    if (!filterPreviewActive_ || !project_) {
        return;
    }
    if (filterPreviewJob_->isRunning()) {
        filterPreviewDirty_ = true;
        return;
    }
    filterPreviewDirty_ = false;

    sound_mind::core::FilterPreviewRequest request;
    request.config = filterConfigurationPanel_->filterConfiguration();
    if (const auto selected = layersPanel_->selectedLayerId()) {
        const sound_mind::core::Layer* layer = project_->layerById(*selected);
        if (layer != nullptr && sound_mind::core::isFilterLayerType(layer->type())) {
            request.filterLayer = *selected;
        }
    }
    request.selection = toolPaletteController_->committedSelection();
    request.startSeconds = filterPreviewWindow_.first;
    request.endSeconds = filterPreviewWindow_.second;
    filterPreviewJob_->start([this, projectCopy = *project_, request](
                                 sound_mind::core::CancellationToken& token) -> BackgroundJob::Completion {
        auto audio = std::make_shared<std::optional<sound_mind::codec::AudioBuffer>>();
        try {
            *audio = sound_mind::core::renderFilterPreview(projectCopy, request,
                                                           [&token]() { return token.cancellationRequested(); });
        } catch (const std::exception&) {
            audio->reset();  // cancelled or failed: keep looping the older audio.
        }
        return [this, audio](bool) { finishFilterPreviewRender(std::move(*audio)); };
    });
}

void MainWindow::finishFilterPreviewRender(std::optional<sound_mind::codec::AudioBuffer> result) {

    if (!filterPreviewActive_) {
        return;
    }
    if (filterPreviewDirty_) {
        scheduleFilterPreviewRender();
        return;
    }
    if (!result.has_value() || result->frameCount() == 0) {
        if (filterPreviewEngine_->totalSamples() == 0) {
            statusBar()->showMessage(tr("Nothing to preview - the project has no content to filter yet."), 5000);
            setFilterPreviewEnabled(false);
        }
        return;
    }
    ++filterPreviewRenders_;
    if (filterPreviewEngine_->isPlaying() && filterPreviewEngine_->stageAudio(std::move(*result))) {
        return;
    }
    filterPreviewEngine_->stop();
    filterPreviewEngine_->loadAudio(std::move(*result));
    filterPreviewEngine_->setPlaybackRange(/*loopEnabled=*/true, filterPreviewEngine_->totalSamples(), 0);
    filterPreviewEngine_->play();
    if (!filterPreviewEngine_->deviceProblem().empty()) {
        statusBar()->showMessage(tr("Filter preview: no sound will be heard - %1")
                                     .arg(QString::fromStdString(filterPreviewEngine_->deviceProblem())),
                                 10000);
    }
}

void MainWindow::cancelPlaybackComposite() { playbackPreparer_->cancelComposite(); }

}  // namespace sound_mind::studio
