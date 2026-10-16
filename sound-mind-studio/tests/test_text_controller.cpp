#include "test_text_controller.h"

#include <algorithm>

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paste_operation.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/paint_controller.h"
#include "sound_mind/studio/pick_controller.h"
#include "sound_mind/studio/text_controller.h"
#include "sound_mind/studio/undo_stack.h"

using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::studio::PaintController;
using sound_mind::studio::TextController;
using sound_mind::studio::TextSettings;
using sound_mind::studio::UndoStack;

namespace {

ProjectSettings testSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 200;
    settings.canvasHeight = 100;
    settings.binCount = 100;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 2020.0f;
    settings.timestepMs = 10.0;
    return settings;
}

LayerId addBlankNormalLayer(Project& project) {
    Layer layer(0, "Text Layer", LayerType::Normal);
    sound_mind::codec::StreamImage content;
    content.config = sound_mind::core::streamCodecConfigFor(project.settings());
    content.frameCount = project.settings().canvasWidth;
    const std::size_t cells = std::size_t{content.config.binCount} * content.frameCount;
    content.leftMagnitudeDb.assign(cells, -96.0f);
    content.rightMagnitudeDb.assign(cells, -96.0f);
    content.sharedPhaseRadians.assign(cells, 0.0f);
    layer.setContent(content);
    return project.addLayer(std::move(layer));
}

TextSettings hello(int size = 20) {
    TextSettings settings;
    settings.text = QStringLiteral("Hi");
    settings.fontSize = size;
    settings.leftIntensityDb = -6.0f;
    settings.rightIntensityDb = -6.0f;
    settings.blendMode = sound_mind::core::BlendMode::Overwrite;
    return settings;
}

/// @brief The point at a (frame, bin) cell.
TimeFrequencyPoint pointAt(const Project& project, double frame, double bin) {
    const auto config = sound_mind::core::streamCodecConfigFor(project.settings());
    return {sound_mind::core::frameIndexToTime(frame, config),
            sound_mind::core::binIndexToFrequency(static_cast<float>(bin), config)};
}

}  // namespace

void TextControllerTest::renderGlyphsIsEmptyForBlankText() {
    TextSettings settings;
    settings.text = QStringLiteral("   ");
    QVERIFY(TextController::renderGlyphs(settings).isNull());
}

void TextControllerTest::renderGlyphsDrawsTheTextWithSomeCoverage() {
    const QImage image = TextController::renderGlyphs(hello(30));
    QVERIFY(!image.isNull());
    QVERIFY(image.height() >= 20);
    bool covered = false;
    for (int y = 0; y < image.height() && !covered; ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 128) {
                covered = true;
                break;
            }
        }
    }
    QVERIFY(covered);
}

void TextControllerTest::rasterizeStretchesToTheBox() {
    const auto raster = TextController::rasterize(hello(), 40, 12);
    QCOMPARE(raster.width, 40u);
    QCOMPARE(raster.height, 12u);
    QCOMPARE(raster.coverage.size(), std::size_t{480});
    QVERIFY(*std::max_element(raster.coverage.begin(), raster.coverage.end()) > 0.5f);
    QVERIFY(TextController::rasterize(TextSettings{}, 40, 12).coverage.empty());
}

void TextControllerTest::draggingPlacesABoxBetweenTheTwoCorners() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    TextController controller(&paintController);
    controller.setProject(&project);
    QSignalSpy spy(&controller, &TextController::previewChanged);

    controller.beginBox(layerId, pointAt(project, 50, 60));
    controller.continueBox(pointAt(project, 10, 40));
    controller.endBox();

    QVERIFY(controller.box().has_value());
    QCOMPARE(controller.box()->frameLow, 10);
    QCOMPARE(controller.box()->binLow, 40);
    QCOMPARE(controller.box()->width, 41);
    QCOMPARE(controller.box()->height, 21);
    QVERIFY(spy.count() >= 2);
    QVERIFY(controller.previewBounds().has_value());
}

void TextControllerTest::aClickPlacesABoxTheTextsOwnSizeCentredOnIt() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    TextController controller(&paintController);
    controller.setProject(&project);
    controller.setSettings(hello());
    const QImage glyphs = TextController::renderGlyphs(hello());

    controller.beginBox(layerId, pointAt(project, 100, 50));
    controller.endBox();

    QVERIFY(controller.box().has_value());
    QCOMPARE(controller.box()->width, glyphs.width());
    QCOMPARE(controller.box()->height, glyphs.height());
    QCOMPARE(controller.box()->frameLow, 100 - glyphs.width() / 2);
    QVERIFY(!controller.previewImage().isNull());
    QCOMPARE(controller.previewImage().size(), glyphs.size());
}

