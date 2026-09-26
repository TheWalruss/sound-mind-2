#include "test_layer_controller.h"

#include <cmath>

#include <QComboBox>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/filter_configuration_panel.h"
#include "sound_mind/studio/layer_controller.h"
#include "sound_mind/studio/layers_panel.h"
#include "sound_mind/studio/playback_controller.h"
#include "sound_mind/studio/undo_stack.h"

using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::MindWaveId;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::studio::CanvasWidget;
using sound_mind::studio::FilterConfigurationPanel;
using sound_mind::studio::LayerController;
using sound_mind::studio::LayersPanel;
using sound_mind::studio::PlaybackController;
using sound_mind::studio::UndoStack;
using sound_mind::studio::UndoStack;

namespace {

ProjectSettings testSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 20;
    settings.canvasHeight = 10;
    settings.binCount = 10;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 2020.0f;
    settings.timestepMs = 10.0;
    return settings;
}

/// @brief Owns every collaborator a LayerController needs, so each test
/// only has to build the controller itself.
struct Fixture {
    CanvasWidget canvas;
    PlaybackController playbackController{nullptr, sound_mind::core::AudioDeviceMode::None};
    LayersPanel layersPanel;
    FilterConfigurationPanel filterConfigurationPanel;
    UndoStack undoStack;
    LayerController controller{&canvas, &playbackController, &layersPanel, &filterConfigurationPanel, &undoStack};
};

}  // namespace

void LayerControllerTest::withNoProjectLookupsReturnNulloptOrNullptr() {
    Fixture fixture;
    QVERIFY(fixture.controller.topmostLayerWithContent() == nullptr);
    QVERIFY(fixture.controller.layerById(1) == nullptr);
    QVERIFY(!fixture.controller.paintTargetLayerId().has_value());
}

void LayerControllerTest::setProjectMakesLookupsWork() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);

    QVERIFY(fixture.controller.layerById(backgroundId) != nullptr);
    QCOMPARE(fixture.controller.paintTargetLayerId().value(), backgroundId);
}

void LayerControllerTest::cycleLayerVisibilityStateCyclesVisibleMutedInvisibleAndEmitsLayersChanged() {
    // v0.Y.46.1 Installment B ("Layers Panel & Editing Enhancements v2") -
    // replaced the old plain on/off toggleLayerVisibility().
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);
    QSignalSpy spy(&fixture.controller, &LayerController::layersChanged);
    QVERIFY(project.layers().front().visible());
    QVERIFY(!project.layers().front().muted());

    fixture.controller.cycleLayerVisibilityState(backgroundId);  // Visible -> Muted.
    QVERIFY(project.layers().front().visible());
    QVERIFY(project.layers().front().muted());
    QCOMPARE(spy.count(), 1);

    fixture.controller.cycleLayerVisibilityState(backgroundId);  // Muted -> Invisible.
    QVERIFY(!project.layers().front().visible());
    QVERIFY(!project.layers().front().muted());
    QCOMPARE(spy.count(), 2);

    fixture.controller.cycleLayerVisibilityState(backgroundId);  // Invisible -> Visible.
    QVERIFY(project.layers().front().visible());
    QVERIFY(!project.layers().front().muted());
    QCOMPARE(spy.count(), 3);
}

void LayerControllerTest::setLayerOpacityChangesOpacity() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);

    fixture.controller.setLayerOpacity(backgroundId, 0.5f);

    QCOMPARE(project.layers().front().opacity(), 0.5f);
}

