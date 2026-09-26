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
#include <string>

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QImage>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QUrl>

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/video_export.h"
#include "sound_mind/core/compositor.h"
#include "sound_mind/core/gpu_compute_availability.h"
#include "sound_mind/core/gradient.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/layer_export.h"
#include "sound_mind/core/mind_grain.h"
#include "sound_mind/core/operation.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/pooling.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/about_dialog.h"
#include "sound_mind/studio/audio_snippet_picker_dialog.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/color_conversion.h"
#include "sound_mind/studio/create_project_wizard.h"
#include "sound_mind/studio/fill_gradient_dialog.h"
#include "sound_mind/studio/image_scale_picker_dialog.h"
#include "sound_mind/studio/import_export.h"
#include "sound_mind/studio/import_helpers.h"
#include "sound_mind/studio/landing_page.h"
#include "sound_mind/studio/layers_panel.h"
#include "sound_mind/studio/loop_panel.h"
#include "sound_mind/studio/mind_waves_panel.h"
#include "sound_mind/studio/playback_controller.h"
#include "sound_mind/studio/playback_panel.h"
#include "sound_mind/studio/qt_image_conversion.h"
#include "sound_mind/studio/record_panel.h"
#include "sound_mind/studio/theme.h"

#ifndef SOUND_MIND_VERSION
#define SOUND_MIND_VERSION "unknown"
#endif

namespace sound_mind::studio {

namespace {
const char* kProjectFileFilter = "Sound Mind Projects (*.smproj)";
const char* kAudioFileFilter = "Audio (*.wav *.mp3 *.flac *.ogg *.aiff *.aif *.m4a *.opus)";
const char* kImageFileFilter = "Images (*.png *.jpg *.jpeg *.bmp *.tga *.webp)";
const char* kExportAudioFileFilter = "FLAC Audio (*.flac);;Ogg Vorbis Audio (*.ogg);;MP3 Audio (*.mp3)";
const char* kExportVideoFileFilter = "MP4 Video (*.mp4)";

/// @brief The opacity step increaseSelectedLayerOpacity()/
/// decreaseSelectedLayerOpacity() apply per keypress - `v0.Y.46.1`
/// Installment A ("Layers Panel & Editing Enhancements v2"), confirmed
/// with the user.
constexpr float kLayerOpacityNudgeStep = 0.05f;

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
/// Import/Pool/Export Audio are still synchronous, blocking calls -
/// without the explicit processEvents() call, Qt wouldn't actually paint
/// the status bar's new text until *after* the blocking call already
/// returned, defeating the whole point of showing progress before a slow
/// operation starts. Export Video is the first exception (real-world
/// testing pass, 2026-09-20, finding #12, Installment C) - see
/// exportTopmostLayerVideoAsync()'s own docs for why it doesn't need this
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
    connect(canvas_, &CanvasWidget::zoomModeChanged, this, [this](CanvasWidget::ZoomMode mode) {
        canvasScrollArea_->setWidgetResizable(mode == CanvasWidget::ZoomMode::FitToWindow);
    });

    stack_ = new QStackedWidget(this);
    stack_->addWidget(landingPage_);      // index 0 - shown first, see setProject().
    stack_->addWidget(canvasScrollArea_);  // index 1
    setCentralWidget(stack_);

