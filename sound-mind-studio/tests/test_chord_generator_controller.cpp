#include "test_chord_generator_controller.h"

#include <algorithm>
#include <memory>

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/chord_generator.h"
#include "sound_mind/core/music_theory.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/sequence_operation.h"
#include "sound_mind/core/tool_configuration.h"
#include "sound_mind/studio/chord_generator_controller.h"
#include "sound_mind/studio/paint_controller.h"

using sound_mind::core::ChordCategory;
using sound_mind::core::ChordGeneratorParams;
using sound_mind::core::ChordPlaybackMode;
using sound_mind::core::frequencyForMidiNote;
using sound_mind::core::InstrumentConfiguration;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::SequenceOperation;
using sound_mind::studio::ChordGeneratorController;
using sound_mind::studio::PaintController;

namespace {

ProjectSettings testSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 100;
    settings.canvasHeight = 50;
    settings.binCount = 50;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 2020.0f;
    settings.timestepMs = 10.0;
    return settings;
}

LayerId addBlankNormalLayer(Project& project) {
    Layer layer(0, "Test Layer", LayerType::Normal);
    sound_mind::codec::StreamImage content;
    content.config = sound_mind::core::streamCodecConfigFor(project.settings());
    content.frameCount = project.settings().canvasWidth;
    const std::size_t pixelCount = std::size_t{content.config.binCount} * content.frameCount;
    content.leftMagnitudeDb.assign(pixelCount, 0.0f);
    content.rightMagnitudeDb.assign(pixelCount, 0.0f);
    content.sharedPhaseRadians.assign(pixelCount, 0.0f);
    layer.setContent(content);
    return project.addLayer(std::move(layer));
}

}  // namespace

void ChordGeneratorControllerTest::defaultParamsPreviewACMajorTriad() {
    PaintController paintController;
    ChordGeneratorController controller(&paintController);

    const auto frequenciesHz = controller.previewFrequenciesHz();

    QCOMPARE(frequenciesHz.size(), std::size_t{3});
    QCOMPARE(frequenciesHz[0], frequencyForMidiNote(60, 440.0));  // C4
    QCOMPARE(frequenciesHz[1], frequencyForMidiNote(64, 440.0));  // E4
    QCOMPARE(frequenciesHz[2], frequencyForMidiNote(67, 440.0));  // G4
}

void ChordGeneratorControllerTest::setParamsEmitsPreviewChanged() {
    PaintController paintController;
    ChordGeneratorController controller(&paintController);
    QSignalSpy spy(&controller, &ChordGeneratorController::previewChanged);

    ChordGeneratorParams params;
    params.rootMidiNote = 69;
    controller.setParams(params);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.params().rootMidiNote, 69);
}

void ChordGeneratorControllerTest::previewFrequenciesHzIsAscendingAndDeduplicated() {
    PaintController paintController;
    ChordGeneratorController controller(&paintController);

    ChordGeneratorParams params;
    params.rootMidiNote = 60;
    params.category = ChordCategory::Triads;
    params.chordIndex = 6;  // Power: 0, 7 - only 2 distinct pitches.
    params.mode = ChordPlaybackMode::Arpeggio;
    params.repeats = 3;  // repeats the same 2 pitches 3 times over.
    controller.setParams(params);

    const auto frequenciesHz = controller.previewFrequenciesHz();

    QCOMPARE(frequenciesHz.size(), std::size_t{2});
    QVERIFY(std::is_sorted(frequenciesHz.begin(), frequenciesHz.end()));
}

void ChordGeneratorControllerTest::stampAtDoesNothingWithNoProjectSet() {
    PaintController paintController;
    ChordGeneratorController controller(&paintController);
    QSignalSpy spy(&controller, &ChordGeneratorController::contentChanged);

    controller.stampAt(0, 1.0);

    QCOMPARE(spy.count(), 0);
}

void ChordGeneratorControllerTest::stampAtDoesNothingForAnOutOfRangeChordIndex() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    ChordGeneratorController controller(&paintController);
    controller.setProject(&project);
    ChordGeneratorParams params;
    params.chordIndex = 999;
    controller.setParams(params);
    QSignalSpy spy(&controller, &ChordGeneratorController::contentChanged);

    controller.stampAt(layerId, 1.0);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(project.operationLog().activeOperationsTargeting(layerId).size(), std::size_t{0});
}

