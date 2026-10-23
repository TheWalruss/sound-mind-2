// MainWindow's menus, the menu bar's top-right dropdowns and its toolbars
// (v0.1.48.9, moved out of the constructor in main_window.cpp unchanged).
// Each function is called once, in this order, from the constructor - see
// the declarations' docs in main_window.h.
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

void MainWindow::buildCornerMenus() {
    // The top-right corner of the menu bar hosts two dropdowns side by
    // side - Input/Output and Macro - via a shared container widget, since
    // QMenuBar::setCornerWidget() only accepts one widget per corner.
    // QMenuBar::setCornerWidget() is the roadmap's own documented "default
    // location... up at the top-right of the menu bar, rather than in the
    // sidebar" mechanism, not used anywhere else in this codebase; Qt
    // reparents whatever's passed to it to the menu bar itself when
    // accepted, so the outer container (not the two buttons individually)
    // is what needs menuBar() as its own parent. Note for future
    // cross-platform testing: a native macOS menu bar doesn't render
    // corner widgets at all - Qt degrades gracefully (the controls simply
    // don't appear there), not a crash, but worth knowing if this is ever
    // exercised on that platform.
    auto* topRightContainer = new QWidget(menuBar());
    auto* topRightLayout = new QHBoxLayout(topRightContainer);
    topRightLayout->setContentsMargins(0, 0, 0, 0);
    topRightLayout->setSpacing(4);

    // The "Input/Output" dropdown - reuses each panel's own
    // toggleViewAction() directly as its three entries (rather than
    // hand-rolled QActions, unlike the Tool dropdown's own
    // panAction_/paintAction_/etc.) precisely because a dock's visibility
    // *is* already a QWidget-backed, self-syncing piece of state - there's
    // no separate "mode" concept like CanvasWidget::ToolMode to manage
    // alongside it.
    auto* ioMenu = new QMenu(topRightContainer);
    ioMenu->addAction(playbackPanel_->toggleViewAction());
    ioMenu->addAction(recordPanel_->toggleViewAction());
    ioMenu->addAction(loopPanel_->toggleViewAction());

    auto* ioButton = new QToolButton(topRightContainer);
    ioButton->setObjectName(QStringLiteral("inputOutputButton"));
    ioButton->setText(tr("Input/Output"));
    ioButton->setPopupMode(QToolButton::InstantPopup);
    ioButton->setMenu(ioMenu);
    ioButton->setToolTip(tr("Show Playback, Record, or Loop - only one at a time"));
    topRightLayout->addWidget(ioButton);

    // The "Macro" dropdown (real-world testing pass, 2026-09-29, confirmed
    // with the user) - collects Record/Play/Export Video Macro, previously
    // three separate transportToolBar actions, next to Input/Output at the
    // top-right rather than under the "Configure" dropdown with the rest of
    // the panel toggles: these three are one-shot/toggle *actions*, not
    // dock panels, and (unlike Configure's own seven entries) there's
    // nothing to leave "open" - grouping them with Input/Output's own
    // action-oriented, top-right placement fits better than Configure's
    // panel-toggle one.
    //
    // The Macro dropdown (v0.1.44.1, redesigned - architecture Decision
    // #262): record a performance, manage the recorded ones, play one back
    // or export it as a video.
    macroRecordAction_ = new QAction(tr("Record &Macro"), this);
    macroRecordAction_->setCheckable(true);
    macroRecordAction_->setToolTip(
        tr("Records a performance: start Playback or Loop, then change the project while it plays. Everything "
           "you do is recorded on a clock that only runs while playback does."));
    connect(macroRecordAction_, &QAction::toggled, this, &MainWindow::setMacroRecordingEnabled);

    macroPlayAction_ = new QAction(tr("&Play Macro"), this);
    macroPlayAction_->setToolTip(tr("Plays back a recorded performance, without changing the project"));
    connect(macroPlayAction_, &QAction::triggered, this, &MainWindow::playMacro);

    macroExportVideoAction_ = new QAction(tr("E&xport Macro Video..."), this);
    macroExportVideoAction_->setToolTip(tr("Renders a recorded performance as a video"));
    connect(macroExportVideoAction_, &QAction::triggered, this, &MainWindow::exportMacroVideo);

    macroManageAction_ = new QAction(tr("Manage &Macros..."), this);
    macroManageAction_->setToolTip(tr("Rename or delete recorded performances"));
    connect(macroManageAction_, &QAction::triggered, this, &MainWindow::manageMacros);

    auto* macroMenu = new QMenu(topRightContainer);
    macroMenu->addAction(macroRecordAction_);
    macroMenu->addAction(macroPlayAction_);
    macroMenu->addAction(macroExportVideoAction_);
    macroMenu->addAction(macroManageAction_);

    auto* macroButton = new QToolButton(topRightContainer);
    macroButton->setObjectName(QStringLiteral("macroButton"));
    macroButton->setText(tr("Macro"));
    macroButton->setPopupMode(QToolButton::InstantPopup);
    macroButton->setMenu(macroMenu);
    macroButton->setToolTip(tr("Record, play back, or export a macro of this session's own actions"));
    topRightLayout->addWidget(macroButton);

    menuBar()->setCornerWidget(topRightContainer, Qt::TopRightCorner);
}