void TextControllerTest::applyWritesTheTextAsOneUndoStepAndClearsTheBox() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    UndoStack undoStack;
    PaintController paintController(&undoStack);
    paintController.setProject(&project);
    TextController controller(&paintController);
    controller.setProject(&project);
    controller.setSettings(hello());
    QSignalSpy contentSpy(&controller, &TextController::contentChanged);
    controller.beginBox(layerId, pointAt(project, 20, 20));
    controller.continueBox(pointAt(project, 80, 50));
    controller.endBox();

    const auto id = controller.apply();

    QVERIFY(id.has_value());
    QVERIFY(!controller.box().has_value());
    QCOMPARE(contentSpy.count(), 1);
    QCOMPARE(undoStack.count(), std::size_t{1});
    QVERIFY(dynamic_cast<const sound_mind::core::PasteOperation*>(&project.operationLog().at(0)) != nullptr);
    const auto& content = project.layerById(layerId)->content()->leftMagnitudeDb;
    QVERIFY(std::any_of(content.begin(), content.end(), [](float db) { return db > -10.0f; }));  // glyphs written...
    QCOMPARE(content.front(), -96.0f);  // ...nothing outside the box.

    undoStack.undo();
    QVERIFY(project.operationLog().activeOperationsTargeting(layerId).empty());
}

void TextControllerTest::applyDoesNothingWithoutABoxOrText() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    TextController controller(&paintController);
    controller.setProject(&project);
    controller.setSettings(hello());
    QVERIFY(!controller.apply().has_value());  // no box.

    controller.setSettings(TextSettings{});  // blank text.
    controller.beginBox(layerId, pointAt(project, 20, 20));
    controller.continueBox(pointAt(project, 80, 50));
    controller.endBox();
    QVERIFY(!controller.apply().has_value());
    QCOMPARE(project.operationLog().size(), std::size_t{0});
}

void TextControllerTest::resetDiscardsTheBox() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    TextController controller(&paintController);
    controller.setProject(&project);
    controller.beginBox(layerId, pointAt(project, 20, 20));
    controller.continueBox(pointAt(project, 80, 50));
    controller.endBox();

    controller.reset();

    QVERIFY(!controller.box().has_value());
    QVERIFY(!controller.previewBounds().has_value());
}

// v0.1.40.13 - applied text stays editable.
void TextControllerTest::appliedTextKeepsItsSettingsAndCanBeEditedWhenPicked() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    UndoStack undoStack;
    PaintController paintController(&undoStack);
    paintController.setProject(&project);
    TextController controller(&paintController);
    controller.setProject(&project);
    controller.setSettings(hello());
    controller.beginBox(layerId, pointAt(project, 20, 20));
    controller.continueBox(pointAt(project, 80, 50));
    controller.endBox();
    const auto id = controller.apply();
    QVERIFY(id.has_value());
    const auto& applied = dynamic_cast<const sound_mind::core::PasteOperation&>(project.operationLog().at(0));
    QVERIFY(applied.textSource().has_value());
    QCOMPARE(TextController::settingsFor(*applied.textSource(), applied.blendMode()), hello());
    const auto box = sound_mind::core::textPasteBox(
        applied, sound_mind::core::streamCodecConfigFor(project.settings()));

    sound_mind::studio::PickController pick(&paintController);
    pick.setProject(&project);
    QVERIFY(pick.selectOperation(layerId, *id));
    QVERIFY(pick.selectedTextSettings().has_value());
    QCOMPARE(*pick.selectedTextSettings(), hello());

    TextSettings edited = hello();
    edited.text = QStringLiteral("Bye");
    edited.bold = true;
    QVERIFY(pick.applyTextSettings(edited));
    QCOMPARE(undoStack.count(), std::size_t{2});
    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{1});  // the edit replaced the original.
    const auto& replaced = dynamic_cast<const sound_mind::core::PasteOperation&>(*active[0]);
    QCOMPARE(QString::fromStdString(replaced.textSource()->text), QStringLiteral("Bye"));
    QVERIFY(replaced.textSource()->bold);
    const auto newBox = sound_mind::core::textPasteBox(
        replaced, sound_mind::core::streamCodecConfigFor(project.settings()));
    QCOMPARE(newBox.frameLow, box.frameLow);  // the same box.
    QCOMPARE(newBox.binLow, box.binLow);
    QCOMPARE(newBox.width, box.width);
    QCOMPARE(newBox.height, box.height);
    QCOMPARE(*pick.selectedTextSettings(), edited);  // still Picked.

    QVERIFY(!pick.applyTextSettings(edited));  // nothing changed: no new step.
    TextSettings blank = edited;
    blank.text.clear();
    QVERIFY(!pick.applyTextSettings(blank));  // would write nothing: refused.
    QCOMPARE(undoStack.count(), std::size_t{2});

    undoStack.undo();  // back to "Hello".
    const auto restored = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(QString::fromStdString(
                 dynamic_cast<const sound_mind::core::PasteOperation&>(*restored[0]).textSource()->text),
             hello().text);
}