void LayerControllerTest::setLayerOpacityMindWaveChangesBindingAndTheRowDataReflectsIt() {
    // Regression test for a real bug: refreshLayersPanel() built each
    // row's RowData without ever setting opacityMindWaveId, so a bound
    // MindWave took effect on the canvas but the Layers Panel's own combo
    // reset to "None" on every refresh - which fires after every single
    // mutation, including the bind itself.
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);
    // Not the Background layer - it has no opacity/MindWave combo at all
    // (see LayersPanelTest::backgroundLayerHasNoOpacityOrTransformControls).
    // Project::createNew() also always carries an Equalizer layer (kept
    // last via addLayer()'s own docs) - it *does* get a combo too (only
    // Background is excluded), so this project ends up with two: the
    // Equalizer's own (untouched, still "None") and this new layer's own.
    const LayerId layerId = project.addLayer(Layer{});
    fixture.layersPanel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});

    fixture.controller.setLayerOpacityMindWave(layerId, MindWaveId{5});
    // setLayerOpacityMindWave() -> refreshLayersPanel() -> setLayers()
    // rebuilds row widgets and schedules the *previous* pass's own combo
    // for deleteLater() - see test_mind_wave_controller.cpp's own
    // identical precedent for why qWait(0) is needed before inspecting it.
    QTest::qWait(0);

    QCOMPARE(project.layerById(layerId)->opacityMindWave(), std::optional<MindWaveId>(MindWaveId{5}));
    // Rows display top-of-stack first: Equalizer (untouched), then this
    // new layer, then Background (no combo at all) - see rebuildRows()'s
    // own reverse-iteration docs.
    const auto combos = fixture.layersPanel.findChildren<QComboBox*>(QStringLiteral("opacityMindWaveCombo"));
    QCOMPARE(combos.size(), 2);
    QCOMPARE(combos.at(0)->currentText(), QStringLiteral("None"));  // Equalizer - untouched.
    QCOMPARE(combos.at(1)->currentText(), QStringLiteral("Slow Pulse"));  // This test's own layer.
}

void LayerControllerTest::setLayerTranslationChangesTranslation() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);

    fixture.controller.setLayerTranslation(backgroundId, 7);

    QCOMPARE(project.layers().front().translationColumns(), static_cast<std::int64_t>(7));
}

void LayerControllerTest::setLayerRescaleChangesRescale() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);

    fixture.controller.setLayerRescale(backgroundId, 2.0);

    QCOMPARE(project.layers().front().rescaleFactor(), 2.0);
}

void LayerControllerTest::cycleLayerVisibilityStateIsUndoableAndRedoable() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);
    QVERIFY(project.layers().front().visible());  // The default, undone-to value.
    QVERIFY(!project.layers().front().muted());

    fixture.controller.cycleLayerVisibilityState(backgroundId);  // Visible -> Muted.
    QVERIFY(project.layers().front().visible());
    QVERIFY(project.layers().front().muted());
    QVERIFY(fixture.undoStack.canUndo());

    fixture.undoStack.undo();
    QVERIFY(project.layers().front().visible());
    QVERIFY(!project.layers().front().muted());
    QVERIFY(fixture.undoStack.canRedo());

    fixture.undoStack.redo();
    QVERIFY(project.layers().front().visible());
    QVERIFY(project.layers().front().muted());
}

void LayerControllerTest::setLayerOpacityIsUndoableAndRedoable() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);
    const float oldOpacity = project.layers().front().opacity();

    fixture.controller.setLayerOpacity(backgroundId, 0.5f);
    QCOMPARE(project.layers().front().opacity(), 0.5f);
    QVERIFY(fixture.undoStack.canUndo());

    fixture.undoStack.undo();
    QCOMPARE(project.layers().front().opacity(), oldOpacity);

    fixture.undoStack.redo();
    QCOMPARE(project.layers().front().opacity(), 0.5f);
}

void LayerControllerTest::setLayerOpacityMindWaveIsUndoableAndRedoable() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);
    QVERIFY(!project.layers().front().opacityMindWave().has_value());

    fixture.controller.setLayerOpacityMindWave(backgroundId, MindWaveId{5});
    QCOMPARE(project.layers().front().opacityMindWave(), std::optional<MindWaveId>(MindWaveId{5}));
    QVERIFY(fixture.undoStack.canUndo());

    fixture.undoStack.undo();
    QVERIFY(!project.layers().front().opacityMindWave().has_value());

    fixture.undoStack.redo();
    QCOMPARE(project.layers().front().opacityMindWave(), std::optional<MindWaveId>(MindWaveId{5}));
}

void LayerControllerTest::setLayerTranslationIsUndoableAndRedoable() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);
    const std::int64_t oldTranslation = project.layers().front().translationColumns();

    fixture.controller.setLayerTranslation(backgroundId, 7);
    QCOMPARE(project.layers().front().translationColumns(), static_cast<std::int64_t>(7));
    QVERIFY(fixture.undoStack.canUndo());

    fixture.undoStack.undo();
    QCOMPARE(project.layers().front().translationColumns(), oldTranslation);

    fixture.undoStack.redo();
    QCOMPARE(project.layers().front().translationColumns(), static_cast<std::int64_t>(7));
}