void MainWindow::buildFileMenu() {
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

    QAction* remasterAction = fileMenu->addAction(tr("&Remaster..."));
    remasterAction->setObjectName(QStringLiteral("remasterAction"));
    connect(remasterAction, &QAction::triggered, this, &MainWindow::remaster);

    fileMenu->addSeparator();

    QAction* importAudioAction = fileMenu->addAction(tr("Import &Audio..."));
    connect(importAudioAction, &QAction::triggered, this, &MainWindow::importAudio);

    QAction* importImageAction = fileMenu->addAction(tr("Import &Image..."));
    connect(importImageAction, &QAction::triggered, this, &MainWindow::importImage);

    QAction* importMidiAction = fileMenu->addAction(tr("Import &MIDI..."));
    connect(importMidiAction, &QAction::triggered, this, &MainWindow::importMidi);

    fileMenu->addSeparator();

    QAction* exportAudioAction = fileMenu->addAction(tr("&Export Audio..."));
    connect(exportAudioAction, &QAction::triggered, this, &MainWindow::exportAudio);

    QAction* exportVideoAction = fileMenu->addAction(tr("Export &Video..."));
    connect(exportVideoAction, &QAction::triggered, this, &MainWindow::exportVideo);
    // v0.1.52.1 - a copy of the canvas as an image file.
    QAction* exportImageAction = fileMenu->addAction(tr("Export I&mage..."));
    exportImageAction->setObjectName(QStringLiteral("exportImageAction"));
    connect(exportImageAction, &QAction::triggered, this, &MainWindow::exportImage);
}

