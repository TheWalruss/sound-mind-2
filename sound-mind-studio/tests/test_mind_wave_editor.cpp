#include "test_mind_wave_editor.h"

#include <optional>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QSignalSpy>
#include <QSpinBox>
#include <QtTest/QtTest>

#include "sound_mind/core/path.h"
#include "sound_mind/studio/mind_wave_editor.h"

using sound_mind::core::EnvelopeShape;
using sound_mind::core::GeneratorType;
using sound_mind::core::MindWave;
using sound_mind::core::MindWaveAxis;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::PeriodicWaveform;
using sound_mind::core::SpatialPattern;
using sound_mind::core::SteppedNoiseShape;
using sound_mind::studio::MindWaveEditor;

void MindWaveEditorTest::freshEditorHasTheDefaultMindWave() {
    const MindWaveEditor editor;
    const MindWave defaultWave;
    QCOMPARE(editor.mindWave().type(), defaultWave.type());
    QCOMPARE(editor.mindWave().periodicWaveform(), defaultWave.periodicWaveform());
    QCOMPARE(editor.mindWave().period(), defaultWave.period());
}

void MindWaveEditorTest::freshEditorShowsOnlyThePeriodicGroup() {
    const MindWaveEditor editor;
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("periodicGroup"))->isHidden());
    QVERIFY(editor.findChild<QGroupBox*>(QStringLiteral("envelopeGroup"))->isHidden());
    QVERIFY(editor.findChild<QGroupBox*>(QStringLiteral("steppedNoiseGroup"))->isHidden());
    QVERIFY(editor.findChild<QGroupBox*>(QStringLiteral("spatialGroup"))->isHidden());
    QVERIFY(editor.findChild<QGroupBox*>(QStringLiteral("fractalGroup"))->isHidden());
    QVERIFY(editor.findChild<QGroupBox*>(QStringLiteral("drawnGroup"))->isHidden());
}

void MindWaveEditorTest::changingGeneratorTypeShowsOnlyThatTypesOwnGroupAndEmits() {
    MindWaveEditor editor;
    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    QVERIFY(combo != nullptr);
    std::optional<MindWave> received;
    connect(&editor, &MindWaveEditor::mindWaveChanged, [&](const MindWave& wave) { received = wave; });

    combo->setCurrentIndex(combo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::Fractal))));

    QVERIFY(received.has_value());
    QCOMPARE(received->type(), GeneratorType::Fractal);
    QCOMPARE(editor.mindWave().type(), GeneratorType::Fractal);
    QVERIFY(editor.findChild<QGroupBox*>(QStringLiteral("fractalGroup")) != nullptr);
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("fractalGroup"))->isHidden());
    QVERIFY(editor.findChild<QGroupBox*>(QStringLiteral("periodicGroup"))->isHidden());
}

void MindWaveEditorTest::spatialHidesTheAxisCombo() {
    MindWaveEditor editor;
    auto* generatorCombo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    auto* axisCombo = editor.findChild<QComboBox*>(QStringLiteral("axisCombo"));
    QVERIFY(!axisCombo->isHidden());

    generatorCombo->setCurrentIndex(generatorCombo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::Spatial))));

    QVERIFY(axisCombo->isHidden());
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("spatialGroup"))->isHidden());
}

void MindWaveEditorTest::changingPeriodPhaseSeedNoiseFieldsUpdateAndEmit() {
    MindWaveEditor editor;
    QSignalSpy spy(&editor, &MindWaveEditor::mindWaveChanged);

    editor.findChild<QDoubleSpinBox*>(QStringLiteral("periodSpinBox"))->setValue(3.5);
    editor.findChild<QDoubleSpinBox*>(QStringLiteral("phaseSpinBox"))->setValue(1.25);
    editor.findChild<QSpinBox*>(QStringLiteral("seedSpinBox"))->setValue(42);
    editor.findChild<QDoubleSpinBox*>(QStringLiteral("noiseScaleSpinBox"))->setValue(2.0);
    editor.findChild<QSpinBox*>(QStringLiteral("noiseOctavesSpinBox"))->setValue(5);
    editor.findChild<QDoubleSpinBox*>(QStringLiteral("noisePersistenceSpinBox"))->setValue(0.3);
    auto* axisCombo = editor.findChild<QComboBox*>(QStringLiteral("axisCombo"));
    axisCombo->setCurrentIndex(axisCombo->findData(QVariant::fromValue(static_cast<int>(MindWaveAxis::Frequency))));

    QCOMPARE(spy.count(), 7);
    QCOMPARE(editor.mindWave().period(), 3.5);
    QCOMPARE(editor.mindWave().phaseRadians(), 1.25);
    QCOMPARE(editor.mindWave().seed(), std::uint32_t{42});
    QCOMPARE(editor.mindWave().noiseScale(), 2.0);
    QCOMPARE(editor.mindWave().noiseOctaves(), 5);
    QCOMPARE(editor.mindWave().noisePersistence(), 0.3);
    QCOMPARE(editor.mindWave().axis(), MindWaveAxis::Frequency);
}