void LayerControllerTest::setLayerRescaleIsUndoableAndRedoable() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);
    const double oldRescale = project.layers().front().rescaleFactor();

    fixture.controller.setLayerRescale(backgroundId, 2.0);
    QCOMPARE(project.layers().front().rescaleFactor(), 2.0);
    QVERIFY(fixture.undoStack.canUndo());

    fixture.undoStack.undo();
    QCOMPARE(project.layers().front().rescaleFactor(), oldRescale);

    fixture.undoStack.redo();
    QCOMPARE(project.layers().front().rescaleFactor(), 2.0);
}

void LayerControllerTest::settingTheSameValueAgainDoesNotPushAnUndoEntry() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);
    const float currentOpacity = project.layers().front().opacity();

    fixture.controller.setLayerOpacity(backgroundId, currentOpacity);

    // The value still applies (no behavior change there), but a no-op
    // change shouldn't clutter the undo history with an entry that would
    // do nothing.
    QCOMPARE(project.layers().front().opacity(), currentOpacity);
    QVERIFY(!fixture.undoStack.canUndo());
}

void LayerControllerTest::setLayerBlendModeChangesBlendModeAndTheRowDataReflectsIt() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);
    // Not the Background layer - same reasoning as
    // setLayerOpacityMindWaveChangesBindingAndTheRowDataReflectsIt().
    const LayerId layerId = project.addLayer(Layer{});

    fixture.controller.setLayerBlendMode(layerId, sound_mind::core::BlendMode::Multiply);
    QTest::qWait(0);  // Same reasoning as the opacityMindWave test's own qWait(0).

    QCOMPARE(project.layerById(layerId)->blendMode(), sound_mind::core::BlendMode::Multiply);
    // Rows display top-of-stack first: Equalizer (untouched), then this
    // new layer, then Background (no combo at all).
    const auto combos = fixture.layersPanel.findChildren<QComboBox*>(QStringLiteral("blendModeCombo"));
    QCOMPARE(combos.size(), 2);
    QCOMPARE(combos.at(0)->currentText(), QStringLiteral("Normal"));    // Equalizer - untouched.
    QCOMPARE(combos.at(1)->currentText(), QStringLiteral("Multiply"));  // This test's own layer.
}

void LayerControllerTest::setLayerBlendModeIsUndoableAndRedoable() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);
    QCOMPARE(project.layers().front().blendMode(), sound_mind::core::BlendMode::Normal);

    fixture.controller.setLayerBlendMode(backgroundId, sound_mind::core::BlendMode::Multiply);
    QCOMPARE(project.layers().front().blendMode(), sound_mind::core::BlendMode::Multiply);
    QVERIFY(fixture.undoStack.canUndo());

    fixture.undoStack.undo();
    QCOMPARE(project.layers().front().blendMode(), sound_mind::core::BlendMode::Normal);

    fixture.undoStack.redo();
    QCOMPARE(project.layers().front().blendMode(), sound_mind::core::BlendMode::Multiply);
}

void LayerControllerTest::renameLayerToRenamesAndRejectsEmptyName() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);

    QVERIFY(fixture.controller.renameLayerTo(backgroundId, QStringLiteral("Floor")));
    QCOMPARE(QString::fromStdString(project.layers().front().name()), QStringLiteral("Floor"));
    QVERIFY(!fixture.controller.renameLayerTo(backgroundId, QString()));
}

void LayerControllerTest::deleteLayerRemovesALayerButRefusesALockedOne() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    const LayerId equalizerId = project.layers().back().id();
    Layer normal(0, "Normal", LayerType::Normal);
    const LayerId normalId = project.addLayer(std::move(normal));
    fixture.controller.setProject(&project);
    const std::size_t countBefore = project.layers().size();

    fixture.controller.deleteLayer(backgroundId);  // locked - refused.
    QCOMPARE(project.layers().size(), countBefore);
    fixture.controller.deleteLayer(equalizerId);  // locked - refused.
    QCOMPARE(project.layers().size(), countBefore);

    fixture.controller.deleteLayer(normalId);  // not locked - removed.
    QCOMPARE(project.layers().size(), countBefore - 1);
    QVERIFY(fixture.controller.layerById(normalId) == nullptr);
}