void MainWindow::buildEditMenu() {
    // Basic Painting (v0.Y.24.1): Undo/Redo apply to paint strokes only so
    // far - the same scope PaintController's own undo()/redo() already
    // has (see its docs) - not a project-wide undo covering every kind of
    // change yet.
    // Edit menu redesign (docs/sound-mind-roadmap.md's own long-flagged
    // "Edit menu redesign" backlog item, v0.Y.65.1's own backlog list) -
    // direct user feedback: the previous flat, 29-action/6-separator
    // menu was "a kludge" and "intimidating". Each of the groups that
    // flat list's own separators (and doc comments) already implied
    // becomes one named submenu here instead, so the top-level Edit menu
    // now shows a short, scannable list of named categories rather than
    // every individual action at once. Undo/Redo/Delete and Image Mode
    // are the deliberate exceptions, kept directly on the top level
    // rather than each getting their own single/triple-item submenu -
    // confirmed-by-judgment, not literally instructed: Undo/Redo/Delete
    // are the most universally expected, most frequently reached-for
    // items in *any* application's Edit menu (burying them a click
    // deeper would make the menu worse, not less intimidating), and
    // Image Mode is a checkable mode toggle (its checked state is worth
    // seeing at a glance, not worth hiding behind a submenu for just one
    // item). Every action's own shortcut, connection, and no-op-when-
    // inapplicable behavior is unchanged - this only reorganizes the
    // menu structure around them. Graying out a group/action when its
    // own precondition isn't met (e.g. "Edit Path" with nothing Picked)
    // is deliberately a separate, later step - confirmed with the user.
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu_ = editMenu;
    // Pre-Beta Plan v0.Y.77.1: entries that don't apply right now are grayed
    // out while the menu is open - see updateEditActionStates().
    connect(editMenu, &QMenu::aboutToShow, this, &MainWindow::updateEditActionStates);
    connect(editMenu, &QMenu::aboutToHide, this, &MainWindow::restoreEditActions);
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
    QMenu* stackOrderMenu = editMenu->addMenu(tr("&Stack Order"));
    QAction* bringToFrontAction = stackOrderMenu->addAction(tr("Bring to &Front"));
    bringToFrontAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Up));
    connect(bringToFrontAction, &QAction::triggered, this, &MainWindow::bringPickedObjectToFront);

    QAction* sendToBackAction = stackOrderMenu->addAction(tr("Send to &Back"));
    sendToBackAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Down));
    connect(sendToBackAction, &QAction::triggered, this, &MainWindow::sendPickedObjectToBack);

    QAction* bringForwardAction = stackOrderMenu->addAction(tr("Bring &Forward"));
    bringForwardAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Up));
    connect(bringForwardAction, &QAction::triggered, this, &MainWindow::bringPickedObjectForward);

    QAction* sendBackwardAction = stackOrderMenu->addAction(tr("Send Back&ward"));
    sendBackwardAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Down));
    connect(sendBackwardAction, &QAction::triggered, this, &MainWindow::sendPickedObjectBackward);

    // Path node/handle editing (v0.0.26.4) - see PickController::
    // beginPathEdit()'s own docs. No standard shortcuts (matching Fill
    // Selection's own no-shortcut choice below); each is a no-op when
    // inapplicable, the same "always present" choice deleteAction makes.
    QMenu* pathEditingMenu = editMenu->addMenu(tr("Path &Editing"));
    QAction* editPathAction = pathEditingMenu->addAction(tr("&Edit Path"));
    connect(editPathAction, &QAction::triggered, this, &MainWindow::editPickedPath);

    QAction* toggleNodeTypeAction = pathEditingMenu->addAction(tr("Toggle &Node Type"));
    connect(toggleNodeTypeAction, &QAction::triggered, this, &MainWindow::togglePickedPathNodeType);

    QAction* applyPathEditAction = pathEditingMenu->addAction(tr("&Apply Path Edit"));
    connect(applyPathEditAction, &QAction::triggered, this, &MainWindow::applyPickedPathEdit);

    QAction* cancelPathEditAction = pathEditingMenu->addAction(tr("Cancel Pat&h Edit"));
    connect(cancelPathEditAction, &QAction::triggered, this, &MainWindow::cancelPickedPathEdit);

    // Selection & Fill (v0.Y.25.1): the standard "clear the current
    // selection" shortcut/name every other image/vector editor already
    // uses - a no-op, per deselect()'s own docs, when there isn't one.
    QMenu* selectionMenu = editMenu->addMenu(tr("&Selection"));
    QAction* deselectAction = selectionMenu->addAction(tr("D&eselect"));
    deselectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(deselectAction, &QAction::triggered, this, &MainWindow::deselect);

    // No standard shortcut for this one (unlike Delete/Deselect above).
    QAction* fillAction = selectionMenu->addAction(tr("&Fill Selection..."));
    connect(fillAction, &QAction::triggered, this, &MainWindow::fillSelection);

    // Apply Filter to Selection (v0.Y.46.1 Installment E, "Layers Panel &
    // Editing Enhancements v2") - no standard shortcut (matching Fill
    // Selection's own no-shortcut choice above); a no-op with no
    // committed selection, the same "always present" choice deleteAction
    // makes.
    QAction* applyFilterToSelectionAction = selectionMenu->addAction(tr("Apply &Filter to Selection"));
    connect(applyFilterToSelectionAction, &QAction::triggered, this, &MainWindow::applyFilterToSelection);

    editMenu->addSeparator();

    // Principal Modes (v0.Y.47.1) - docs/sound-mind-design.md's own
    // "Principal modes". An editing-behavior toggle, not a view/display
    // setting, hence Edit menu (not View, unlike Hardware Acceleration)
    // and persisted per-project (not app-wide QSettings) - see
    // setPrincipalMode()'s own docs. Initial state applied directly
    // (matching Hardware Acceleration's own QSignalBlocker pattern above)
    // once a project is actually set, in setProject() below - a
    // freshly-constructed MainWindow has no project yet to reflect. Kept
    // directly on the Edit menu (not nested in a submenu) - see this
    // whole block's own opening comment for why.
    principalModeAction_ = editMenu->addAction(tr("&Image Mode"));
    principalModeAction_->setCheckable(true);
    principalModeAction_->setToolTip(
        tr("When checked, brush strokes keep a fixed pixel footprint regardless of frequency register "
           "(Image-mode). When unchecked (default), strokes keep a fixed frequency span instead, appearing "
           "as an oval near the top of the frequency range and an egg shape near the bottom (Sound-mode)."));
    connect(principalModeAction_, &QAction::toggled, this, &MainWindow::setPrincipalMode);

    // Per project, like Image Mode - see setDecimalSeparator().
    QMenu* decimalSeparatorMenu = editMenu->addMenu(tr("Decimal &Separator"));
    auto* decimalSeparatorGroup = new QActionGroup(this);
    decimalPeriodAction_ = decimalSeparatorMenu->addAction(tr("Period (1.5)"));
    decimalPeriodAction_->setObjectName(QStringLiteral("decimalPeriodAction"));
    decimalCommaAction_ = decimalSeparatorMenu->addAction(tr("Comma (1,5)"));
    decimalCommaAction_->setObjectName(QStringLiteral("decimalCommaAction"));
    for (QAction* action : {decimalPeriodAction_, decimalCommaAction_}) {
        action->setCheckable(true);
        decimalSeparatorGroup->addAction(action);
    }
    decimalPeriodAction_->setChecked(true);
    connect(decimalPeriodAction_, &QAction::triggered, this,
            [this]() { setDecimalSeparator(sound_mind::core::DecimalSeparator::Period); });
    connect(decimalCommaAction_, &QAction::triggered, this,
            [this]() { setDecimalSeparator(sound_mind::core::DecimalSeparator::Comma); });

    editMenu->addSeparator();

    // Cut/Copy/Paste (v0.Y.25.2): the standard shortcuts every other
    // editor already uses. Cut/Copy are no-ops (per copySelection()'s/
    // cutSelection()'s own docs) with no committed selection; Paste is a
    // no-op with nothing on the clipboard - none of the three are enabled/
    // disabled in sync with that state, the same "always present, no-op
    // when inapplicable" choice deleteAction above already makes.
    QMenu* clipboardMenu = editMenu->addMenu(tr("&Clipboard"));
    QAction* cutAction = clipboardMenu->addAction(tr("Cu&t"));
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, this, &MainWindow::cutSelection);

    QAction* copyAction = clipboardMenu->addAction(tr("&Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, &MainWindow::copySelection);

    QAction* pasteAction = clipboardMenu->addAction(tr("&Paste"));
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, &MainWindow::paste);

    // Mind Shots (v0.Y.33.1 Installment A) - no standard shortcut (matching
    // Fill Selection's own no-shortcut choice above); a no-op with no
    // committed selection, the same "always present" choice deleteAction/
    // Cut/Copy/Paste all make.
    QMenu* captureMenu = editMenu->addMenu(tr("Ca&pture"));
    QAction* captureMindShotAction = captureMenu->addAction(tr("Capture as &Mind Shot"));
    connect(captureMindShotAction, &QAction::triggered, this, &MainWindow::captureMindShot);

    // Mind Grains (v0.Y.33.1 Installment B) - the same "no shortcut, no-op
    // with no committed selection" treatment as Mind Shot's own action
    // right above.
    QAction* captureMindGrainAction = captureMenu->addAction(tr("Capture as Mind &Grain"));
    connect(captureMindGrainAction, &QAction::triggered, this, &MainWindow::captureMindGrain);

    // MindWaves v2, Installment B (v0.Y.39.1) - no standard shortcut
    // (matching Fill Selection's own no-shortcut choice above); a no-op
    // with nothing suitable Picked, the same "always present" choice
    // deleteAction/Cut/Copy/Paste all make. Grouped with the Resonant
    // Instrument actions right below rather than with Capture above -
    // both take the same "a Picked path/curve" input and produce a new
    // library entry from it, unlike Capture's own "turn a Selection into
    // a Mind Shot/Grain" shape.
    QMenu* pickedPathToolsMenu = editMenu->addMenu(tr("Picked Path &Tools"));
    QAction* usePickedPathAsMindWaveShapeAction =
        pickedPathToolsMenu->addAction(tr("Use Picked Path as MindWave &Shape"));
    connect(usePickedPathAsMindWaveShapeAction, &QAction::triggered, this, &MainWindow::usePickedPathAsMindWaveShape);

    // Resonant Instruments (v0.Y.59.1 Installment C) - same "always
    // present, no-op with nothing suitable Picked" treatment as the
    // MindWave-shape action right above, since both share the same input
    // (a Picked path/curve), just producing a new named library entry
    // instead of overwriting part of an existing MindWave.
    QAction* createResonanceAction = pickedPathToolsMenu->addAction(tr("Create &Resonance from Picked Path..."));
    connect(createResonanceAction, &QAction::triggered, this, &MainWindow::createResonanceFromPickedPath);

    // Resonant Instruments - Branching Curve editor (v0.Y.59.1, the
    // deferred half of item 1) - same "always present, no-op with nothing
    // suitable Picked/no session active" treatment as every action above.
    // See startBranchingCurveFromPickedPath()'s own docs for the full
    // workflow these three fit into.
    QAction* startBranchingCurveAction =
        pickedPathToolsMenu->addAction(tr("Start Branching Curve from Picked Path"));
    connect(startBranchingCurveAction, &QAction::triggered, this, &MainWindow::startBranchingCurveFromPickedPath);

    QAction* addBranchAction = pickedPathToolsMenu->addAction(tr("Add Picked Path as &Branch"));
    connect(addBranchAction, &QAction::triggered, this, &MainWindow::addPickedPathAsBranch);

    QAction* cancelBranchingCurveAction = pickedPathToolsMenu->addAction(tr("Cancel Branching Curve"));
    connect(cancelBranchingCurveAction, &QAction::triggered, this, &MainWindow::cancelBranchingCurve);

    QAction* createResonanceFromGraphAction =
        pickedPathToolsMenu->addAction(tr("Create Resonance from Picked &Graph..."));
    connect(createResonanceFromGraphAction, &QAction::triggered, this,
            &MainWindow::createResonanceFromPickedGraph);

    // Pre-Beta Plan v0.Y.77.1: paints along a silent guide path.
    QAction* strokePickedPathAction = pickedPathToolsMenu->addAction(tr("Stroke Picked Path with Current &Tool"));
    connect(strokePickedPathAction, &QAction::triggered, this, &MainWindow::strokePickedPath);

    QAction* mergePickedPathsAction = pickedPathToolsMenu->addAction(tr("&Merge Picked Paths into Branching Path"));
    connect(mergePickedPathsAction, &QAction::triggered, this, &MainWindow::mergePickedPaths);

    // When each of the actions above applies - see updateEditActionStates().
    {
        ToolPaletteController* const palette = toolPaletteController_;
        const auto picked = [palette]() { return palette->selectedOperationId().has_value(); };
        const auto anyPicked = [palette]() { return palette->hasPickedObjects(); };
        const auto editing = [palette]() { return palette->isPathEditActive(); };
        const auto singlePath = [palette]() { return palette->selectedPath().has_value(); };
        const auto selection = [palette]() { return palette->hasSelection(); };
        const auto session = [this]() { return branchCurveSession_.isActive(); };
        editActionConditions_ = {
            {undoAction, [this]() { return history_.canUndo(); }},
            {redoAction, [this]() { return history_.canRedo(); }},
            {deleteAction, [anyPicked]() { return anyPicked(); }},
            {bringToFrontAction, [picked, editing]() { return picked() && !editing(); }},
            {sendToBackAction, [picked, editing]() { return picked() && !editing(); }},
            {bringForwardAction, [picked, editing]() { return picked() && !editing(); }},
            {sendBackwardAction, [picked, editing]() { return picked() && !editing(); }},
            {editPathAction, [singlePath, editing]() { return singlePath() && !editing(); }},
            {toggleNodeTypeAction, [palette]() { return palette->hasSelectedPathNode(); }},
            {applyPathEditAction, editing},
            {cancelPathEditAction, editing},
            {deselectAction, selection},
            {fillAction, selection},
            {applyFilterToSelectionAction, selection},
            {cutAction, selection},
            {copyAction, [selection, anyPicked]() { return selection() || anyPicked(); }},
            {pasteAction, [palette]() { return palette->hasClipboard() || palette->pastesObjects(); }},
            {captureMindShotAction, selection},
            {captureMindGrainAction, selection},
            {usePickedPathAsMindWaveShapeAction, singlePath},
            {createResonanceAction, [palette]() { return !palette->selectedBranches().empty(); }},
            {startBranchingCurveAction, [singlePath, session]() { return singlePath() && !session(); }},
            {addBranchAction,
             [this, singlePath]() { return branchCurveSession_.hasPendingGraft() && singlePath(); }},
            {cancelBranchingCurveAction, session},
            {createResonanceFromGraphAction, session},
            {strokePickedPathAction, [palette]() { return palette->selectedIsGuidePath(); }},
            {mergePickedPathsAction, [palette]() { return palette->canMergePickedPaths(); }},
        };
    }

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
    QMenu* layersMenu = editMenu->addMenu(tr("&Layers"));
    QAction* selectLayerAboveAction = layersMenu->addAction(tr("Select Layer &Above"));
    selectLayerAboveAction->setShortcut(QKeySequence(Qt::Key_PageUp));
    connect(selectLayerAboveAction, &QAction::triggered, this, &MainWindow::selectLayerAbove);

    QAction* selectLayerBelowAction = layersMenu->addAction(tr("Select Layer &Below"));
    selectLayerBelowAction->setShortcut(QKeySequence(Qt::Key_PageDown));
    connect(selectLayerBelowAction, &QAction::triggered, this, &MainWindow::selectLayerBelow);

    QAction* moveLayerUpAction = layersMenu->addAction(tr("&Move Layer Up"));
    moveLayerUpAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_PageUp));
    connect(moveLayerUpAction, &QAction::triggered, this, &MainWindow::moveSelectedLayerUp);

    QAction* moveLayerDownAction = layersMenu->addAction(tr("M&ove Layer Down"));
    moveLayerDownAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_PageDown));
    connect(moveLayerDownAction, &QAction::triggered, this, &MainWindow::moveSelectedLayerDown);

    QAction* increaseLayerOpacityAction = layersMenu->addAction(tr("&Increase Layer Opacity"));
    increaseLayerOpacityAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp));
    connect(increaseLayerOpacityAction, &QAction::triggered, this, &MainWindow::increaseSelectedLayerOpacity);

    QAction* decreaseLayerOpacityAction = layersMenu->addAction(tr("&Decrease Layer Opacity"));
    decreaseLayerOpacityAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown));
    connect(decreaseLayerOpacityAction, &QAction::triggered, this, &MainWindow::decreaseSelectedLayerOpacity);
}