void MindWaveEditorTest::changingPeriodicWaveformAndDutyCycleUpdateAndEmit() {
    MindWaveEditor editor;
    auto* waveformCombo = editor.findChild<QComboBox*>(QStringLiteral("periodicWaveformCombo"));
    waveformCombo->setCurrentIndex(waveformCombo->findData(QVariant::fromValue(static_cast<int>(PeriodicWaveform::Pulse))));
    editor.findChild<QDoubleSpinBox*>(QStringLiteral("dutyCycleSpinBox"))->setValue(0.25);

    QCOMPARE(editor.mindWave().periodicWaveform(), PeriodicWaveform::Pulse);
    QCOMPARE(editor.mindWave().dutyCycle(), 0.25);
}

void MindWaveEditorTest::changingEnvelopeFieldsUpdateAndEmit() {
    MindWaveEditor editor;
    auto* generatorCombo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    generatorCombo->setCurrentIndex(generatorCombo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::Envelope))));

    auto* shapeCombo = editor.findChild<QComboBox*>(QStringLiteral("envelopeShapeCombo"));
    shapeCombo->setCurrentIndex(shapeCombo->findData(QVariant::fromValue(static_cast<int>(EnvelopeShape::SCurve))));
    editor.findChild<QDoubleSpinBox*>(QStringLiteral("envelopeCenterSpinBox"))->setValue(2.0);
    editor.findChild<QDoubleSpinBox*>(QStringLiteral("envelopeSteepnessSpinBox"))->setValue(4.0);
    editor.findChild<QDoubleSpinBox*>(QStringLiteral("decayRateSpinBox"))->setValue(1.5);

    QCOMPARE(editor.mindWave().type(), GeneratorType::Envelope);
    QCOMPARE(editor.mindWave().envelopeShape(), EnvelopeShape::SCurve);
    QCOMPARE(editor.mindWave().envelopeCenter(), 2.0);
    QCOMPARE(editor.mindWave().envelopeSteepness(), 4.0);
    QCOMPARE(editor.mindWave().decayRate(), 1.5);
}

void MindWaveEditorTest::changingSteppedNoiseFieldsUpdateAndEmit() {
    MindWaveEditor editor;
    auto* generatorCombo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    generatorCombo->setCurrentIndex(
        generatorCombo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::SteppedNoise))));

    auto* shapeCombo = editor.findChild<QComboBox*>(QStringLiteral("steppedNoiseShapeCombo"));
    shapeCombo->setCurrentIndex(shapeCombo->findData(QVariant::fromValue(static_cast<int>(SteppedNoiseShape::FractalNoise))));
    editor.findChild<QSpinBox*>(QStringLiteral("stepCountSpinBox"))->setValue(8);

    QCOMPARE(editor.mindWave().type(), GeneratorType::SteppedNoise);
    QCOMPARE(editor.mindWave().steppedNoiseShape(), SteppedNoiseShape::FractalNoise);
    QCOMPARE(editor.mindWave().stepCount(), 8);
}

void MindWaveEditorTest::changingSpatialFieldsUpdateAndEmit() {
    MindWaveEditor editor;
    auto* generatorCombo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    generatorCombo->setCurrentIndex(generatorCombo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::Spatial))));

    auto* patternCombo = editor.findChild<QComboBox*>(QStringLiteral("spatialPatternCombo"));
    patternCombo->setCurrentIndex(patternCombo->findData(QVariant::fromValue(static_cast<int>(SpatialPattern::Cellular))));
    editor.findChild<QDoubleSpinBox*>(QStringLiteral("spatialCenterXSpinBox"))->setValue(1.5);
    editor.findChild<QDoubleSpinBox*>(QStringLiteral("spatialCenterYSpinBox"))->setValue(2.5);
    editor.findChild<QDoubleSpinBox*>(QStringLiteral("domainWarpStrengthSpinBox"))->setValue(3.0);

    QCOMPARE(editor.mindWave().type(), GeneratorType::Spatial);
    QCOMPARE(editor.mindWave().spatialPattern(), SpatialPattern::Cellular);
    QCOMPARE(editor.mindWave().spatialCenterX(), 1.5);
    QCOMPARE(editor.mindWave().spatialCenterY(), 2.5);
    QCOMPARE(editor.mindWave().domainWarpStrength(), 3.0);
}