void LayerControllerTest::duplicateLayerCopiesContentAndPropertiesButRefusesALockedOne() {
    // Real-world testing pass finding #22.
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    const LayerId equalizerId = project.layers().back().id();
    Layer normal(0, "Normal", LayerType::Normal);
    normal.setOpacity(0.42f);
    sound_mind::codec::StreamImage content;
    content.config = sound_mind::core::streamCodecConfigFor(project.settings());
    content.frameCount = project.settings().canvasWidth;
    const std::size_t pixelCount = std::size_t{content.config.binCount} * content.frameCount;
    content.leftMagnitudeDb.assign(pixelCount, -10.0f);
    content.rightMagnitudeDb.assign(pixelCount, -20.0f);
    content.sharedPhaseRadians.assign(pixelCount, 0.0f);
    normal.setContent(content);
    const LayerId normalId = project.addLayer(std::move(normal));
    fixture.controller.setProject(&project);
    const std::size_t countBefore = project.layers().size();

    fixture.controller.duplicateLayer(backgroundId);  // locked - refused.
    QCOMPARE(project.layers().size(), countBefore);
    fixture.controller.duplicateLayer(equalizerId);  // locked - refused.
    QCOMPARE(project.layers().size(), countBefore);

    fixture.controller.duplicateLayer(normalId);  // not locked - duplicated.

    QCOMPARE(project.layers().size(), countBefore + 1);
    // duplicateLayer() selects the new copy immediately - the same
    // mechanism addEmptyLayerAddsAndSelectsANormalLayer() already relies
    // on, and more direct than assuming any particular position in
    // layers() (addLayer() inserts just below an existing Equalizer
    // layer, not necessarily at the very end - see its own docs).
    QVERIFY(fixture.layersPanel.selectedLayerId().has_value());
    const LayerId duplicateId = *fixture.layersPanel.selectedLayerId();
    QVERIFY(duplicateId != normalId);
    const Layer* duplicate = fixture.controller.layerById(duplicateId);
    QVERIFY(duplicate != nullptr);
    QCOMPARE(duplicate->type(), LayerType::Normal);
    QCOMPARE(duplicate->opacity(), 0.42f);
    QVERIFY(duplicate->content().has_value());
    QCOMPARE(duplicate->content()->leftMagnitudeDb.front(), -10.0f);
    QCOMPARE(duplicate->content()->rightMagnitudeDb.front(), -20.0f);
}

void LayerControllerTest::duplicateLayerNamesTheCopyUniquelyAndSelectsIt() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    Layer normal(0, "My Layer", LayerType::Normal);
    const LayerId normalId = project.addLayer(std::move(normal));
    fixture.controller.setProject(&project);

    fixture.controller.duplicateLayer(normalId);

    QVERIFY(fixture.layersPanel.selectedLayerId().has_value());
    const LayerId duplicateId = *fixture.layersPanel.selectedLayerId();
    const Layer* duplicate = fixture.controller.layerById(duplicateId);
    QVERIFY(duplicate != nullptr);
    // addLayer()'s own uniqueLayerName() dedupes the identical copied name
    // automatically - see LayerController::duplicateLayer()'s own docs.
    QCOMPARE(QString::fromStdString(duplicate->name()), QStringLiteral("My Layer (2)"));
}