    layersPanel_ = new LayersPanel(this);
    layersPanel_->hide();  // nothing to show until setProject() - see refreshLayersPanel()'s docs.
    addDockWidget(Qt::RightDockWidgetArea, layersPanel_);
    connect(layersPanel_, &LayersPanel::visibilityCycleRequested, this, &MainWindow::cycleLayerVisibilityState);
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
    connect(layersPanel_, &LayersPanel::reorderRequested, this, &MainWindow::reorderLayers);
    connect(layersPanel_, &LayersPanel::addLayerRequested, this, &MainWindow::addEmptyLayer);
    connect(layersPanel_, &LayersPanel::addFilterLayerRequested, this, &MainWindow::addFilterLayer);

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
        playbackPanel_->setPositionSeconds(positionSeconds);
        currentPlaybackPositionSeconds_ = positionSeconds;
        const double total = playbackController_->totalSeconds();
        canvas_->setPlayheadFraction(total > 0.0 ? std::optional<double>(positionSeconds / total) : std::nullopt);
        checkRepeatPlaybackRange(positionSeconds);
    });
    connect(playbackPanel_, &PlaybackPanel::repeatChanged, this, &MainWindow::setPlaybackRepeat);
    connect(playbackPanel_, &PlaybackPanel::scopeChanged, this, &MainWindow::setPlaybackScope);

    toolConfigurationPanel_ = new ToolConfigurationPanel(this);
    toolConfigurationPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, toolConfigurationPanel_);

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
    configureDevicesPanel_->setInputDevices(toQStringList(recordEngine_.availableInputDeviceNames()));
    configureDevicesPanel_->setOutputDevices(toQStringList(playbackController_->availableOutputDeviceNames()));
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

    testInputLevelTimer_ = new QTimer(this);
    testInputLevelTimer_->setInterval(100);  // same cadence as recordDrainTimer_ - see its own docs.
    connect(testInputLevelTimer_, &QTimer::timeout, this, &MainWindow::pollTestInputLevel);

    // Real-world testing pass, 2026-09-20, finding #12 (a real, non-blocking
    // cancel affordance for long operations), Installments C (video)/E
    // (audio) - see exportTopmostLayerVideoAsync()'s/
    // exportTopmostLayerAudioAsync()'s own docs. Hidden until an export is
    // actually running - see pollExportProgress().
    exportCancelButton_ = new QPushButton(tr("✕"));  // "✕".
    exportCancelButton_->setObjectName(QStringLiteral("exportCancelButton"));
    exportCancelButton_->setToolTip(tr("Cancel"));
    exportCancelButton_->setFixedWidth(24);
    exportCancelButton_->hide();
    connect(exportCancelButton_, &QPushButton::clicked, this, &MainWindow::cancelExport);
    statusBar()->addPermanentWidget(exportCancelButton_);

    exportProgressTimer_ = new QTimer(this);
    exportProgressTimer_->setInterval(33);  // same ~30fps cadence as loopUpdateTimer_.
    connect(exportProgressTimer_, &QTimer::timeout, this, &MainWindow::pollExportProgress);

    // Finding #12, Installment F - see importAudioSnippetsAsync()'s own
    // docs. A separate slot from export's own, not shared - unlike
    // Video/Audio export (both reading the same topmost layer, never
    // meaningfully run together), importing new content and exporting
    // existing content are independent operations with no reason to block
    // each other.
    importCancelButton_ = new QPushButton(tr("✕"));  // "✕".
    importCancelButton_->setObjectName(QStringLiteral("importCancelButton"));
    importCancelButton_->setToolTip(tr("Cancel"));
    importCancelButton_->setFixedWidth(24);
    importCancelButton_->hide();
    connect(importCancelButton_, &QPushButton::clicked, this, &MainWindow::cancelImport);
    statusBar()->addPermanentWidget(importCancelButton_);

    importProgressTimer_ = new QTimer(this);
    importProgressTimer_->setInterval(33);  // same ~30fps cadence as loopUpdateTimer_.
    connect(importProgressTimer_, &QTimer::timeout, this, &MainWindow::pollImportProgress);

    // Finding #12, Installment G - see poolTopmostLayerAsync()'s own docs.
    // A third, independent slot - not shared with export's or import's own.
    poolCancelButton_ = new QPushButton(tr("✕"));  // "✕".
    poolCancelButton_->setObjectName(QStringLiteral("poolCancelButton"));
    poolCancelButton_->setToolTip(tr("Cancel"));
    poolCancelButton_->setFixedWidth(24);
    poolCancelButton_->hide();
    connect(poolCancelButton_, &QPushButton::clicked, this, &MainWindow::cancelPool);
    statusBar()->addPermanentWidget(poolCancelButton_);

    poolProgressTimer_ = new QTimer(this);
    poolProgressTimer_->setInterval(33);  // same ~30fps cadence as loopUpdateTimer_.
    connect(poolProgressTimer_, &QTimer::timeout, this, &MainWindow::pollPoolProgress);

    // Finding #12, Installment H - see startPlayback()'s own docs. A
    // fourth, independent slot - not shared with export's/import's/pool's
    // own.
    compositeCancelButton_ = new QPushButton(tr("✕"));  // "✕".
    compositeCancelButton_->setObjectName(QStringLiteral("compositeCancelButton"));
    compositeCancelButton_->setToolTip(tr("Cancel"));
    compositeCancelButton_->setFixedWidth(24);
    compositeCancelButton_->hide();
    connect(compositeCancelButton_, &QPushButton::clicked, this, &MainWindow::cancelPlaybackComposite);
    statusBar()->addPermanentWidget(compositeCancelButton_);

    compositeProgressTimer_ = new QTimer(this);
    compositeProgressTimer_->setInterval(33);  // same ~30fps cadence as loopUpdateTimer_.
    connect(compositeProgressTimer_, &QTimer::timeout, this, &MainWindow::pollCompositeProgress);

    // Selection & Fill (v0.Y.25.1), Selection Type (v0.Y.35.1 Installment
    // A) - toolPaletteController_ isn't constructed until just below, but
    // this lambda only ever runs later, on a real dropdown change - safe
    // by the time it fires, the same reasoning every other lambda in this
    // constructor relies on.
    selectionConfigurationPanel_ = new SelectionConfigurationPanel(this);
    selectionConfigurationPanel_->hide();
    addDockWidget(Qt::RightDockWidgetArea, selectionConfigurationPanel_);
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
    toolPaletteController_ = new ToolPaletteController(canvas_, toolConfigurationPanel_, &undoStack_, this);
    // chordGeneratorPanel_'s own constructor already emitted paramsChanged()
    // once, before the connection above existed - this explicit initial
    // sync is the same "the panel's own constructed-with defaults are
    // already real, applied here" precedent ToolPaletteController's own
    // constructor follows for toolConfigurationPanel_ (see its own docs),
    // just done here instead since chordGeneratorPanel_ isn't passed into
    // ToolPaletteController's constructor at all (see that class's own docs
    // on why).
    toolPaletteController_->setChordParams(chordGeneratorPanel_->params());
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
    });
    connect(canvas_, &CanvasWidget::paintStrokeStarted, this, [this](sound_mind::core::TimeFrequencyPoint point) {
        if (const auto layerId = layerController_->paintTargetLayerId(); layerId.has_value()) {
            toolPaletteController_->beginPaintStroke(*layerId, point);
        }
    });
    connect(canvas_, &CanvasWidget::pickStrokeStarted, this, [this](sound_mind::core::TimeFrequencyPoint point) {
        if (const auto layerId = layerController_->paintTargetLayerId(); layerId.has_value()) {
            toolPaletteController_->beginPick(*layerId, point);
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
    connect(canvas_, &CanvasWidget::pathNodePlaced, this, [this](sound_mind::core::TimeFrequencyPoint point) {
        if (const auto layerId = layerController_->paintTargetLayerId(); layerId.has_value()) {
            toolPaletteController_->placePathNode(*layerId, point);
        }
    });
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
    // Merges all four tool controllers' own contentChanged() into one
    // connection - see ToolPaletteController::contentChanged()'s own
    // docs; it has already called canvas_->update() itself by this point.
    connect(toolPaletteController_, &ToolPaletteController::contentChanged, this,
            [this](sound_mind::core::LayerId layer) {
                hasUnsavedChanges_ = true;
                // `layer`'s own thumbnail cache entry is discarded and
                // freshly re-rendered - every other layer's own cached
                // thumbnail is reused as-is (see LayerController::
                // refreshLayersPanel()'s own docs on why this matters:
                // this fires after nearly every canvas edit).
                layerController_->refreshLayersPanel(layer);
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
    const auto applyGridSnapping = [this]() {
        toolPaletteController_->setGridSnapping(gridPanel_->snapToGridEnabled(), gridPanel_->frequencyGridConfig(),
                                                 gridPanel_->timingGridConfig());
    };
    connect(gridPanel_, &GridPanel::snapToGridChanged, this, applyGridSnapping);
    connect(gridPanel_, &GridPanel::frequencyGridConfigChanged, this, applyGridSnapping);
    connect(gridPanel_, &GridPanel::timingGridConfigChanged, this, applyGridSnapping);

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

    // Layer-stack lookup/mutation and Layers/Filter Configuration Panel
    // refresh - extracted as its own class (Refactor & Clean Up,
    // v0.Y.29.1, Installment D); see its own docs.
    layerController_ =
        new LayerController(canvas_, playbackController_, layersPanel_, filterConfigurationPanel_, &undoStack_, this);
    connect(layerController_, &LayerController::layersChanged, this, [this]() { hasUnsavedChanges_ = true; });

    // The MindWave library itself - add/remove/rename/edit - and keeping
    // mindWavesPanel_/layersPanel_'s own opacity-binding combo in sync
    // with it (v0.Y.31.1 Installment C2); see its own class docs.
    mindWaveController_ = new MindWaveController(mindWavesPanel_, layersPanel_, filterConfigurationPanel_,
                                                  toolConfigurationPanel_, canvas_, this);
    connect(mindWaveController_, &MindWaveController::mindWavesChanged, this, [this]() { hasUnsavedChanges_ = true; });

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
    connect(recordPanel_, &RecordPanel::toggleRequested, this, &MainWindow::toggleRecording);

    loopPanel_ = new LoopPanel(this);
    loopPanel_->hide();  // also needs setProject() - loopEngine_ doesn't exist until then.
    addDockWidget(Qt::RightDockWidgetArea, loopPanel_);
    connect(loopPanel_, &LoopPanel::toggleRequested, this, &MainWindow::toggleLoopMode);
    connect(loopPanel_, &LoopPanel::keepLoopingChanged, this, &MainWindow::setKeepLooping);

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

    // Basic Painting (v0.Y.24.1): Undo/Redo apply to paint strokes only so
    // far - the same scope PaintController's own undo()/redo() already
    // has (see its docs) - not a project-wide undo covering every kind of
    // change yet.
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction* undoAction = editMenu->addAction(tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, &MainWindow::undo);

    QAction* redoAction = editMenu->addAction(tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, &MainWindow::redo);

    // Pick (v0.Y.24.1): deletes whatever's currently selected - a no-op,
    // per deletePickedObject()'s own docs, when nothing is (rather than
    // disabling/enabling this action in sync with the selection, which
    // would need its own extra wiring for no real benefit here).
    QAction* deleteAction = editMenu->addAction(tr("&Delete"));
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deletePickedObject);

    editMenu->addSeparator();

    // Stack order (v0.0.26.2): moves the Picked object within its own
    // layer's stack, without changing what it is or where it geometrically
    // sits - see PickController::bringToFront()'s own docs. No-ops (same
    // "always present" choice deleteAction makes) with nothing Picked, or
    // when already at the requested end.
    //
    // Shortcuts moved off Ctrl+[/Ctrl+]/Ctrl+Shift+[/Ctrl+Shift+] (the
    // common Illustrator/Photoshop convention this originally used) to
    // Ctrl+Up/Down/Shift+Up/Down - see Canvas Navigation's own Zoom fix
    // (docs/sound-mind-architecture.md's Decision on it): the bracket keys
    // are now Zoom's own, and Ctrl+[/Ctrl+] specifically is Zoom's coarse
    // step, confirmed with the user as the side that wins this conflict.
    QAction* bringToFrontAction = editMenu->addAction(tr("Bring to &Front"));
    bringToFrontAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Up));
    connect(bringToFrontAction, &QAction::triggered, this, &MainWindow::bringPickedObjectToFront);

    QAction* sendToBackAction = editMenu->addAction(tr("Send to &Back"));
    sendToBackAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Down));
    connect(sendToBackAction, &QAction::triggered, this, &MainWindow::sendPickedObjectToBack);

    QAction* bringForwardAction = editMenu->addAction(tr("Bring &Forward"));
    bringForwardAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Up));
    connect(bringForwardAction, &QAction::triggered, this, &MainWindow::bringPickedObjectForward);

    QAction* sendBackwardAction = editMenu->addAction(tr("Send Back&ward"));
    sendBackwardAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Down));
    connect(sendBackwardAction, &QAction::triggered, this, &MainWindow::sendPickedObjectBackward);

    editMenu->addSeparator();

    // Path node/handle editing (v0.0.26.4) - see PickController::
    // beginPathEdit()'s own docs. No standard shortcuts (matching Fill
    // Selection's own no-shortcut choice above); each is a no-op when
    // inapplicable, the same "always present" choice deleteAction makes.
    QAction* editPathAction = editMenu->addAction(tr("&Edit Path"));
    connect(editPathAction, &QAction::triggered, this, &MainWindow::editPickedPath);

    QAction* toggleNodeTypeAction = editMenu->addAction(tr("Toggle &Node Type"));
    connect(toggleNodeTypeAction, &QAction::triggered, this, &MainWindow::togglePickedPathNodeType);

    QAction* applyPathEditAction = editMenu->addAction(tr("&Apply Path Edit"));
    connect(applyPathEditAction, &QAction::triggered, this, &MainWindow::applyPickedPathEdit);

    QAction* cancelPathEditAction = editMenu->addAction(tr("Cancel Pat&h Edit"));
    connect(cancelPathEditAction, &QAction::triggered, this, &MainWindow::cancelPickedPathEdit);

    editMenu->addSeparator();

    // Selection & Fill (v0.Y.25.1): the standard "clear the current
    // selection" shortcut/name every other image/vector editor already
    // uses - a no-op, per deselect()'s own docs, when there isn't one.
    QAction* deselectAction = editMenu->addAction(tr("D&eselect"));
    deselectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(deselectAction, &QAction::triggered, this, &MainWindow::deselect);

    // No standard shortcut for this one (unlike Delete/Deselect above) -
    // matching Pool Layer's own no-shortcut toolbar action below.
    QAction* fillAction = editMenu->addAction(tr("&Fill Selection..."));
    connect(fillAction, &QAction::triggered, this, &MainWindow::fillSelection);

    editMenu->addSeparator();

    // Cut/Copy/Paste (v0.Y.25.2): the standard shortcuts every other
    // editor already uses. Cut/Copy are no-ops (per copySelection()'s/
    // cutSelection()'s own docs) with no committed selection; Paste is a
    // no-op with nothing on the clipboard - none of the three are enabled/
    // disabled in sync with that state, the same "always present, no-op
    // when inapplicable" choice deleteAction above already makes.
    QAction* cutAction = editMenu->addAction(tr("Cu&t"));
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, this, &MainWindow::cutSelection);

    QAction* copyAction = editMenu->addAction(tr("&Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, &MainWindow::copySelection);

    QAction* pasteAction = editMenu->addAction(tr("&Paste"));
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, &MainWindow::paste);

    // Mind Shots (v0.Y.33.1 Installment A) - no standard shortcut (matching
    // Fill Selection's own no-shortcut choice above); a no-op with no
    // committed selection, the same "always present" choice deleteAction/
    // Cut/Copy/Paste all make.
    QAction* captureMindShotAction = editMenu->addAction(tr("Capture as &Mind Shot"));
    connect(captureMindShotAction, &QAction::triggered, this, &MainWindow::captureMindShot);

    // Mind Grains (v0.Y.33.1 Installment B) - the same "no shortcut, no-op
    // with no committed selection" treatment as Mind Shot's own action
    // right above.
    QAction* captureMindGrainAction = editMenu->addAction(tr("Capture as Mind &Grain"));
    connect(captureMindGrainAction, &QAction::triggered, this, &MainWindow::captureMindGrain);

    // MindWaves v2, Installment B (v0.Y.39.1) - no standard shortcut
    // (matching Fill Selection's own no-shortcut choice above); a no-op
    // with nothing suitable Picked, the same "always present" choice
    // deleteAction/Cut/Copy/Paste all make.
    QAction* usePickedPathAsMindWaveShapeAction = editMenu->addAction(tr("Use Picked Path as MindWave &Shape"));
    connect(usePickedPathAsMindWaveShapeAction, &QAction::triggered, this, &MainWindow::usePickedPathAsMindWaveShape);

    editMenu->addSeparator();

    // Layer navigation & manipulation (v0.Y.46.1 Installment A, "Layers
    // Panel & Editing Enhancements v2") - keyboard shortcuts for the
    // Layers Panel's own selection/reorder/opacity, so small adjustments
    // don't need reaching for the mouse every time. Ctrl+Up/Down/
    // Ctrl+Shift+Up/Down are already Bring Forward/Backward/to Front/to
    // Back (above) - Page Up/Down (plus Shift/Ctrl) was chosen instead,
    // confirmed with the user, to avoid that collision. Each is a no-op
    // (the same "always present" choice deleteAction makes) with no layer
    // selected, already at the relevant end of the stack, or blocked by a
    // locked Background/Equalizer neighbor - see LayerController::
    // selectLayerAbove()'s and its siblings' own docs.
    QAction* selectLayerAboveAction = editMenu->addAction(tr("Select Layer &Above"));
    selectLayerAboveAction->setShortcut(QKeySequence(Qt::Key_PageUp));
    connect(selectLayerAboveAction, &QAction::triggered, this, &MainWindow::selectLayerAbove);

    QAction* selectLayerBelowAction = editMenu->addAction(tr("Select Layer &Below"));
    selectLayerBelowAction->setShortcut(QKeySequence(Qt::Key_PageDown));
    connect(selectLayerBelowAction, &QAction::triggered, this, &MainWindow::selectLayerBelow);

    QAction* moveLayerUpAction = editMenu->addAction(tr("&Move Layer Up"));
    moveLayerUpAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_PageUp));
    connect(moveLayerUpAction, &QAction::triggered, this, &MainWindow::moveSelectedLayerUp);

    QAction* moveLayerDownAction = editMenu->addAction(tr("M&ove Layer Down"));
    moveLayerDownAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_PageDown));
    connect(moveLayerDownAction, &QAction::triggered, this, &MainWindow::moveSelectedLayerDown);

    QAction* increaseLayerOpacityAction = editMenu->addAction(tr("&Increase Layer Opacity"));
    increaseLayerOpacityAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp));
    connect(increaseLayerOpacityAction, &QAction::triggered, this, &MainWindow::increaseSelectedLayerOpacity);

    QAction* decreaseLayerOpacityAction = editMenu->addAction(tr("&Decrease Layer Opacity"));
    decreaseLayerOpacityAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown));
    connect(decreaseLayerOpacityAction, &QAction::triggered, this, &MainWindow::decreaseSelectedLayerOpacity);

    editMenu->addSeparator();

    // Paths & Grids (v0.Y.26.1): ends/discards the Path tool's own in-
    // progress node placement - see finishPath()'s/cancelPath()'s own
    // docs. No standard shortcut for either (matching Fill Selection's
    // own no-shortcut choice above); both are no-ops when nothing's being
    // placed, the same "always present" choice deleteAction makes.
    QAction* finishPathAction = editMenu->addAction(tr("&Finish Path"));
    connect(finishPathAction, &QAction::triggered, this, &MainWindow::finishPath);

    QAction* cancelPathAction = editMenu->addAction(tr("Cance&l Path"));
    connect(cancelPathAction, &QAction::triggered, this, &MainWindow::cancelPath);

    // Canvas Navigation's own Zoom feature (docs/sound-mind-design.md) -
    // keybinding mnemonic: Alt = frequency axis, Shift = time axis, Ctrl =
    // proportional control, repurposed here as "coarser step" since plain
    // ]/[ is already proportional by default (see CanvasWidget::
    // zoomInCoarse()'s own docs). Fit to Window/Actual Size follow the
    // common Ctrl+0/Ctrl+1 convention (Photoshop and others).
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    QMenu* zoomMenu = viewMenu->addMenu(tr("&Zoom"));

    QAction* zoomInAction = zoomMenu->addAction(tr("Zoom &In"));
    zoomInAction->setShortcut(QKeySequence(Qt::Key_BracketRight));
    connect(zoomInAction, &QAction::triggered, this, &MainWindow::zoomIn);

    QAction* zoomOutAction = zoomMenu->addAction(tr("Zoom &Out"));
    zoomOutAction->setShortcut(QKeySequence(Qt::Key_BracketLeft));
    connect(zoomOutAction, &QAction::triggered, this, &MainWindow::zoomOut);

    zoomMenu->addSeparator();

    QAction* zoomInFrequencyAction = zoomMenu->addAction(tr("Zoom In (&Frequency Only)"));
    zoomInFrequencyAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_BracketRight));
    connect(zoomInFrequencyAction, &QAction::triggered, this, &MainWindow::zoomInFrequencyOnly);

    QAction* zoomOutFrequencyAction = zoomMenu->addAction(tr("Zoom Out (F&requency Only)"));
    zoomOutFrequencyAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_BracketLeft));
    connect(zoomOutFrequencyAction, &QAction::triggered, this, &MainWindow::zoomOutFrequencyOnly);

    QAction* zoomInTimeAction = zoomMenu->addAction(tr("Zoom In (&Time Only)"));
    zoomInTimeAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_BracketRight));
    connect(zoomInTimeAction, &QAction::triggered, this, &MainWindow::zoomInTimeOnly);

    QAction* zoomOutTimeAction = zoomMenu->addAction(tr("Zoom Out (&Time Only)"));
    zoomOutTimeAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_BracketLeft));
    connect(zoomOutTimeAction, &QAction::triggered, this, &MainWindow::zoomOutTimeOnly);

    QAction* zoomInCoarseAction = zoomMenu->addAction(tr("Zoom In (&Coarse)"));
    zoomInCoarseAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
    connect(zoomInCoarseAction, &QAction::triggered, this, &MainWindow::zoomInCoarse);

    QAction* zoomOutCoarseAction = zoomMenu->addAction(tr("Zoom Out (Co&arse)"));
    zoomOutCoarseAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
    connect(zoomOutCoarseAction, &QAction::triggered, this, &MainWindow::zoomOutCoarse);

    zoomMenu->addSeparator();

    QAction* zoomToFitAction = zoomMenu->addAction(tr("&Fit to Window"));
    zoomToFitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(zoomToFitAction, &QAction::triggered, this, &MainWindow::zoomToFit);

    QAction* zoomToActualSizeAction = zoomMenu->addAction(tr("&Actual Size"));
    zoomToActualSizeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    connect(zoomToActualSizeAction, &QAction::triggered, this, &MainWindow::zoomToActualSize);

    // Workflow & Device Polish, Installment E: Hardware Acceleration toggle
    // (v0.0.42.5) - docs/sound-mind-design.md's own "Hardware acceleration
    // option" ("a menu option to toggle GPU acceleration"). Persisted via
    // settings_ (the same ini-format store recentProjects_ already uses),
    // defaulting to enabled - so it's a lasting preference, not a
    // session-only diagnostic switch, confirmed with the user. Initial
    // state is applied directly to sound_mind::core's own flag here
    // (not through setHardwareAccelerationEnabled(), which would also
    // needlessly re-save the exact same value back to settings_); the
    // QSignalBlocker keeps that initial setChecked() call from firing
    // toggled() and duplicating the work.
    QAction* hardwareAccelerationAction = viewMenu->addAction(tr("&Hardware Acceleration"));
    hardwareAccelerationAction->setCheckable(true);
    const bool hardwareAccelerationEnabled =
        settings_.value(QStringLiteral("hardwareAccelerationEnabled"), true).toBool();
    sound_mind::core::setHardwareAccelerationEnabled(hardwareAccelerationEnabled);
    {
        const QSignalBlocker blocker(hardwareAccelerationAction);
        hardwareAccelerationAction->setChecked(hardwareAccelerationEnabled);
    }
    connect(hardwareAccelerationAction, &QAction::toggled, this, &MainWindow::setHardwareAccelerationEnabled);

    // Workflow & Device Polish, Installment D: Documentation links
    // (v0.0.42.4) - each link is reachable from both the Help menu and the
    // Landing Page's own Documentation section (see LandingPage's own
    // docs), so each is wired to one shared lambda rather than duplicating
    // the same openUserDocOrShowFallback() call at two separate connect()
    // sites. Ordering matches docs/sound-mind-design.md's own "Quickstart,
    // Readme, and Userguide" list, with Changelog appended after (a legacy-
    // Studio precedent, confirmed with the user) and About last, separated
    // - the same layout the legacy Studio's own Help menu used.
    auto openQuickStart = [this]() {
        openUserDocOrShowFallback(QStringLiteral("quickstart.html"), tr("The Quick Start guide"));
    };
    auto openReadme = [this]() { openUserDocOrShowFallback(QStringLiteral("index.html"), tr("The Readme")); };
    auto openUserGuideDoc = [this]() {
        openUserDocOrShowFallback(QStringLiteral("user_guide.html"), tr("The User Guide"));
    };
    auto openChangelog = [this]() {
        openUserDocOrShowFallback(QStringLiteral("changelog.html"), tr("The Changelog"));
    };
    auto openAbout = [this]() { AboutDialog(this).exec(); };

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* quickStartAction = helpMenu->addAction(tr("&Quick Start"));
    connect(quickStartAction, &QAction::triggered, this, openQuickStart);
    QAction* readmeAction = helpMenu->addAction(tr("&Readme"));
    connect(readmeAction, &QAction::triggered, this, openReadme);
    QAction* userGuideAction = helpMenu->addAction(tr("&User Guide"));
    connect(userGuideAction, &QAction::triggered, this, openUserGuideDoc);
    QAction* changelogAction = helpMenu->addAction(tr("&Changelog"));
    connect(changelogAction, &QAction::triggered, this, openChangelog);
    helpMenu->addSeparator();
    QAction* aboutAction = helpMenu->addAction(tr("&About Sound Mind Studio..."));
    connect(aboutAction, &QAction::triggered, this, openAbout);

    connect(landingPage_, &LandingPage::quickStartRequested, this, openQuickStart);
    connect(landingPage_, &LandingPage::readmeRequested, this, openReadme);
    connect(landingPage_, &LandingPage::userGuideRequested, this, openUserGuideDoc);
    connect(landingPage_, &LandingPage::changelogRequested, this, openChangelog);
    connect(landingPage_, &LandingPage::aboutRequested, this, openAbout);

    QToolBar* transportToolBar = addToolBar(tr("Transport"));
    // Plain text actions rather than icons - no icon assets exist yet, and
    // these are unambiguous enough on their own for a first pass.
    QAction* poolAction = transportToolBar->addAction(tr("Pool Layer"));
    connect(poolAction, &QAction::triggered, this, &MainWindow::poolTopmostLayer);

    // A plain checkable toggle, not toggleViewAction()-based like the
    // panels below - this isn't a dock's own visibility, it's the
    // canvas's current tool mode (see CanvasWidget::setToolMode()'s own
    // docs).
    paintAction_ = transportToolBar->addAction(tr("Paint"));
    paintAction_->setCheckable(true);
    connect(paintAction_, &QAction::toggled, this, &MainWindow::setPaintModeEnabled);

    // Pick (v0.Y.24.1): the same kind of plain checkable toggle as Paint
    // above, for CanvasWidget::ToolMode::Pick. Deliberately *not* grouped
    // with paintAction_ via QActionGroup - see setPaintModeEnabled()'s own
    // docs on why mutual exclusivity is instead handled directly, by hand,
    // in setPaintModeEnabled()/setPickModeEnabled() themselves.
    pickAction_ = transportToolBar->addAction(tr("Pick"));
    pickAction_->setCheckable(true);
    connect(pickAction_, &QAction::toggled, this, &MainWindow::setPickModeEnabled);

    // Selection & Fill (v0.Y.25.1): the same kind of plain checkable
    // toggle as Paint/Pick above, for CanvasWidget::ToolMode::Select.
    selectAction_ = transportToolBar->addAction(tr("Select"));
    selectAction_->setCheckable(true);
    connect(selectAction_, &QAction::toggled, this, &MainWindow::setSelectModeEnabled);

    // Paths & Grids (v0.Y.26.1): the same kind of plain checkable toggle
    // as Paint/Pick/Select above, for CanvasWidget::ToolMode::Path.
    pathAction_ = transportToolBar->addAction(tr("Path"));
    pathAction_->setCheckable(true);
    connect(pathAction_, &QAction::toggled, this, &MainWindow::setPathModeEnabled);

    // Chords/Arpeggiator/Sequencer, Installment B (v0.0.40.2): the same
    // kind of plain checkable toggle as Paint/Pick/Select/Path above, for
    // CanvasWidget::ToolMode::ChordStamp - arms the Chord Generator's own
    // "click to place" gesture (see ChordGeneratorPanel's own docs on why
    // there's no separate "Stamp" button).
    chordAction_ = transportToolBar->addAction(tr("Chord"));
    chordAction_->setCheckable(true);
    connect(chordAction_, &QAction::toggled, this, &MainWindow::setChordModeEnabled);

    // The Path tool's own "standing default" node type (see
    // PathController::setDefaultNodeType()'s own docs) - deliberately
    // independent of tool-mode exclusivity (not reset by
    // setExclusiveToolMode(), not affected by switching tools) since it's
    // a placement preference, not a mode of its own.
    smoothNodesAction_ = transportToolBar->addAction(tr("Smooth Nodes"));
    smoothNodesAction_->setCheckable(true);
    connect(smoothNodesAction_, &QAction::toggled, this, &MainWindow::setPathPlacesSmoothNodes);

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
    // Off by default, the same as Playback/Record/Loop above - see
    // toolConfigurationPanel_'s own docs.
    transportToolBar->addAction(toolConfigurationPanel_->toggleViewAction());
    // Off by default, same reasoning - see chordGeneratorPanel_'s own docs.
    transportToolBar->addAction(chordGeneratorPanel_->toggleViewAction());
    // Off by default, same reasoning - see configureDevicesPanel_'s own docs.
    transportToolBar->addAction(configureDevicesPanel_->toggleViewAction());
    transportToolBar->addAction(selectionConfigurationPanel_->toggleViewAction());
    // Off by default, same reasoning - see gridPanel_'s own docs.
    transportToolBar->addAction(gridPanel_->toggleViewAction());
    // Off by default, same reasoning - see filterConfigurationPanel_'s own docs.
    transportToolBar->addAction(filterConfigurationPanel_->toggleViewAction());
    // Off by default, same reasoning - see mindWavesPanel_'s own docs.
    transportToolBar->addAction(mindWavesPanel_->toggleViewAction());

    // Zoom's own toolbar, added after transportToolBar (not before) so
    // findChild<QToolBar*>()'s own singular/first-match behavior - already
    // relied on by existing tests to reach transportToolBar specifically -
    // keeps finding it, not this one. The four most commonly reached-for
    // zoom actions - the axis-restricted/coarse variants stay
    // menu(+keyboard)-only, matching how e.g. Bring Forward/Send Backward
    // above are menu-only despite having shortcuts.
    QToolBar* viewToolBar = addToolBar(tr("View"));
    viewToolBar->addAction(zoomOutAction);
    viewToolBar->addAction(zoomInAction);
    viewToolBar->addAction(zoomToFitAction);
    viewToolBar->addAction(zoomToActualSizeAction);

    // ~30fps - frequent enough for each completed loop's spectrogram
    // update to read as prompt, without repainting so often it competes
    // noticeably with the background encode/decode worker thread (see
    // LoopEngine's docs) for CPU time.
    loopUpdateTimer_ = new QTimer(this);
    loopUpdateTimer_->setInterval(33);
    connect(loopUpdateTimer_, &QTimer::timeout, this, &MainWindow::updateLoopLayer);

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
    if (isImportRunning()) {
        statusBar()->showMessage(tr("Wait for the import to finish, or cancel it, before closing."), 5000);
        event->ignore();
        return;
    }
    if (isPoolRunning()) {
        statusBar()->showMessage(tr("Wait for pooling to finish, or cancel it, before closing."), 5000);
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
    for (const auto& path : paths) {
        const std::string extension = lowercasedExtension(path);
        if (isImageExtension(extension)) {
            imagePaths.push_back(path);
        } else if (isAudioExtension(extension)) {
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

    handleDroppedFiles(paths, imageMode, importImagesAsSequence, audioSnippetSelections, audioSnippetOffsets);
}

void MainWindow::handleDroppedFiles(const std::vector<std::filesystem::path>& paths,
                                     ImageScalePickerDialog::Mode imageMode, bool importImagesAsSequence,
                                     const std::map<std::filesystem::path, std::vector<std::size_t>>& audioSnippetSelections,
                                     const std::map<std::filesystem::path, double>& audioSnippetOffsets) {
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
    // A previous project's own undo/redo history is meaningless once it's
    // gone - its LayerIds/OperationIds could coincidentally collide with
    // the new project's own, the same risk layersPanel_->clearSelection()
    // just guarded against above.
    undoStack_.clear();

    project_ = std::move(project);

    // (Re)constructed fresh for the new project's own settings - loop
    // length is the project's own duration in samples (canvasWidth
    // timeline columns, each hopLength samples wide) - see the class docs'
    // v0.Y.12.1 note for why this replaced a fixed, always-default-
    // constructed member.
    const sound_mind::core::ProjectSettings& settings = project_->settings();
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    const auto loopLengthSamples = static_cast<std::size_t>(settings.canvasWidth) * config.hopLength;
    loopEngine_ = std::make_unique<sound_mind::core::LoopEngine>(config, loopLengthSamples, audioDeviceMode_);
    // Unlike recordEngine_ (a persistent member, never recreated), this is a
    // brand new engine with no memory of whatever was already configured in
    // Configure Devices - re-applying it here is what stops a project
    // switch from silently resetting Loop Mode back to system-default
    // devices at unity gain (real-world testing pass, 2026-09-20,
    // finding #10 - see configuredInputDeviceName_'s own docs).
    loopEngine_->setPreferredInputDevice(configuredInputDeviceName_.toStdString());
    loopEngine_->setPreferredOutputDevice(configuredOutputDeviceName_.toStdString());
    loopEngine_->setInputGain(configuredInputGain_);

    canvas_->setProject(&*project_);
    toolPaletteController_->setProject(&*project_);
    layerController_->setProject(&*project_);
    mindWaveController_->setProject(&*project_);
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
    refreshConvolutionKernelCombo();
    // The new project's own layer stack/active layer are both different
    // from whatever the guardrail last computed - see
    // updateMindGrainGuardrails()'s own docs.
    updateMindGrainGuardrails();
}

void MainWindow::newProject() {
    if ((loopEngine_ && loopEngine_->isRunning()) || recordEngine_.isRecording()) {
        statusBar()->showMessage(tr("Stop Loop Mode or Recording before starting a new project."), 5000);
        return;
    }
    if (isImportRunning()) {
        statusBar()->showMessage(tr("Wait for the import to finish, or cancel it, before starting a new project."),
                                  5000);
        return;
    }
    if (isPoolRunning()) {
        statusBar()->showMessage(tr("Wait for pooling to finish, or cancel it, before starting a new project."),
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
    if ((loopEngine_ && loopEngine_->isRunning()) || recordEngine_.isRecording()) {
        statusBar()->showMessage(tr("Stop Loop Mode or Recording before opening a project."), 5000);
        return;
    }
    if (isImportRunning()) {
        statusBar()->showMessage(tr("Wait for the import to finish, or cancel it, before opening a project."), 5000);
        return;
    }
    if (isPoolRunning()) {
        statusBar()->showMessage(tr("Wait for pooling to finish, or cancel it, before opening a project."), 5000);
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
    if ((loopEngine_ && loopEngine_->isRunning()) || recordEngine_.isRecording()) {
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
    if (isPoolRunning()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Wait for pooling to finish, or cancel it, before opening a project.");
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
                                                                                 QString* errorMessage,
                                                                                 double offsetSeconds) const {
    if (!project_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No project is open.");
        }
        return {};
    }
    return sound_mind::studio::audioSnippetsForFile(*project_, path, errorMessage, offsetSeconds);
}

void MainWindow::wireSnippetOffsetRecompute(AudioSnippetPickerDialog& dialog, const std::filesystem::path& path) {
    connect(&dialog, &AudioSnippetPickerDialog::offsetChanged, &dialog, [this, &dialog, path](double offsetSeconds) {
        QString errorMessage;
        dialog.setSnippets(audioSnippetsForFile(path, &errorMessage, offsetSeconds));
    });
}

bool MainWindow::importAudioSnippets(const std::filesystem::path& path, const std::vector<std::size_t>& snippetIndices,
                                      QString* errorMessage, double offsetSeconds) {
    if (!project_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No project is open.");
        }
        return false;
    }

    showBusyStatus(statusBar(), tr("Importing audio..."));
    const int importedCount =
        sound_mind::studio::importAudioSnippetsInto(*project_, path, snippetIndices, errorMessage, offsetSeconds);
    if (importedCount == 0) {
        statusBar()->clearMessage();
        return false;
    }

    canvas_->update();
    // The topmost layer just changed - the next startPlayback() should
    // pick up the newly imported one instead of whatever was loaded
    // before, rather than silently keep playing stale content.
    playbackController_->invalidate();
    hasUnsavedChanges_ = true;
    layerController_->refreshLayersPanel();
    statusBar()->showMessage(
        tr("Imported %1 layer(s) from \"%2\".").arg(importedCount).arg(QString::fromStdString(path.filename().string())),
        5000);
    return true;
}

void MainWindow::importAudioSnippetsAsync(const std::filesystem::path& path,
                                           const std::vector<std::size_t>& snippetIndices, double offsetSeconds) {
    if (!project_) {
        statusBar()->showMessage(tr("No project is open."), 5000);
        return;
    }
    if (isImportRunning()) {
        statusBar()->showMessage(tr("Already importing - wait for it to finish, or cancel it, first."), 5000);
        return;
    }

    importPath_ = path;
    importOutcome_ = ImportOutcome::Success;
    importErrorMessage_.clear();
    importedLayers_.clear();

    // settings copied by value - encodeAudioSnippets() never touches
    // project_ itself, so the background thread has nothing shared to
    // synchronize against - see this method's own docs.
    const auto settings = project_->settings();
    importTask_ = std::make_unique<sound_mind::core::BackgroundTask>(
        [this, path, snippetIndices, settings, offsetSeconds](sound_mind::core::CancellationToken& token) {
            QString encodeError;
            try {
                importedLayers_ = sound_mind::studio::encodeAudioSnippets(
                    settings, path, snippetIndices, [&token]() { return token.cancellationRequested(); },
                    &encodeError, offsetSeconds);
                if (importedLayers_.empty()) {
                    importOutcome_ = ImportOutcome::Failed;
                    importErrorMessage_ = encodeError;
                }
            } catch (const sound_mind::studio::ImportCancelled&) {
                importOutcome_ = ImportOutcome::Cancelled;
            } catch (const std::exception& e) {
                importOutcome_ = ImportOutcome::Failed;
                importErrorMessage_ = QString::fromStdString(e.what());
            }
        });
    importTask_->start();

    statusBar()->showMessage(tr("Importing audio..."));
    importCancelButton_->show();
    importProgressTimer_->start();
}

bool MainWindow::isImportRunning() const noexcept { return importTask_ != nullptr && importTask_->isRunning(); }

void MainWindow::cancelImport() {
    if (importTask_) {
        importTask_->requestCancel();
    }
}

void MainWindow::pollImportProgress() {
    if (importTask_->isRunning()) {
        return;
    }
    importProgressTimer_->stop();
    importCancelButton_->hide();

    switch (importOutcome_) {
        case ImportOutcome::Success: {
            const auto importedCount = static_cast<int>(importedLayers_.size());
            for (auto& layer : importedLayers_) {
                project_->addLayer(std::move(layer));
            }
            importedLayers_.clear();
            canvas_->update();
            playbackController_->invalidate();
            hasUnsavedChanges_ = true;
            layerController_->refreshLayersPanel();
            statusBar()->showMessage(tr("Imported %1 layer(s) from \"%2\".")
                                          .arg(importedCount)
                                          .arg(QString::fromStdString(importPath_.filename().string())),
                                      5000);
            break;
        }
        case ImportOutcome::Cancelled:
            // Nothing to roll back beyond discarding these - the encoded
            // layers were never added to project_ in the first place,
            // unlike Export's own "delete the partial file" rollback (see
            // Decision #136) - matching finding #12's confirmed "cancel
            // rolls back partial work" semantics with the simplest
            // rollback an import can have: not committing anything at all.
            importedLayers_.clear();
            statusBar()->showMessage(tr("Audio import cancelled."), 5000);
            break;
        case ImportOutcome::Failed:
            statusBar()->clearMessage();
            QMessageBox::critical(this, tr("Import Audio Failed"), importErrorMessage_);
            break;
    }
    importTask_.reset();
}

bool MainWindow::importImageFile(const std::filesystem::path& path, ImageScalePickerDialog::Mode mode,
                                  QString* errorMessage) {
    if (!project_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No project is open.");
        }
        return false;
    }

    showBusyStatus(statusBar(), tr("Importing image..."));
    if (!sound_mind::studio::importImageFileInto(*project_, path, mode, errorMessage)) {
        statusBar()->clearMessage();
        return false;
    }

    canvas_->update();
    playbackController_->invalidate();
    hasUnsavedChanges_ = true;
    layerController_->refreshLayersPanel();
    statusBar()->showMessage(tr("Imported \"%1\".").arg(QString::fromStdString(path.filename().string())), 5000);
    return true;
}

bool MainWindow::importImageFiles(const std::vector<std::filesystem::path>& paths, ImageScalePickerDialog::Mode mode,
                                   bool importAsSequence, QString* errorMessage) {
    if (!project_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No project is open.");
        }
        return false;
    }

    showBusyStatus(statusBar(), tr("Importing image..."));
    const int importedCount =
        sound_mind::studio::importImageFilesInto(*project_, paths, mode, importAsSequence, errorMessage);
    if (importedCount == 0) {
        statusBar()->clearMessage();
        return false;
    }

    canvas_->update();
    playbackController_->invalidate();
    hasUnsavedChanges_ = true;
    layerController_->refreshLayersPanel();
    statusBar()->showMessage(tr("Imported %1 file(s).").arg(importedCount), 5000);
    return true;
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
    exportTopmostLayerAudioAsync(path, *format);
}

void MainWindow::exportVideo() {
    const QString fileName =
        QFileDialog::getSaveFileName(this, tr("Export Video"), QString(), tr(kExportVideoFileFilter));
    if (fileName.isEmpty()) {
        return;
    }
    exportTopmostLayerVideoAsync(std::filesystem::path(fileName.toStdString()));
}

bool MainWindow::exportTopmostLayerAudioNow(const std::filesystem::path& path, QString* errorMessage) {
    sound_mind::core::Layer* layer = layerController_->topmostLayerWithContent();
    if (layer == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No layer with content to export.");
        }
        return false;
    }

    showBusyStatus(statusBar(), tr("Exporting audio..."));
    if (!sound_mind::studio::exportLayerAudioNow(*layer, path, errorMessage)) {
        statusBar()->clearMessage();
        return false;
    }
    statusBar()->showMessage(tr("Exported audio to \"%1\".").arg(QString::fromStdString(path.string())), 5000);
    return true;
}

bool MainWindow::exportTopmostLayerVideoNow(const std::filesystem::path& path, QString* errorMessage) {
    sound_mind::core::Layer* layer = layerController_->topmostLayerWithContent();
    if (layer == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No layer with content to export.");
        }
        return false;
    }

    showBusyStatus(statusBar(), tr("Exporting video..."));
    if (!sound_mind::studio::exportLayerVideoNow(*layer, path, project_->settings().canvasWidth, errorMessage)) {
        statusBar()->clearMessage();
        return false;
    }
    statusBar()->showMessage(tr("Exported video to \"%1\".").arg(QString::fromStdString(path.string())), 5000);
    return true;
}

void MainWindow::exportTopmostLayerVideoAsync(const std::filesystem::path& path) {
    if (isExportRunning()) {
        statusBar()->showMessage(tr("Already exporting - wait for it to finish, or cancel it, first."), 5000);
        return;
    }

    sound_mind::core::Layer* layer = layerController_->topmostLayerWithContent();
    const auto audio = layer != nullptr ? sound_mind::core::decodeLayerForExport(*layer) : std::nullopt;
    const auto canvas =
        audio.has_value() ? sound_mind::core::renderLayer(*layer, project_->settings().canvasWidth) : std::nullopt;
    if (!audio.has_value() || !canvas.has_value()) {
        // Both decodeLayerForExport()/renderLayer() failing after a
        // non-null layer would be the same "shouldn't happen given both
        // read from the same layer state" case exportLayerVideo()'s own
        // docs already note - treated identically here.
        QMessageBox::critical(this, tr("Export Video Failed"), tr("No layer with content to export."));
        return;
    }

    exportKind_ = ExportKind::Video;
    exportPath_ = path;
    exportOutcome_ = ExportOutcome::Success;
    exportErrorMessage_.clear();

    // audio/canvas captured by value: independent, owned copies the
    // background thread can safely touch with no synchronization needed
    // against project_/layer, which stay exclusively the UI thread's own -
    // see this method's own docs.
    exportTask_ = std::make_unique<sound_mind::core::BackgroundTask>(
        [this, path, audio = *audio, canvas = *canvas](sound_mind::core::CancellationToken& token) {
            try {
                sound_mind::codec::exportVideo(path, canvas, audio, /*frameRate=*/30,
                                                [&token]() { return token.cancellationRequested(); });
            } catch (const sound_mind::codec::ExportCancelled&) {
                exportOutcome_ = ExportOutcome::Cancelled;
            } catch (const std::exception& e) {
                exportOutcome_ = ExportOutcome::Failed;
                exportErrorMessage_ = QString::fromStdString(e.what());
            }
        });
    exportTask_->start();

    statusBar()->showMessage(tr("Exporting video..."));
    exportCancelButton_->show();
    exportProgressTimer_->start();
}

void MainWindow::exportTopmostLayerAudioAsync(const std::filesystem::path& path,
                                               sound_mind::codec::CompressedAudioFormat format) {
    if (isExportRunning()) {
        statusBar()->showMessage(tr("Already exporting - wait for it to finish, or cancel it, first."), 5000);
        return;
    }

    sound_mind::core::Layer* layer = layerController_->topmostLayerWithContent();
    const auto audio = layer != nullptr ? sound_mind::core::decodeLayerForExport(*layer) : std::nullopt;
    if (!audio.has_value()) {
        QMessageBox::critical(this, tr("Export Audio Failed"), tr("No layer with content to export."));
        return;
    }

    exportKind_ = ExportKind::Audio;
    exportPath_ = path;
    exportOutcome_ = ExportOutcome::Success;
    exportErrorMessage_.clear();

    // audio captured by value - see exportTopmostLayerVideoAsync()'s own
    // docs for why this is what makes the background thread safe against
    // concurrent project edits.
    exportTask_ = std::make_unique<sound_mind::core::BackgroundTask>(
        [this, path, format, audio = *audio](sound_mind::core::CancellationToken& token) {
            try {
                sound_mind::codec::exportCompressedAudio(path, audio, format,
                                                          [&token]() { return token.cancellationRequested(); });
            } catch (const sound_mind::codec::ExportCancelled&) {
                exportOutcome_ = ExportOutcome::Cancelled;
            } catch (const std::exception& e) {
                exportOutcome_ = ExportOutcome::Failed;
                exportErrorMessage_ = QString::fromStdString(e.what());
            }
        });
    exportTask_->start();

    statusBar()->showMessage(tr("Exporting audio..."));
    exportCancelButton_->show();
    exportProgressTimer_->start();
}

bool MainWindow::isExportRunning() const noexcept { return exportTask_ != nullptr && exportTask_->isRunning(); }

void MainWindow::cancelExport() {
    if (exportTask_) {
        exportTask_->requestCancel();
    }
}

void MainWindow::pollExportProgress() {
    if (exportTask_->isRunning()) {
        return;
    }
    exportProgressTimer_->stop();
    exportCancelButton_->hide();

    const QString kindLabel = exportKind_ == ExportKind::Video ? tr("video") : tr("audio");
    switch (exportOutcome_) {
        case ExportOutcome::Success:
            statusBar()->showMessage(
                tr("Exported %1 to \"%2\".").arg(kindLabel, QString::fromStdString(exportPath_.string())), 5000);
            break;
        case ExportOutcome::Cancelled:
            // Rolls back the partial file ExportCancelled itself
            // deliberately leaves behind - see exportVideo()'s/
            // exportCompressedAudio()'s own docs - matching finding #12's
            // confirmed "cancel rolls back partial work" semantics rather
            // than leaving a broken, unplayable file at the path the user
            // chose.
            std::filesystem::remove(exportPath_);
            statusBar()->showMessage(tr("%1 export cancelled.").arg(exportKind_ == ExportKind::Video
                                                                          ? tr("Video")
                                                                          : tr("Audio")),
                                      5000);
            break;
        case ExportOutcome::Failed:
            statusBar()->clearMessage();
            QMessageBox::critical(
                this, exportKind_ == ExportKind::Video ? tr("Export Video Failed") : tr("Export Audio Failed"),
                exportErrorMessage_);
            break;
    }
    exportTask_.reset();
}

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
    // loopEngine_ is already guaranteed to exist for any open project
    // (constructed fresh in setProject()), and its emptyImage() already
    // knows this project's real dimensions/config - LayerController
    // itself knows nothing about LoopEngine (see its own docs), so this
    // is resolved here and passed in.
    layerController_->addEmptyLayer(loopEngine_->emptyImage());
}