void MindWaveEditorTest::changingFractalFieldsUpdateAndEmit() {
    MindWaveEditor editor;
    auto* generatorCombo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    generatorCombo->setCurrentIndex(generatorCombo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::Fractal))));

    editor.findChild<QDoubleSpinBox*>(QStringLiteral("fractalRoughnessSpinBox"))->setValue(0.6);
    editor.findChild<QSpinBox*>(QStringLiteral("fractalIterationsSpinBox"))->setValue(10);

    QCOMPARE(editor.mindWave().fractalRoughness(), 0.6);
    QCOMPARE(editor.mindWave().fractalIterations(), 10);
}

void MindWaveEditorTest::setMindWaveSyncsEveryControlWithoutEmitting() {
    MindWaveEditor editor;
    MindWave wave;
    wave.setType(GeneratorType::Fractal);
    wave.setSeed(99);
    wave.setFractalRoughness(0.7);
    wave.setFractalIterations(6);
    QSignalSpy spy(&editor, &MindWaveEditor::mindWaveChanged);

    editor.setMindWave(wave);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(editor.mindWave().type(), GeneratorType::Fractal);
    QCOMPARE(editor.findChild<QSpinBox*>(QStringLiteral("seedSpinBox"))->value(), 99);
    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("fractalRoughnessSpinBox"))->value(), 0.7);
    QCOMPARE(editor.findChild<QSpinBox*>(QStringLiteral("fractalIterationsSpinBox"))->value(), 6);
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("fractalGroup"))->isHidden());
}

void MindWaveEditorTest::setMindWavePreservesSuperpositionStackAndBlendMode() {
    MindWaveEditor editor;
    MindWave member;
    member.setPeriod(2.0);
    MindWave wave;
    wave.setSuperpositionStack({member});
    wave.setSuperpositionBlendMode(sound_mind::core::SuperpositionBlendMode::Max);

    editor.setMindWave(wave);

    // No controls edit the stack/blend mode - mindWave() should still
    // round-trip them unchanged, since MindWavesPanel relies on this to
    // avoid needing to re-merge them itself after every top-level edit.
    QCOMPARE(editor.mindWave().superpositionStack().size(), std::size_t{1});
    QCOMPARE(editor.mindWave().superpositionBlendMode(), sound_mind::core::SuperpositionBlendMode::Max);
}

void MindWaveEditorTest::switchingToDrawnShowsOnlyItsOwnGroup() {
    MindWaveEditor editor;
    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));

    combo->setCurrentIndex(combo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::Drawn))));

    QCOMPARE(editor.mindWave().type(), GeneratorType::Drawn);
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("drawnGroup"))->isHidden());
    QVERIFY(editor.findChild<QGroupBox*>(QStringLiteral("periodicGroup"))->isHidden());
}

void MindWaveEditorTest::aFreshlyLoadedDrawnMindWaveWithNoPathShowsTheUncapturedStatus() {
    MindWaveEditor editor;
    MindWave wave;
    wave.setType(GeneratorType::Drawn);

    editor.setMindWave(wave);

    auto* label = editor.findChild<QLabel*>(QStringLiteral("drawnStatusLabel"));
    QVERIFY(label != nullptr);
    QVERIFY(label->text().contains(QStringLiteral("No shape captured")));
}

void MindWaveEditorTest::aLoadedDrawnMindWaveWithACapturedPathShowsTheNodeCountAndIsPreserved() {
    MindWaveEditor editor;
    Path path;
    PathNode nodeA;
    nodeA.anchor = sound_mind::core::TimeFrequencyPoint{0.0, 100.0};
    nodeA.type = PathNodeType::Corner;
    path.addNode(nodeA);
    PathNode nodeB;
    nodeB.anchor = sound_mind::core::TimeFrequencyPoint{2.0, 300.0};
    nodeB.type = PathNodeType::Corner;
    path.addNode(nodeB);
    MindWave wave;
    wave.setType(GeneratorType::Drawn);
    wave.setDrawnPath(path);

    editor.setMindWave(wave);

    auto* label = editor.findChild<QLabel*>(QStringLiteral("drawnStatusLabel"));
    QVERIFY(label->text().contains(QStringLiteral("2")));
    // No controls edit drawnPath() here - mindWave() should still round-trip
    // it unchanged, the same "preserved but not editable here" contract
    // setMindWavePreservesSuperpositionStackAndBlendMode() already
    // establishes for the superposition stack/blend mode.
    QCOMPARE(editor.mindWave().drawnPath().nodes().size(), std::size_t{2});
}

void MindWaveEditorTest::switchingToStepGridShowsOnlyItsOwnGroupWithTheDefaultFourSteps() {
    MindWaveEditor editor;
    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));

    combo->setCurrentIndex(combo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::StepGrid))));

    QCOMPARE(editor.mindWave().type(), GeneratorType::StepGrid);
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("stepGridGroup"))->isHidden());
    QVERIFY(editor.findChild<QGroupBox*>(QStringLiteral("periodicGroup"))->isHidden());
    QCOMPARE(editor.findChild<QSpinBox*>(QStringLiteral("stepGridCountSpinBox"))->value(), 4);
    QVERIFY(editor.findChild<QDoubleSpinBox*>(QStringLiteral("stepGridValueSpinBox4")) != nullptr);
}