void LayerControllerTest::cleanUpLayerPhaseZeroesPhaseInSilentCellsOnlyButRefusesALayerWithNoContent() {
    // Real-world testing pass finding #24.
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    Layer empty(0, "Empty", LayerType::Normal);
    const LayerId emptyId = project.addLayer(std::move(empty));

    Layer normal(0, "Normal", LayerType::Normal);
    sound_mind::codec::StreamImage content;
    content.config = sound_mind::core::streamCodecConfigFor(project.settings());
    content.frameCount = project.settings().canvasWidth;
    const std::size_t pixelCount = std::size_t{content.config.binCount} * content.frameCount;
    content.leftMagnitudeDb.assign(pixelCount, -10.0f);   // audible.
    content.rightMagnitudeDb.assign(pixelCount, -10.0f);  // audible.
    content.sharedPhaseRadians.assign(pixelCount, 1.0f);
    // One silent cell, with junk phase that should get zeroed.
    content.leftMagnitudeDb[0] = -96.0f;
    content.rightMagnitudeDb[0] = -96.0f;
    normal.setContent(content);
    const LayerId normalId = project.addLayer(std::move(normal));

    fixture.controller.setProject(&project);
    QSignalSpy spy(&fixture.controller, &LayerController::layersChanged);

    fixture.controller.cleanUpLayerPhase(emptyId);  // no content - refused.
    QCOMPARE(spy.count(), 0);

    fixture.controller.cleanUpLayerPhase(normalId);  // has content - cleaned up.
    QCOMPARE(spy.count(), 1);

    const Layer* cleaned = fixture.controller.layerById(normalId);
    QVERIFY(cleaned != nullptr);
    QVERIFY(cleaned->content().has_value());
    QCOMPARE(cleaned->content()->sharedPhaseRadians[0], 0.0f);   // silent cell - zeroed.
    QCOMPARE(cleaned->content()->sharedPhaseRadians[1], 1.0f);   // audible cell - untouched.
    QCOMPARE(cleaned->content()->leftMagnitudeDb[0], -96.0f);    // magnitudes never rewritten.
    QCOMPARE(cleaned->content()->rightMagnitudeDb[0], -96.0f);
}

void LayerControllerTest::selectLayerAboveAndBelowNavigateTheStackAndNoOpAtTheEnds() {
    // v0.Y.46.1 Installment A ("Layers Panel & Editing Enhancements v2").
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    const LayerId equalizerId = project.layers().back().id();
    Layer normal(0, "Normal", LayerType::Normal);
    const LayerId normalId = project.addLayer(std::move(normal));  // Background, Normal, Equalizer.
    fixture.controller.setProject(&project);
    fixture.controller.refreshLayersPanel();

    // No-op with nothing selected.
    fixture.controller.selectLayerAbove();
    QVERIFY(!fixture.layersPanel.selectedLayerId().has_value());
    fixture.controller.selectLayerBelow();
    QVERIFY(!fixture.layersPanel.selectedLayerId().has_value());

    fixture.layersPanel.selectLayer(backgroundId);

    fixture.controller.selectLayerAbove();
    QCOMPARE(*fixture.layersPanel.selectedLayerId(), normalId);

    fixture.controller.selectLayerAbove();
    QCOMPARE(*fixture.layersPanel.selectedLayerId(), equalizerId);

    fixture.controller.selectLayerAbove();  // already topmost - no-op.
    QCOMPARE(*fixture.layersPanel.selectedLayerId(), equalizerId);

    fixture.controller.selectLayerBelow();
    QCOMPARE(*fixture.layersPanel.selectedLayerId(), normalId);

    fixture.controller.selectLayerBelow();
    QCOMPARE(*fixture.layersPanel.selectedLayerId(), backgroundId);

    fixture.controller.selectLayerBelow();  // already bottommost - no-op.
    QCOMPARE(*fixture.layersPanel.selectedLayerId(), backgroundId);
}

void LayerControllerTest::moveSelectedLayerUpAndDownReorderTheStackAndRefuseAtLockedBoundaries() {
    // v0.Y.46.1 Installment A.
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    Layer first(0, "First", LayerType::Normal);
    const LayerId firstId = project.addLayer(std::move(first));
    Layer second(0, "Second", LayerType::Normal);
    const LayerId secondId = project.addLayer(std::move(second));
    // Bottom-to-top: Background, First, Second, Equalizer.
    fixture.controller.setProject(&project);
    fixture.controller.refreshLayersPanel();

    // No-op with nothing selected.
    fixture.controller.moveSelectedLayerUp();
    fixture.controller.moveSelectedLayerDown();
    QCOMPARE(project.layers()[1].id(), firstId);
    QCOMPARE(project.layers()[2].id(), secondId);

    fixture.layersPanel.selectLayer(firstId);
    fixture.controller.moveSelectedLayerUp();  // swaps First/Second.
    QCOMPARE(project.layers()[1].id(), secondId);
    QCOMPARE(project.layers()[2].id(), firstId);
    QCOMPARE(*fixture.layersPanel.selectedLayerId(), firstId);  // selection follows by id.

    fixture.controller.moveSelectedLayerUp();  // First is now just below the locked Equalizer - refused.
    QCOMPARE(project.layers()[2].id(), firstId);

    fixture.controller.moveSelectedLayerDown();  // back to Background, First, Second, Equalizer.
    QCOMPARE(project.layers()[1].id(), firstId);
    QCOMPARE(project.layers()[2].id(), secondId);

    fixture.controller.moveSelectedLayerDown();  // First is now just above the locked Background - refused.
    QCOMPARE(project.layers()[1].id(), firstId);

    // Selecting a locked layer itself refuses either direction.
    const LayerId backgroundId = project.layers().front().id();
    const LayerId equalizerId = project.layers().back().id();
    fixture.layersPanel.selectLayer(backgroundId);
    fixture.controller.moveSelectedLayerUp();
    QCOMPARE(project.layers().front().id(), backgroundId);  // unchanged.
    fixture.layersPanel.selectLayer(equalizerId);
    fixture.controller.moveSelectedLayerDown();
    QCOMPARE(project.layers().back().id(), equalizerId);  // unchanged.
}