void MainWindow::addFilterLayer() { layerController_->addFilterLayer(); }

void MainWindow::handleLayerSelectionChanged(std::optional<sound_mind::core::LayerId> id) {
    layerController_->handleLayerSelectionChanged(id);
    // The active layer (paintTargetLayerId()) may have just changed - see
    // updateMindGrainGuardrails()'s own docs.
    updateMindGrainGuardrails();
}

void MainWindow::applyFilterConfiguration(const sound_mind::core::FilterConfiguration& config) {
    layerController_->applyFilterConfiguration(config);
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
    bool ok = false;
    const QString newName = QInputDialog::getText(this, tr("Rename MindWave"), tr("Name:"), QLineEdit::Normal,
                                                    QString::fromStdString(entry->name), &ok);
    if (!ok) {
        return;
    }
    mindWaveController_->renameMindWaveTo(id, newName);
}

void MainWindow::updateMindWave(sound_mind::core::MindWaveId id, const sound_mind::core::MindWave& wave) {
    mindWaveController_->updateMindWave(id, wave);
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
        toolPaletteController_->cancelPathPlacement();
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

void MainWindow::setExclusiveToolMode(QAction* activated, bool enabled, CanvasWidget::ToolMode mode) {
    canvas_->setToolMode(enabled ? mode : CanvasWidget::ToolMode::None);

    // Blocked so this doesn't recurse back into setPaintModeEnabled()/
    // setPickModeEnabled()/setSelectModeEnabled()/setPathModeEnabled() -
    // see this method's own docs for why exclusivity is handled by hand
    // here rather than via a QActionGroup.
    const QSignalBlocker paintBlocker(paintAction_);
    const QSignalBlocker pickBlocker(pickAction_);
    const QSignalBlocker selectBlocker(selectAction_);
    const QSignalBlocker pathBlocker(pathAction_);
    const QSignalBlocker chordBlocker(chordAction_);
    activated->setChecked(enabled);
    if (enabled) {
        if (activated != paintAction_) {
            paintAction_->setChecked(false);
        }
        if (activated != pickAction_) {
            pickAction_->setChecked(false);
        }
        if (activated != selectAction_) {
            selectAction_->setChecked(false);
        }
        if (activated != pathAction_) {
            pathAction_->setChecked(false);
        }
        if (activated != chordAction_) {
            chordAction_->setChecked(false);
        }
    }
}

void MainWindow::updateMindGrainGuardrails() {
    const auto activeLayer = layerController_->paintTargetLayerId();
    if (activeLayer.has_value()) {
        toolConfigurationPanel_->setActiveLayer(*activeLayer);
    }

    const auto* mindGrain =
        dynamic_cast<const sound_mind::core::MindGrainConfiguration*>(&toolConfigurationPanel_->toolConfiguration());

    std::vector<sound_mind::core::LayerId> disallowed;
    bool activeLayerDisallowed = false;
    if (mindGrain != nullptr && project_.has_value()) {
        const sound_mind::core::LayerId sourceLayer = mindGrain->sourceLayerId();
        for (const sound_mind::core::Layer& layer : project_->layers()) {
            if (!sound_mind::core::isLayerAbove(*project_, layer.id(), sourceLayer)) {
                disallowed.push_back(layer.id());
            }
        }
        activeLayerDisallowed =
            !activeLayer.has_value() || !sound_mind::core::isLayerAbove(*project_, *activeLayer, sourceLayer);
    }
    layersPanel_->setDisallowedLayers(disallowed);

    if (activeLayerDisallowed) {
        if (paintAction_->isChecked()) {
            setPaintModeEnabled(false);
        }
        paintAction_->setEnabled(false);
        paintAction_->setToolTip(
            tr("The configured Mind Grain can't paint onto the active layer - it must stay above its own source "
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
    const bool inputBusy = (loopEngine_ && loopEngine_->isRunning()) || recordEngine_.isRecording();
    const bool outputBusy = loopEngine_ && loopEngine_->isRunning();
    configureDevicesPanel_->setInputDeviceSelectionEnabled(!inputBusy);
    configureDevicesPanel_->setOutputDeviceSelectionEnabled(!outputBusy);
}

void MainWindow::undo() { undoStack_.undo(); }

void MainWindow::redo() { undoStack_.redo(); }

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

void MainWindow::deselect() { toolPaletteController_->clearSelection(); }

void MainWindow::finishPath() { toolPaletteController_->finishPath(); }

void MainWindow::cancelPath() { toolPaletteController_->cancelPath(); }

void MainWindow::setPathPlacesSmoothNodes(bool smooth) { toolPaletteController_->setPathPlacesSmoothNodes(smooth); }

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

void MainWindow::copySelection() { toolPaletteController_->copySelection(); }

void MainWindow::cutSelection() { toolPaletteController_->cutSelection(); }

void MainWindow::paste() {
    // The paste target is resolved fresh, right now - independent of
    // whichever layer the clipboard was originally copied from - per
    // SelectionController::pasteInto()'s own docs.
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
    if (!project_.has_value()) {
        return;
    }
    const std::string name = "Mind Shot " + std::to_string(project_->mindShots().size() + 1);
    if (const auto id = toolPaletteController_->captureMindShot(name); id.has_value()) {
        statusBar()->showMessage(tr("Captured as \"%1\".").arg(QString::fromStdString(name)), 5000);
    }
}

void MainWindow::captureMindGrain() {
    if (!project_.has_value()) {
        return;
    }
    const std::string name = "Mind Grain " + std::to_string(project_->mindGrains().size() + 1);
    if (const auto id = toolPaletteController_->captureMindGrain(name); id.has_value()) {
        statusBar()->showMessage(tr("Captured as \"%1\".").arg(QString::fromStdString(name)), 5000);
    }
}

void MainWindow::saveConvolutionKernel(int size, std::vector<float> coefficients, bool normalize) {
    if (!project_.has_value()) {
        return;
    }
    const std::string name = "Kernel " + std::to_string(project_->convolutionKernels().size() + 1);
    project_->addConvolutionKernel(name, size, std::move(coefficients), normalize);
    refreshConvolutionKernelCombo();
    hasUnsavedChanges_ = true;
    statusBar()->showMessage(tr("Saved as \"%1\".").arg(QString::fromStdString(name)), 5000);
}

void MainWindow::refreshConvolutionKernelCombo() {
    filterConfigurationPanel_->setAvailableConvolutionKernels(
        project_.has_value() ? project_->convolutionKernels() : std::vector<sound_mind::core::NamedConvolutionKernel>{});
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

    if (!playbackController_->isLoaded()) {
        if (isCompositingForPlayback()) {
            statusBar()->showMessage(
                tr("Already preparing playback - wait for it to finish, or cancel it, first."), 5000);
            return;
        }

        // Cheap synchronous pre-check, mirroring compositeProject()'s own
        // pre-pass (see its docs): is there anything to composite at all?
        // Kept synchronous (not backgrounded) so the no-content case stays
        // an instant no-op, exactly as before - only a real composite
        // attempt is worth backgrounding.
        const auto& layers = project_->layers();
        const bool anyContributor =
            std::any_of(layers.begin(), layers.end(), [](const sound_mind::core::Layer& layer) {
                return layer.visible() && !layer.muted() && layer.content().has_value() &&
                       !sound_mind::core::isFilterLayerType(layer.type());
            });
        if (!anyContributor) {
            return;
        }

        compositeOutcome_ = CompositeOutcome::Success;
        compositeErrorMessage_.clear();
        compositedResult_.reset();

        // project_ copied by value - compositeProject() never touches
        // project_ itself once backgrounded, so the background thread has
        // nothing shared to synchronize against - see startPlayback()'s
        // own docs.
        compositeTask_ = std::make_unique<sound_mind::core::BackgroundTask>(
            [this, projectCopy = *project_](sound_mind::core::CancellationToken& token) {
                try {
                    compositedResult_ = sound_mind::core::compositeProject(
                        projectCopy, [&token]() { return token.cancellationRequested(); }, /*respectMute=*/true);
                } catch (const sound_mind::core::CompositeCancelled&) {
                    compositeOutcome_ = CompositeOutcome::Cancelled;
                } catch (const std::exception& e) {
                    compositeOutcome_ = CompositeOutcome::Failed;
                    compositeErrorMessage_ = QString::fromStdString(e.what());
                }
            });
        compositeTask_->start();

        statusBar()->showMessage(tr("Preparing playback..."));
        compositeCancelButton_->show();
        compositeProgressTimer_->start();
        return;
    }

    playbackController_->play();
}

void MainWindow::pausePlayback() {
    // Position bar/playhead stay where they are - only stopPlayback()
    // resets them, matching "startPlayback() resumes from the same
    // position" - no point polling a position that isn't moving.
    playbackController_->pause();
}

void MainWindow::stopPlayback() {
    playbackController_->stop();
    playbackPanel_->setDuration(0.0);
    canvas_->setPlayheadFraction(std::nullopt);
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
    repeatRangeStartSeconds_ = 0.0;
    repeatRangeEndSeconds_ = playbackController_->totalSeconds();
    repeatLoopBackSeconds_ = 0.0;
}

void MainWindow::setPlaybackOutputDevice(const QString& deviceName) {
    // Unlike Loop/Record's device preferences (only applied on their next
    // start()), PlaybackEngine's device is open for its whole lifetime, so
    // this switches immediately - see PlaybackEngine::setPreferredOutputDevice()'s
    // own docs.
    if (!playbackController_->setOutputDevice(deviceName)) {
        statusBar()->showMessage(tr("Could not switch to the selected output device."), 5000);
    }
}

void MainWindow::setPlaybackVolume(int percent) {
    playbackController_->setVolume(percent);
}

void MainWindow::setHardwareAccelerationEnabled(bool enabled) {
    settings_.setValue(QStringLiteral("hardwareAccelerationEnabled"), enabled);
    sound_mind::core::setHardwareAccelerationEnabled(enabled);
    canvas_->update();
}

void MainWindow::setPlaybackRepeat(bool enabled) {
    repeatEnabled_ = enabled;
    if (!enabled && playbackController_->isLoaded()) {
        // Widen back to the whole track immediately - see this method's
        // own docs on why nothing about a stale Delta/Review range should
        // linger once Repeat is unchecked.
        repeatRangeStartSeconds_ = 0.0;
        repeatRangeEndSeconds_ = playbackController_->totalSeconds();
        repeatLoopBackSeconds_ = 0.0;
    }
}

void MainWindow::setPlaybackScope(sound_mind::studio::PlaybackScope scope) {
    playbackScope_ = scope;
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
        updateConfiguredDeviceLockState();
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
    layerController_->refreshLayersPanel();
    canvas_->update();

    loopEngine_->start();
    loopPanel_->setRunning(true);
    updateConfiguredDeviceLockState();
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

void MainWindow::updateLoopLayer() {
    if (!loopLayerId_ || !loopEngine_) {
        return;
    }
    sound_mind::core::Layer* layer = layerController_->layerById(*loopLayerId_);
    if (layer == nullptr) {
        return;
    }

    // Loop Mode Live Preview (v0.0.41.1) - prefer the current loop's own
    // still-growing preview whenever it has anything to show, falling back
    // to the last *completed* loop's image otherwise (right after a loop
    // boundary, before the new loop has captured enough for even one
    // preview frame) - see LoopEngine::currentPreviewImage()'s own docs for
    // why this combination is what actually produces continuous growth
    // rather than a once-per-loop jump.
    sound_mind::codec::StreamImage image = loopEngine_->currentPreviewImage();
    if (image.frameCount == 0) {
        image = loopEngine_->currentImage();
    }
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
    return playbackController_->isPlaying();
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

float MainWindow::loopInputGain() const noexcept {
    return loopEngine_ ? loopEngine_->inputGain() : 1.0f;
}

float MainWindow::playbackVolume() const noexcept {
    return playbackController_->volume();
}

void MainWindow::toggleRecording() {
    if (recordEngine_.isRecording()) {
        recordDrainTimer_->stop();
        recordEngine_.stop();
        recordPanel_->setRecording(false);
        updateConfiguredDeviceLockState();

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
            playbackController_->invalidate();
            hasUnsavedChanges_ = true;
            layerController_->refreshLayersPanel();
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
    updateConfiguredDeviceLockState();
    if (!recordEngine_.isDeviceAvailable()) {
        statusBar()->showMessage(tr("Recording started, but no input device is available - nothing will be captured."),
                                  5000);
    } else {
        statusBar()->showMessage(tr("Recording..."));
    }
    recordDrainTimer_->start();
}

void MainWindow::refreshConfiguredDevices() {
    const QStringList inputDevices = toQStringList(recordEngine_.availableInputDeviceNames());
    const QStringList outputDevices = toQStringList(playbackController_->availableOutputDeviceNames());
    configureDevicesPanel_->setInputDevices(inputDevices);
    configureDevicesPanel_->setOutputDevices(outputDevices);
}

void MainWindow::setConfiguredInputDevice(const QString& deviceName) {
    configuredInputDeviceName_ = deviceName;
    recordEngine_.setPreferredInputDevice(deviceName.toStdString());
    if (loopEngine_) {
        loopEngine_->setPreferredInputDevice(deviceName.toStdString());
    }
}

void MainWindow::setConfiguredOutputDevice(const QString& deviceName) {
    configuredOutputDeviceName_ = deviceName;
    setPlaybackOutputDevice(deviceName);
    if (loopEngine_) {
        loopEngine_->setPreferredOutputDevice(deviceName.toStdString());
    }
}

void MainWindow::setConfiguredInputGain(int percent) {
    const float gain = static_cast<float>(percent) / 100.0f;
    configuredInputGain_ = gain;
    recordEngine_.setInputGain(gain);
    // Kept consistent with the real configured gain, so "test" actually
    // reflects what real recording would sound/level like.
    deviceTestRecordEngine_.setInputGain(gain);
    if (loopEngine_) {
        loopEngine_->setInputGain(gain);
    }
}

void MainWindow::setConfiguredOutputGain(int percent) { setPlaybackVolume(percent); }

void MainWindow::toggleTestInputDevice(bool testing) {
    if (testing) {
        deviceTestRecordEngine_.setPreferredInputDevice(configuredInputDeviceName_.toStdString());
        deviceTestRecordEngine_.start();
        testInputLevelTimer_->start();
    } else {
        testInputLevelTimer_->stop();
        deviceTestRecordEngine_.stop();
        configureDevicesPanel_->setInputLevel(0.0f);
    }
}

void MainWindow::toggleTestOutputDevice(bool testing) {
    if (testing) {
        deviceTestTonePlayer_.start(configuredOutputDeviceName_.toStdString());
    } else {
        deviceTestTonePlayer_.stop();
    }
}

void MainWindow::drainRecording() {
    recordEngine_.drainAvailable();
}

void MainWindow::pollTestInputLevel() {
    // Keeps deviceTestRecordEngine_'s own ring buffer from overflowing -
    // the accumulated capturedAudio() itself is never used for testing,
    // only currentInputLevel() below, but letting the ring fill up would
    // start dropping samples mid-block, same reasoning as drainRecording().
    deviceTestRecordEngine_.drainAvailable();
    configureDevicesPanel_->setInputLevel(deviceTestRecordEngine_.currentInputLevel());
}

void MainWindow::handleContentChangedForPlayback(sound_mind::core::LayerId layer) {
    if (!project_) {
        return;
    }
    // Repeat ON only reacts to an edit while a session is already playing -
    // "works in principle just like Loop Mode", which never auto-starts
    // itself either. Repeat OFF, on the other hand, is a fresh one-shot
    // preview per docs/sound-mind-design.md's "Repeat Playback" section:
    // Delta/Review's own "restarts audio playback... the moment the canvas
    // is updated" is driven by Scope alone, independent of Repeat and of
    // whether anything was already playing - Track has no such behavior at
    // all (it isn't "the edit" in the Track case, there's no narrower
    // region to preview).
    const bool deltaOrReview = playbackScope_ == sound_mind::studio::PlaybackScope::Delta ||
                                playbackScope_ == sound_mind::studio::PlaybackScope::Review;
    if (repeatEnabled_) {
        if (!playbackController_->isPlaying()) {
            return;
        }
    } else if (!deltaOrReview) {
        return;
    }

    const auto composite = sound_mind::core::compositeProject(*project_, /*shouldCancel=*/nullptr, /*respectMute=*/true);
    if (!composite.has_value()) {
        return;
    }

    // The edited region, for Delta/Review - layer's own most recently
    // active operation is a practical approximation of "what just
    // changed": exact for a fresh paint/fill/paste/sequence stamp
    // (that operation is always the newest active one on its own layer),
    // and still reasonable for an undo/redo (neither appends a fresh
    // operation of its own - the layer's own now-different "most recently
    // active" operation is the closest available stand-in for what the
    // user just saw change).
    std::optional<sound_mind::core::TimeFrequencyRect> editedBounds;
    const auto operations = project_->operationLog().activeOperationsTargeting(layer);
    if (!operations.empty()) {
        editedBounds = operations.back()->bounds();
    }

    // Stops playback first (see load()'s own docs) - the confirmed
    // "audible restart on edit" behavior, not seamless live double-
    // buffering.
    playbackController_->load(sound_mind::codec::decode(*composite));
    const double totalSeconds = playbackController_->totalSeconds();

    switch (playbackScope_) {
        case sound_mind::studio::PlaybackScope::Track:
            repeatRangeStartSeconds_ = 0.0;
            repeatRangeEndSeconds_ = totalSeconds;
            repeatLoopBackSeconds_ = repeatRangeStartSeconds_;
            playbackController_->seek(currentPlaybackPositionSeconds_);
            break;
        case sound_mind::studio::PlaybackScope::Delta:
            repeatRangeStartSeconds_ = editedBounds ? editedBounds->startTimeSeconds : 0.0;
            repeatRangeEndSeconds_ = editedBounds ? editedBounds->endTimeSeconds : totalSeconds;
            repeatLoopBackSeconds_ = repeatRangeStartSeconds_;
            playbackController_->seek(repeatRangeStartSeconds_);
            break;
        case sound_mind::studio::PlaybackScope::Review:
            repeatRangeStartSeconds_ = editedBounds ? editedBounds->startTimeSeconds : 0.0;
            repeatRangeEndSeconds_ = totalSeconds;
            // Unlike Delta, Review loops the whole track once it reaches
            // the end - back to the track's own start, not the edit's.
            repeatLoopBackSeconds_ = 0.0;
            playbackController_->seek(repeatRangeStartSeconds_);
            break;
    }

    playbackController_->play();
}

void MainWindow::checkRepeatPlaybackRange(double positionSeconds) {
    // `<= repeatRangeStartSeconds_`, not `<= 0.0`: a single-click (as
    // opposed to dragged) paint stroke's own bounds() is a genuine
    // zero-width point - Delta/Review scope then sets
    // repeatRangeEndSeconds_ == repeatRangeStartSeconds_ exactly (see
    // handleContentChangedForPlayback() above). The seek() below emits
    // positionChanged() synchronously (see PlaybackController::seek()'s
    // own docs), re-entering this same function - with the old `<= 0.0`
    // guard, a zero-width range still looked "active" (end > 0), so the
    // very seek() that establishes it already satisfies "past the end"
    // (positionSeconds == repeatRangeEndSeconds_), triggering another
    // seek(), another positionChanged(), and so on - unbounded recursion
    // until the stack overflows. A range with nothing to actually loop
    // over (end <= start) is treated the same as "no range yet": play
    // straight through with no halt/loop-back at all for that one edit.
    if (repeatRangeEndSeconds_ <= repeatRangeStartSeconds_) {
        return;
    }
    if (positionSeconds < repeatRangeEndSeconds_) {
        return;
    }
    // Repeat ON loops back (see repeatLoopBackSeconds_'s own docs for the
    // per-scope target); Repeat OFF halts right here instead - per
    // docs/sound-mind-design.md's "Repeat Playback" section, Delta "halts
    // or repeats depending on the repeat checkbox" once it reaches its own
    // range's end, regardless of which one happens. `pause()`, not
    // `stop()`: the playhead stays exactly where it halted rather than
    // resetting to the start - see stopPlayback()'s own docs on why that
    // one's different (an explicit Stop is a deliberate reset).
    if (repeatEnabled_) {
        playbackController_->seek(repeatLoopBackSeconds_);
        playbackController_->play();
    } else {
        playbackController_->pause();
    }
}

void MainWindow::poolTopmostLayer() { poolTopmostLayerAsync(); }

bool MainWindow::poolTopmostLayerNow(QString* errorMessage, QString* streamPngPath, QString* poolPngPath) {
    sound_mind::core::Layer* layer = layerController_->topmostLayerWithContent();
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
        playbackController_->invalidate();
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

void MainWindow::poolTopmostLayerAsync() {
    if (isPoolRunning()) {
        statusBar()->showMessage(tr("Already pooling - wait for it to finish, or cancel it, first."), 5000);
        return;
    }

    sound_mind::core::Layer* layer = layerController_->topmostLayerWithContent();
    if (layer == nullptr || !layer->content().has_value()) {
        QMessageBox::critical(this, tr("Pool Layer Failed"), tr("No layer with content to pool."));
        return;
    }

    poolLayerId_ = layer->id();
    poolLayerName_ = QString::fromStdString(layer->name());
    poolOutcome_ = PoolOutcome::Success;
    poolErrorMessage_.clear();
    pooledContent_.reset();

    // content copied by value - computePooledContent() never touches
    // project_/layer itself, so the background thread has nothing shared
    // to synchronize against - see this method's own docs.
    const auto content = *layer->content();
    poolTask_ = std::make_unique<sound_mind::core::BackgroundTask>(
        [this, content](sound_mind::core::CancellationToken& token) {
            try {
                pooledContent_ = sound_mind::core::computePooledContent(
                    content, [&token]() { return token.cancellationRequested(); });
            } catch (const sound_mind::core::PoolCancelled&) {
                poolOutcome_ = PoolOutcome::Cancelled;
            } catch (const std::exception& e) {
                poolOutcome_ = PoolOutcome::Failed;
                poolErrorMessage_ = QString::fromStdString(e.what());
            }
        });
    poolTask_->start();

    statusBar()->showMessage(tr("Pooling layer..."));
    poolCancelButton_->show();
    poolProgressTimer_->start();
}

bool MainWindow::isPoolRunning() const noexcept { return poolTask_ != nullptr && poolTask_->isRunning(); }

void MainWindow::cancelPool() {
    if (poolTask_) {
        poolTask_->requestCancel();
    }
}

void MainWindow::pollPoolProgress() {
    if (poolTask_->isRunning()) {
        return;
    }
    poolProgressTimer_->stop();
    poolCancelButton_->hide();

    switch (poolOutcome_) {
        case PoolOutcome::Success: {
            sound_mind::core::Layer* layer =
                poolLayerId_.has_value() ? layerController_->layerById(*poolLayerId_) : nullptr;
            if (layer != nullptr && pooledContent_.has_value()) {
                layer->setPoolContent(std::move(pooledContent_->poolImage));
                layer->setContent(std::move(pooledContent_->streamContent));
                canvas_->update();
                // The layer's content was just replaced with a fresh,
                // pool-derived Stream copy - the next startPlayback()
                // should pick that up rather than continue playing
                // whatever was loaded before.
                playbackController_->invalidate();
                hasUnsavedChanges_ = true;
                statusBar()->showMessage(tr("Pooled layer \"%1\".").arg(poolLayerName_), 5000);
            } else {
                // The layer was deleted (or the project switched - though
                // newProject()/openProject()/openProjectAt()/closeEvent()
                // all refuse outright while isPoolRunning(), see this
                // class's own docs) while pooling ran.
                statusBar()->showMessage(tr("Pooling finished, but the layer no longer exists."), 5000);
            }
            break;
        }
        case PoolOutcome::Cancelled:
            // Nothing to roll back beyond discarding pooledContent_ - the
            // layer's own real content was never touched, unlike Export's
            // "delete the partial file" rollback - matching finding #12's
            // confirmed "cancel rolls back partial work" semantics with the
            // simplest rollback a pool can have: not applying anything.
            statusBar()->showMessage(tr("Pooling cancelled."), 5000);
            break;
        case PoolOutcome::Failed:
            statusBar()->clearMessage();
            QMessageBox::critical(this, tr("Pool Layer Failed"), poolErrorMessage_);
            break;
    }
    pooledContent_.reset();
    poolLayerId_.reset();
    poolTask_.reset();
}

bool MainWindow::isCompositingForPlayback() const noexcept {
    return compositeTask_ != nullptr && compositeTask_->isRunning();
}

void MainWindow::cancelPlaybackComposite() {
    if (compositeTask_) {
        compositeTask_->requestCancel();
    }
}

void MainWindow::pollCompositeProgress() {
    if (compositeTask_->isRunning()) {
        return;
    }
    compositeProgressTimer_->stop();
    compositeCancelButton_->hide();

    switch (compositeOutcome_) {
        case CompositeOutcome::Success:
            if (compositedResult_.has_value()) {
                // load() itself emits durationChanged() (connected in the
                // constructor to playbackPanel_->setDuration()) - only
                // needs doing once per load, matching startPlayback()'s own
                // prior synchronous contract.
                playbackController_->load(sound_mind::codec::decode(*compositedResult_));
                // A fresh load starts a new "nothing edited yet this
                // session" range - the whole track, regardless of
                // playbackScope_ - narrowed only once a real edit actually
                // arrives (handleContentChangedForPlayback()).
                repeatRangeStartSeconds_ = 0.0;
                repeatRangeEndSeconds_ = playbackController_->totalSeconds();
                repeatLoopBackSeconds_ = 0.0;
                playbackController_->play();
                statusBar()->clearMessage();
            } else {
                // Shouldn't happen given startPlayback()'s own pre-check
                // already confirmed a contributor exists - kept as a safe
                // fallback rather than an assert, the same caution
                // pollPoolProgress()'s own "layer no longer exists" branch
                // takes.
                statusBar()->showMessage(tr("Nothing to play."), 5000);
            }
            break;
        case CompositeOutcome::Cancelled:
            // Nothing to roll back beyond discarding compositedResult_ -
            // playbackController_ was never touched, matching Pool's own
            // simplest-possible rollback.
            statusBar()->showMessage(tr("Playback preparation cancelled."), 5000);
            break;
        case CompositeOutcome::Failed:
            statusBar()->clearMessage();
            QMessageBox::critical(this, tr("Playback Failed"), compositeErrorMessage_);
            break;
    }
    compositedResult_.reset();
    compositeTask_.reset();
}

}  // namespace sound_mind::studio