void MindWaveEditorTest::changingStepGridCountResizesTheValueRows() {
    MindWaveEditor editor;
    auto* generatorCombo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    generatorCombo->setCurrentIndex(generatorCombo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::StepGrid))));
    auto* countSpinBox = editor.findChild<QSpinBox*>(QStringLiteral("stepGridCountSpinBox"));

    countSpinBox->setValue(2);
    QVERIFY(editor.findChild<QDoubleSpinBox*>(QStringLiteral("stepGridValueSpinBox3")) == nullptr);
    QCOMPARE(editor.mindWave().stepGridValues().size(), std::size_t{2});

    countSpinBox->setValue(5);
    QVERIFY(editor.findChild<QDoubleSpinBox*>(QStringLiteral("stepGridValueSpinBox5")) != nullptr);
    QCOMPARE(editor.mindWave().stepGridValues().size(), std::size_t{5});
}

void MindWaveEditorTest::changingAStepGridValueEmitsWithTheUpdatedValues() {
    MindWaveEditor editor;
    auto* generatorCombo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    generatorCombo->setCurrentIndex(generatorCombo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::StepGrid))));
    std::optional<MindWave> received;
    connect(&editor, &MindWaveEditor::mindWaveChanged, [&](const MindWave& wave) { received = wave; });

    editor.findChild<QDoubleSpinBox*>(QStringLiteral("stepGridValueSpinBox2"))->setValue(0.1);

    QVERIFY(received.has_value());
    QCOMPARE(received->stepGridValues()[1], 0.1);
}

void MindWaveEditorTest::loadingAStepGridMindWaveSyncsTheCountAndEachStepsOwnValue() {
    MindWaveEditor editor;
    MindWave wave;
    wave.setType(GeneratorType::StepGrid);
    wave.setStepGridValues({0.2, 0.4, 0.6});

    editor.setMindWave(wave);

    QCOMPARE(editor.findChild<QSpinBox*>(QStringLiteral("stepGridCountSpinBox"))->value(), 3);
    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("stepGridValueSpinBox1"))->value(), 0.2);
    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("stepGridValueSpinBox2"))->value(), 0.4);
    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("stepGridValueSpinBox3"))->value(), 0.6);
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("stepGridGroup"))->isHidden());
}

void MindWaveEditorTest::switchingToContinuousShowsOnlyItsOwnGroupWithTheDefaultKnobPositions() {
    MindWaveEditor editor;
    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));

    combo->setCurrentIndex(combo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::Continuous))));

    QCOMPARE(editor.mindWave().type(), GeneratorType::Continuous);
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("continuousGroup"))->isHidden());
    QVERIFY(editor.findChild<QGroupBox*>(QStringLiteral("periodicGroup"))->isHidden());
    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("continuousShapeSpinBox"))->value(), 0.0);
    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("continuousSkewSpinBox"))->value(), 0.5);
    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("continuousCharacterSpinBox"))->value(), 0.0);
}

void MindWaveEditorTest::changingShapeSkewOrCharacterUpdatesAndEmits() {
    MindWaveEditor editor;
    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    combo->setCurrentIndex(combo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::Continuous))));
    std::optional<MindWave> received;
    connect(&editor, &MindWaveEditor::mindWaveChanged, [&](const MindWave& wave) { received = wave; });

    editor.findChild<QDoubleSpinBox*>(QStringLiteral("continuousShapeSpinBox"))->setValue(0.7);
    QVERIFY(received.has_value());
    QCOMPARE(received->continuousShape(), 0.7);

    editor.findChild<QDoubleSpinBox*>(QStringLiteral("continuousSkewSpinBox"))->setValue(0.2);
    QCOMPARE(received->continuousSkew(), 0.2);

    editor.findChild<QDoubleSpinBox*>(QStringLiteral("continuousCharacterSpinBox"))->setValue(0.9);
    QCOMPARE(received->continuousCharacter(), 0.9);
}

void MindWaveEditorTest::loadingAContinuousMindWaveSyncsAllThreeKnobs() {
    MindWaveEditor editor;
    MindWave wave;
    wave.setType(GeneratorType::Continuous);
    wave.setContinuousShape(0.4);
    wave.setContinuousSkew(0.1);
    wave.setContinuousCharacter(0.8);

    editor.setMindWave(wave);

    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("continuousShapeSpinBox"))->value(), 0.4);
    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("continuousSkewSpinBox"))->value(), 0.1);
    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("continuousCharacterSpinBox"))->value(), 0.8);
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("continuousGroup"))->isHidden());
}