void LayerControllerTest::nudgeSelectedLayerOpacityClampsAndIsUndoable() {
    // v0.Y.46.1 Installment A.
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    Layer normal(0, "Normal", LayerType::Normal);
    normal.setOpacity(0.5f);
    const LayerId normalId = project.addLayer(std::move(normal));
    fixture.controller.setProject(&project);
    fixture.controller.refreshLayersPanel();

    // No-op with nothing selected.
    QSignalSpy spy(&fixture.controller, &LayerController::layersChanged);
    fixture.controller.nudgeSelectedLayerOpacity(0.1f);
    QCOMPARE(spy.count(), 0);

    fixture.layersPanel.selectLayer(normalId);

    fixture.controller.nudgeSelectedLayerOpacity(0.05f);
    QVERIFY(std::abs(fixture.controller.layerById(normalId)->opacity() - 0.55f) < 0.001f);
    QVERIFY(fixture.undoStack.canUndo());

    fixture.undoStack.undo();
    QVERIFY(std::abs(fixture.controller.layerById(normalId)->opacity() - 0.5f) < 0.001f);

    fixture.undoStack.redo();
    QVERIFY(std::abs(fixture.controller.layerById(normalId)->opacity() - 0.55f) < 0.001f);

    // Clamped at both extremes.
    fixture.controller.nudgeSelectedLayerOpacity(10.0f);
    QCOMPARE(fixture.controller.layerById(normalId)->opacity(), 1.0f);

    fixture.controller.nudgeSelectedLayerOpacity(-10.0f);
    QCOMPARE(fixture.controller.layerById(normalId)->opacity(), 0.0f);
}

void LayerControllerTest::addEmptyLayerAddsAndSelectsANormalLayer() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);
    const std::size_t countBefore = project.layers().size();

    sound_mind::codec::StreamImage placeholder;
    placeholder.config = sound_mind::core::streamCodecConfigFor(project.settings());
    placeholder.frameCount = project.settings().canvasWidth;
    const std::size_t pixelCount = std::size_t{placeholder.config.binCount} * placeholder.frameCount;
    placeholder.leftMagnitudeDb.assign(pixelCount, 0.0f);
    placeholder.rightMagnitudeDb.assign(pixelCount, 0.0f);
    placeholder.sharedPhaseRadians.assign(pixelCount, 0.0f);
    fixture.controller.addEmptyLayer(placeholder);

    QCOMPARE(project.layers().size(), countBefore + 1);
    QVERIFY(fixture.layersPanel.selectedLayerId().has_value());
}

void LayerControllerTest::addFilterLayerAddsAndSelectsAFilterLayer() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);
    const std::size_t countBefore = project.layers().size();

    fixture.controller.addFilterLayer();

    QCOMPARE(project.layers().size(), countBefore + 1);
    const auto selectedId = fixture.layersPanel.selectedLayerId();
    QVERIFY(selectedId.has_value());
    QCOMPARE(fixture.controller.layerById(*selectedId)->type(), LayerType::Filter);
}