void ChordGeneratorControllerTest::stampAtAppendsASequenceOperationAndEmitsContentChanged() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    ChordGeneratorController controller(&paintController);
    controller.setProject(&project);
    QSignalSpy spy(&controller, &ChordGeneratorController::contentChanged);

    controller.stampAt(layerId, 2.5);

    QCOMPARE(spy.count(), 1);
    const auto operations = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(operations.size(), std::size_t{1});
    const auto* sequence = dynamic_cast<const SequenceOperation*>(operations.front());
    QVERIFY(sequence != nullptr);
    QCOMPARE(sequence->notes().size(), std::size_t{3});
}

void ChordGeneratorControllerTest::stampAtClonesThePaintControllersCurrentToolConfiguration() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    auto instrument = std::make_unique<InstrumentConfiguration>();
    instrument->setInharmonicity(0.02);
    paintController.setToolConfiguration(std::move(instrument));
    ChordGeneratorController controller(&paintController);
    controller.setProject(&project);

    controller.stampAt(layerId, 0.0);

    const auto operations = project.operationLog().activeOperationsTargeting(layerId);
    const auto* sequence = dynamic_cast<const SequenceOperation*>(operations.front());
    QVERIFY(sequence != nullptr);
    const auto* config = dynamic_cast<const InstrumentConfiguration*>(&sequence->config());
    QVERIFY(config != nullptr);
    QCOMPARE(config->inharmonicity(), 0.02);
}

void ChordGeneratorControllerTest::stampAtResolvesNotesAtTheGivenStartTime() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    ChordGeneratorController controller(&paintController);
    controller.setProject(&project);

    controller.stampAt(layerId, 3.0);

    const auto operations = project.operationLog().activeOperationsTargeting(layerId);
    const auto* sequence = dynamic_cast<const SequenceOperation*>(operations.front());
    QVERIFY(sequence != nullptr);
    for (const auto& note : sequence->notes()) {
        QCOMPARE(note.startTimeSeconds, 3.0);
    }
}

void ChordGeneratorControllerTest::setNotationSwitchesTheCurrentSourceAndEmitsPreviewChanged() {
    PaintController paintController;
    ChordGeneratorController controller(&paintController);
    QSignalSpy spy(&controller, &ChordGeneratorController::previewChanged);

    controller.setNotation("A4:0.5 C5:0.5", 440.0, 120.0);

    QCOMPARE(spy.count(), 1);
    const auto frequenciesHz = controller.previewFrequenciesHz();
    QCOMPARE(frequenciesHz.size(), std::size_t{2});
}

void ChordGeneratorControllerTest::previewFrequenciesHzReturnsEmptyForInvalidNotation() {
    PaintController paintController;
    ChordGeneratorController controller(&paintController);

    controller.setNotation("not valid notation :::", 440.0, 120.0);

    QVERIFY(controller.previewFrequenciesHz().empty());
}

void ChordGeneratorControllerTest::stampAtDoesNothingForInvalidNotation() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    ChordGeneratorController controller(&paintController);
    controller.setProject(&project);
    controller.setNotation("garbage", 440.0, 120.0);
    QSignalSpy spy(&controller, &ChordGeneratorController::contentChanged);

    controller.stampAt(layerId, 0.0);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(project.operationLog().activeOperationsTargeting(layerId).size(), std::size_t{0});
}

void ChordGeneratorControllerTest::stampAtAppendsASequenceOperationFromValidNotation() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    ChordGeneratorController controller(&paintController);
    controller.setProject(&project);
    controller.setNotation("A4:0.5 z0.25 C5:0.5", 440.0, 120.0);

    controller.stampAt(layerId, 2.0);

    const auto operations = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(operations.size(), std::size_t{1});
    const auto* sequence = dynamic_cast<const SequenceOperation*>(operations.front());
    QVERIFY(sequence != nullptr);
    QCOMPARE(sequence->notes().size(), std::size_t{2});
    QCOMPARE(sequence->notes()[0].startTimeSeconds, 2.0);
    QCOMPARE(sequence->notes()[1].startTimeSeconds, 2.75);
}

void ChordGeneratorControllerTest::settingParamsAfterNotationSwitchesBackToTheChordBuilder() {
    PaintController paintController;
    ChordGeneratorController controller(&paintController);
    controller.setNotation("A4:0.5 C5:0.5", 440.0, 120.0);
    QCOMPARE(controller.previewFrequenciesHz().size(), std::size_t{2});

    ChordGeneratorParams params;
    controller.setParams(params);  // Triads/Major default - 3 distinct notes.

    QCOMPARE(controller.previewFrequenciesHz().size(), std::size_t{3});
}