MainWindow::ZoomActions MainWindow::buildViewMenu() {
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

    // Sound Flower (v0.Y.53.1) - a polar-coordinate view over the exact
    // same canvas data (see CanvasWidget::setPolarMode()'s own docs).
    // Not persisted (unlike Hardware Acceleration above) - a fresh project
    // always opens in flat view, matching the legacy Python Studio's own
    // precedent.
    QAction* soundFlowerAction = viewMenu->addAction(tr("Sound &Flower"));
    soundFlowerAction->setCheckable(true);
    soundFlowerAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F));
    soundFlowerAction->setToolTip(
        tr("Display the spectrogram as a Sound Flower - a polar coordinate\n"
           "projection where time wraps around the ring and frequency radiates\n"
           "outward from the centre. Painting works in polar mode."));
    connect(soundFlowerAction, &QAction::toggled, this, &MainWindow::setSoundFlowerModeEnabled);

    QAction* showClippingAction = viewMenu->addAction(tr("Show &Clipping"));
    showClippingAction->setObjectName(QStringLiteral("showClippingAction"));
    showClippingAction->setCheckable(true);
    showClippingAction->setToolTip(
        tr("Mark every spot louder than full scale (0 dBFS) with a magenta/black\n"
           "checker - those spots will distort (clip) when played or exported."));
    connect(showClippingAction, &QAction::toggled, canvas_, &CanvasWidget::setClipIndicatorEnabled);

    // Composer Mode's own toggle, moved here from a standalone
    // transportToolBar action - real-world testing pass 2026-09-29,
    // confirmed with the user.
    viewMenu->addSeparator();
    viewMenu->addAction(composerPanel_->toggleViewAction());
    viewMenu->addAction(analysisPanel_->toggleViewAction());

    return ZoomActions{zoomInAction, zoomOutAction, zoomToFitAction, zoomToActualSizeAction};
}