void LayerControllerTest::handleLayerSelectionChangedSyncsFilterConfigurationPanel() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();  // Normal - not a Filter layer.
    fixture.controller.setProject(&project);

    // Finding #13 (real-world testing pass, 2026-09-20): the panel stays
    // enabled regardless of whether the selection is a Filter/Equalizer
    // layer - see pendingFilterConfigurationTests below for what it shows/
    // edits in that case.
    fixture.controller.handleLayerSelectionChanged(backgroundId);
    QVERIFY(fixture.filterConfigurationPanel.isEnabled());

    fixture.controller.addFilterLayer();
    const auto filterId = fixture.layersPanel.selectedLayerId();
    fixture.controller.handleLayerSelectionChanged(filterId);
    QVERIFY(fixture.filterConfigurationPanel.isEnabled());

    fixture.controller.handleLayerSelectionChanged(std::nullopt);
    QVERIFY(fixture.filterConfigurationPanel.isEnabled());
}

void LayerControllerTest::applyFilterConfigurationAppliesOnlyToAFilterLayer() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);

    sound_mind::core::FilterConfiguration config;
    config.setBlurSigma(3.0f);

    // No real Filter/Equalizer layer selected in the LayersPanel itself -
    // updates pendingFilterConfiguration(), not any real layer - see
    // applyFilterConfigurationWithNoFilterLayerSelectedUpdatesThePendingConfiguration()
    // for the dedicated test of that.
    fixture.controller.applyFilterConfiguration(config);
    QCOMPARE(fixture.controller.layerById(backgroundId)->filterConfiguration().blurSigma(),
             sound_mind::core::FilterConfiguration{}.blurSigma());

    fixture.controller.addFilterLayer();
    const auto filterId = fixture.layersPanel.selectedLayerId();
    QVERIFY(filterId.has_value());

    fixture.controller.applyFilterConfiguration(config);
    QCOMPARE(fixture.controller.layerById(*filterId)->filterConfiguration().blurSigma(), 3.0f);
    // The Normal (Background) layer is untouched by a config meant for
    // whichever layer is currently selected in the panel.
    QVERIFY(backgroundId != *filterId);
}

void LayerControllerTest::applyFilterConfigurationWithNoFilterLayerSelectedUpdatesThePendingConfiguration() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);

    sound_mind::core::FilterConfiguration config;
    config.setBlurSigma(3.0f);

    // No selection at all.
    fixture.controller.applyFilterConfiguration(config);
    QCOMPARE(fixture.controller.pendingFilterConfiguration().blurSigma(), 3.0f);
}

void LayerControllerTest::addFilterLayerSeedsFromThePendingFilterConfiguration() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);

    sound_mind::core::FilterConfiguration config;
    config.setBlurSigma(3.0f);
    fixture.controller.applyFilterConfiguration(config);

    fixture.controller.addFilterLayer();
    const auto filterId = fixture.layersPanel.selectedLayerId();
    QVERIFY(filterId.has_value());
    QCOMPARE(fixture.controller.layerById(*filterId)->filterConfiguration().blurSigma(), 3.0f);
}

void LayerControllerTest::setProjectResetsThePendingFilterConfiguration() {
    Fixture fixture;
    Project firstProject = Project::createNew(testSettings());
    fixture.controller.setProject(&firstProject);

    sound_mind::core::FilterConfiguration config;
    config.setBlurSigma(3.0f);
    fixture.controller.applyFilterConfiguration(config);
    QCOMPARE(fixture.controller.pendingFilterConfiguration().blurSigma(), 3.0f);

    Project secondProject = Project::createNew(testSettings());
    fixture.controller.setProject(&secondProject);
    QCOMPARE(fixture.controller.pendingFilterConfiguration().blurSigma(),
             sound_mind::core::FilterConfiguration{}.blurSigma());
}

void LayerControllerTest::reorderLayersRefreshesEitherWay() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);

    // An invalid permutation (missing/duplicate ids) is rejected by
    // Project::reorderLayers() itself, but the panel is still refreshed -
    // this just needs to not crash.
    fixture.controller.reorderLayers({});
}

void LayerControllerTest::paintTargetLayerIdFallsBackToTheBottommostLayer() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);

    // Nothing selected in the panel yet - falls back to the bottommost
    // (Background) layer, not the Equalizer (always .back()).
    QCOMPARE(fixture.controller.paintTargetLayerId().value(), backgroundId);
}