void MainWindow::buildHelpMenu() {
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

    // Resource Browser's own toggle, promoted from the Configure dropdown
    // to a top-level menu-bar entry (between View and Help) - confirmed
    // with the user; unlike the Configure dropdown's other panels, the
    // Resource Browser is used often enough on its own (not just as one
    // of many "configure a tool" panels) to warrant one less click.
    menuBar()->addAction(resourceBrowserPanel_->toggleViewAction());

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
}

void MainWindow::buildToolBars(const ZoomActions& zoomActions) {
    QAction* zoomInAction = zoomActions.zoomIn;
    QAction* zoomOutAction = zoomActions.zoomOut;
    QAction* zoomToFitAction = zoomActions.fit;
    QAction* zoomToActualSizeAction = zoomActions.actualSize;

    QToolBar* transportToolBar = addToolBar(tr("Transport"));
    // Plain text actions rather than icons - no icon assets exist yet, and
    // these are unambiguous enough on their own for a first pass.

    // Tool dropdown (v0.Y.58.1, "Reduce top-level buttons" UI polish): a
    // single QToolButton/QMenu collapsing what used to be five separate
    // checkable toolbar actions (Paint/Pick/Select/Path, and a Chord tool since
    // removed in v0.1.50.1) plus a new
    // Pan entry, per the roadmap's own "Choosing between Paint/Pick/Path/
    // Chord should be a single Tool drop-down option" spec (Select folds
    // in as a sixth entry - it's a full peer tool mode in CanvasWidget::
    // ToolMode already, not one of the four named, but dropping it wasn't
    // an option). Each entry is still its own real QAction (not a
    // QComboBox item) so per-entry enable/disable + tooltip keeps working
    // exactly as before for the Mind Grain guardrail (see
    // updateMindGrainGuardrails()'s own docs) - only *where* the actions
    // live changed, not what they are or how setExclusiveToolMode()
    // manages them.
    panAction_ = new QAction(tr("Pan"), this);
    panAction_->setCheckable(true);
    panAction_->setToolTip(tr("Navigate the canvas without painting, picking, or placing anything"));
    connect(panAction_, &QAction::toggled, this, &MainWindow::setPanModeEnabled);

    // A plain checkable toggle - this isn't a dock's own visibility, it's
    // the canvas's current tool mode (see CanvasWidget::setToolMode()'s own
    // docs).
    paintAction_ = new QAction(tr("Paint"), this);
    paintAction_->setCheckable(true);
    connect(paintAction_, &QAction::toggled, this, &MainWindow::setPaintModeEnabled);

    // Pick (v0.Y.24.1): the same kind of plain checkable toggle as Paint
    // above, for CanvasWidget::ToolMode::Pick. Deliberately *not* grouped
    // with paintAction_ via QActionGroup - see setPaintModeEnabled()'s own
    // docs on why mutual exclusivity is instead handled directly, by hand,
    // in setPaintModeEnabled()/setPickModeEnabled() themselves.
    pickAction_ = new QAction(tr("Pick"), this);
    pickAction_->setCheckable(true);
    connect(pickAction_, &QAction::toggled, this, &MainWindow::setPickModeEnabled);

    // Selection & Fill (v0.Y.25.1): the same kind of plain checkable
    // toggle as Paint/Pick above, for CanvasWidget::ToolMode::Select.
    selectAction_ = new QAction(tr("Select"), this);
    selectAction_->setCheckable(true);
    connect(selectAction_, &QAction::toggled, this, &MainWindow::setSelectModeEnabled);

    // Paths & Grids (v0.Y.26.1): the same kind of plain checkable toggle
    // as Paint/Pick/Select above, for CanvasWidget::ToolMode::Path.
    pathAction_ = new QAction(tr("Path"), this);
    pathAction_->setCheckable(true);
    connect(pathAction_, &QAction::toggled, this, &MainWindow::setPathModeEnabled);

    // The Text tool (v0.Y.78.1) - see setTextModeEnabled().
    textAction_ = new QAction(tr("Text"), this);
    textAction_->setCheckable(true);
    connect(textAction_, &QAction::toggled, this, &MainWindow::setTextModeEnabled);

    auto* toolMenu = new QMenu(this);
    toolMenu->setToolTipsVisible(true);
    toolMenu->addAction(panAction_);
    toolMenu->addAction(paintAction_);
    toolMenu->addAction(pickAction_);
    toolMenu->addAction(selectAction_);
    toolMenu->addAction(pathAction_);
    toolMenu->addAction(textAction_);

    toolButton_ = new QToolButton(this);
    // QToolBar creates its own internal, unnamed QToolButton for every
    // plain QAction added to it (every dock's
    // toggleViewAction(), etc.) - an explicit object name is what lets
    // tests reliably find *this* one via findChild<QToolButton*>(name)
    // rather than matching one of those by accident.
    toolButton_->setObjectName(QStringLiteral("toolButton"));
    toolButton_->setPopupMode(QToolButton::InstantPopup);
    toolButton_->setMenu(toolMenu);
    toolButton_->setToolTip(tr("Choose the canvas's current input tool"));
    transportToolBar->addWidget(toolButton_);

    // Establishes the "exactly one of the six is checked" invariant
    // setExclusiveToolMode() relies on from here on, and sets toolButton_'s
    // own initial label - matches ToolMode::None already being the
    // documented default (see paintModeIsOffByDefault()'s own test).
    panAction_->setChecked(true);

    // As of v0.Y.16.1 (Transport Panels): Play/Pause/Stop/Loop/Record are
    // no longer direct toolbar actions - each now lives inside its own
    // dock panel (see the panel construction above). toggleViewAction() is
    // Qt's own ready-made action for a dock's own show/hide toggle - it
    // stays in sync with the dock's actual visibility automatically, no
    // manual signal wiring needed (unlike a hand-rolled checkable QAction
    // would).
    // The Layers panel gets this kind of toggle directly on the toolbar -
    // ON by default, matching its pre-existing "just show it" behavior;
    // confirmed with the user, along with visibility persisting across
    // project switches within a session rather than resetting every time.
    transportToolBar->addAction(layersPanel_->toggleViewAction());

    // "Configure" dropdown (v0.Y.58.1, "Reduce top-level buttons", the
    // last sub-item) - collects every remaining per-feature configuration
    // panel's own toggleViewAction() behind one button, the same "reuse
    // each dock's own self-syncing toggle directly as a menu entry"
    // pattern the Input/Output dropdown already established (unlike the
    // Tool dropdown's hand-rolled QActions - there's no separate "mode"
    // concept here either, just a dock's own visibility). Unlike
    // Input/Output, there's no mutual exclusivity - any number of these
    // can stay open together, this is purely decluttering the toolbar.
    // MIDI Configuration joins the four panels the
    // roadmap's own spec named explicitly (Selection/Filter/MindWaves/
    // Tool Configuration/Grid) - confirmed with the user (see
    // docs/sound-mind-architecture.md's Decision #179 on where this and
    // the other roadmap-silent panels/actions were confirmed to land).
    auto* configureMenu = new QMenu(this);
    configureMenu->addAction(toolConfigurationPanel_->toggleViewAction());
    configureMenu->addAction(midiConfigurationPanel_->toggleViewAction());
    configureMenu->addAction(selectionConfigurationPanel_->toggleViewAction());
    configureMenu->addAction(gridPanel_->toggleViewAction());
    configureMenu->addAction(filterConfigurationPanel_->toggleViewAction());
    configureMenu->addAction(mindWavesPanel_->toggleViewAction());

    auto* configureButton = new QToolButton(this);
    configureButton->setObjectName(QStringLiteral("configureButton"));
    configureButton->setText(tr("Configure"));
    configureButton->setPopupMode(QToolButton::InstantPopup);
    configureButton->setMenu(configureMenu);
    configureButton->setToolTip(tr("Open Tool/MIDI/Selection/Filter Configuration, Grid, or MindWaves"));
    transportToolBar->addWidget(configureButton);

    // Off by default, same reasoning - see historyPanel_'s own docs.
    transportToolBar->addAction(historyPanel_->toggleViewAction());
    // composerPanel_'s own toggle and the three Macro actions now live
    // elsewhere - see their own construction sites (the View menu, and the
    // top-right "Macro" dropdown, respectively) for why.

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
}

}  // namespace sound_mind::studio
