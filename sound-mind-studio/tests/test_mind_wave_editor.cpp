#include "test_mind_wave_editor.h"

#include <optional>

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/path.h"
#include "sound_mind/core/resonance_profile.h"
#include "sound_mind/studio/mind_wave_editor.h"
#include "sound_mind/studio/numeric_field.h"
#include "sound_mind/studio/rotary_dial_widget.h"
#include "sound_mind/studio/waveform_preview_widget.h"

using sound_mind::core::EnvelopeShape;
using sound_mind::core::GeneratorType;
using sound_mind::core::MindWave;
using sound_mind::core::MindWaveAxis;
using sound_mind::core::NamedResonanceProfile;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::PeriodicWaveform;
using sound_mind::core::SpatialPattern;
using sound_mind::core::SteppedNoiseShape;
using sound_mind::studio::MindWaveEditor;
using sound_mind::studio::RotaryDialWidget;
using sound_mind::studio::WaveformPreviewWidget;
using sound_mind::studio::NumericField;
using sound_mind::studio::IntegerField;

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

    editor.findChild<NumericField*>(QStringLiteral("periodSpinBox"))->setValue(3.5);
    editor.findChild<NumericField*>(QStringLiteral("phaseSpinBox"))->setValue(1.25);
    editor.findChild<IntegerField*>(QStringLiteral("seedSpinBox"))->setValue(42);
    editor.findChild<NumericField*>(QStringLiteral("noiseScaleSpinBox"))->setValue(2.0);
    editor.findChild<IntegerField*>(QStringLiteral("noiseOctavesSpinBox"))->setValue(5);
    editor.findChild<NumericField*>(QStringLiteral("noisePersistenceSpinBox"))->setValue(0.3);
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
    editor.findChild<NumericField*>(QStringLiteral("dutyCycleSpinBox"))->setValue(0.25);

    QCOMPARE(editor.mindWave().periodicWaveform(), PeriodicWaveform::Pulse);
    QCOMPARE(editor.mindWave().dutyCycle(), 0.25);
}

void MindWaveEditorTest::reversedAndInvertedCheckBoxesUpdateTheWaveEmitAndRoundTripThroughSetMindWave() {
    MindWaveEditor editor;
    auto* reversedBox = editor.findChild<QCheckBox*>(QStringLiteral("reversedCheckBox"));
    auto* invertedBox = editor.findChild<QCheckBox*>(QStringLiteral("invertedCheckBox"));
    QVERIFY(reversedBox != nullptr);
    QVERIFY(invertedBox != nullptr);
    QVERIFY(!reversedBox->isChecked());
    QVERIFY(!invertedBox->isChecked());
    QSignalSpy spy(&editor, &MindWaveEditor::mindWaveChanged);

    reversedBox->setChecked(true);
    invertedBox->setChecked(true);

    QVERIFY(editor.mindWave().reversed());
    QVERIFY(editor.mindWave().inverted());
    QCOMPARE(spy.count(), 2);

    MindWaveEditor other;
    other.setMindWave(editor.mindWave());
    QVERIFY(other.findChild<QCheckBox*>(QStringLiteral("reversedCheckBox"))->isChecked());
    QVERIFY(other.findChild<QCheckBox*>(QStringLiteral("invertedCheckBox"))->isChecked());
    other.setMindWave(sound_mind::core::MindWave{});
    QVERIFY(!other.findChild<QCheckBox*>(QStringLiteral("reversedCheckBox"))->isChecked());
}

void MindWaveEditorTest::theNewEnvelopeAndPeriodicShapesAreSelectable() {
    MindWaveEditor editor;
    auto* waveformCombo = editor.findChild<QComboBox*>(QStringLiteral("periodicWaveformCombo"));
    waveformCombo->setCurrentIndex(
        waveformCombo->findData(QVariant::fromValue(static_cast<int>(PeriodicWaveform::RectifiedSine))));
    QCOMPARE(editor.mindWave().periodicWaveform(), PeriodicWaveform::RectifiedSine);

    auto* shapeCombo = editor.findChild<QComboBox*>(QStringLiteral("envelopeShapeCombo"));
    for (const EnvelopeShape shape : {EnvelopeShape::Peak, EnvelopeShape::Ramp}) {
        const int index = shapeCombo->findData(QVariant::fromValue(static_cast<int>(shape)));
        QVERIFY(index >= 0);
        shapeCombo->setCurrentIndex(index);
        QCOMPARE(editor.mindWave().envelopeShape(), shape);
    }
}

void MindWaveEditorTest::changingEnvelopeFieldsUpdateAndEmit() {
    MindWaveEditor editor;
    auto* generatorCombo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    generatorCombo->setCurrentIndex(generatorCombo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::Envelope))));

    auto* shapeCombo = editor.findChild<QComboBox*>(QStringLiteral("envelopeShapeCombo"));
    shapeCombo->setCurrentIndex(shapeCombo->findData(QVariant::fromValue(static_cast<int>(EnvelopeShape::SCurve))));
    editor.findChild<NumericField*>(QStringLiteral("envelopeCenterSpinBox"))->setValue(2.0);
    editor.findChild<NumericField*>(QStringLiteral("envelopeSteepnessSpinBox"))->setValue(4.0);
    editor.findChild<NumericField*>(QStringLiteral("decayRateSpinBox"))->setValue(1.5);

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
    editor.findChild<IntegerField*>(QStringLiteral("stepCountSpinBox"))->setValue(8);

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
    editor.findChild<NumericField*>(QStringLiteral("spatialCenterXSpinBox"))->setValue(1.5);
    editor.findChild<NumericField*>(QStringLiteral("spatialCenterYSpinBox"))->setValue(2.5);
    editor.findChild<NumericField*>(QStringLiteral("domainWarpStrengthSpinBox"))->setValue(3.0);

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

    editor.findChild<NumericField*>(QStringLiteral("fractalRoughnessSpinBox"))->setValue(0.6);
    editor.findChild<IntegerField*>(QStringLiteral("fractalIterationsSpinBox"))->setValue(10);

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
    QCOMPARE(editor.findChild<IntegerField*>(QStringLiteral("seedSpinBox"))->value(), 99);
    QCOMPARE(editor.findChild<NumericField*>(QStringLiteral("fractalRoughnessSpinBox"))->value(), 0.7);
    QCOMPARE(editor.findChild<IntegerField*>(QStringLiteral("fractalIterationsSpinBox"))->value(), 6);
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("fractalGroup"))->isHidden());
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
    // it unchanged ("preserved but not editable here").
    QCOMPARE(editor.mindWave().drawnPath().nodes().size(), std::size_t{2});
}

void MindWaveEditorTest::switchingToStepGridShowsOnlyItsOwnGroupWithTheDefaultFourSteps() {
    MindWaveEditor editor;
    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));

    combo->setCurrentIndex(combo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::StepGrid))));

    QCOMPARE(editor.mindWave().type(), GeneratorType::StepGrid);
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("stepGridGroup"))->isHidden());
    QVERIFY(editor.findChild<QGroupBox*>(QStringLiteral("periodicGroup"))->isHidden());
    QCOMPARE(editor.findChild<IntegerField*>(QStringLiteral("stepGridCountSpinBox"))->value(), 4);
    QVERIFY(editor.findChild<NumericField*>(QStringLiteral("stepGridValueSpinBox4")) != nullptr);
}

void MindWaveEditorTest::changingStepGridCountResizesTheValueRows() {
    MindWaveEditor editor;
    auto* generatorCombo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    generatorCombo->setCurrentIndex(generatorCombo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::StepGrid))));
    auto* countSpinBox = editor.findChild<IntegerField*>(QStringLiteral("stepGridCountSpinBox"));

    countSpinBox->setValue(2);
    QVERIFY(editor.findChild<NumericField*>(QStringLiteral("stepGridValueSpinBox3")) == nullptr);
    QCOMPARE(editor.mindWave().stepGridValues().size(), std::size_t{2});

    countSpinBox->setValue(5);
    QVERIFY(editor.findChild<NumericField*>(QStringLiteral("stepGridValueSpinBox5")) != nullptr);
    QCOMPARE(editor.mindWave().stepGridValues().size(), std::size_t{5});
}

void MindWaveEditorTest::changingAStepGridValueEmitsWithTheUpdatedValues() {
    MindWaveEditor editor;
    auto* generatorCombo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    generatorCombo->setCurrentIndex(generatorCombo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::StepGrid))));
    std::optional<MindWave> received;
    connect(&editor, &MindWaveEditor::mindWaveChanged, [&](const MindWave& wave) { received = wave; });

    editor.findChild<NumericField*>(QStringLiteral("stepGridValueSpinBox2"))->setValue(0.1);

    QVERIFY(received.has_value());
    QCOMPARE(received->stepGridValues()[1], 0.1);
}

void MindWaveEditorTest::loadingAStepGridMindWaveSyncsTheCountAndEachStepsOwnValue() {
    MindWaveEditor editor;
    MindWave wave;
    wave.setType(GeneratorType::StepGrid);
    wave.setStepGridValues({0.2, 0.4, 0.6});

    editor.setMindWave(wave);

    QCOMPARE(editor.findChild<IntegerField*>(QStringLiteral("stepGridCountSpinBox"))->value(), 3);
    QCOMPARE(editor.findChild<NumericField*>(QStringLiteral("stepGridValueSpinBox1"))->value(), 0.2);
    QCOMPARE(editor.findChild<NumericField*>(QStringLiteral("stepGridValueSpinBox2"))->value(), 0.4);
    QCOMPARE(editor.findChild<NumericField*>(QStringLiteral("stepGridValueSpinBox3"))->value(), 0.6);
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("stepGridGroup"))->isHidden());
}

void MindWaveEditorTest::switchingToContinuousShowsOnlyItsOwnGroupWithTheDefaultKnobPositions() {
    MindWaveEditor editor;
    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));

    combo->setCurrentIndex(combo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::Continuous))));

    QCOMPARE(editor.mindWave().type(), GeneratorType::Continuous);
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("continuousGroup"))->isHidden());
    QVERIFY(editor.findChild<QGroupBox*>(QStringLiteral("periodicGroup"))->isHidden());
    QCOMPARE(editor.findChild<NumericField*>(QStringLiteral("continuousShapeSpinBox"))->value(), 0.0);
    QCOMPARE(editor.findChild<NumericField*>(QStringLiteral("continuousSkewSpinBox"))->value(), 0.5);
    QCOMPARE(editor.findChild<NumericField*>(QStringLiteral("continuousCharacterSpinBox"))->value(), 0.0);
    // v0.Y.58.1's own MindWave UI/UX uplift - a real RotaryDialWidget next
    // to each spin box, kept in sync, plus a live WaveformPreviewWidget
    // above all three - see MindWaveEditor's own docs.
    QCOMPARE(editor.findChild<RotaryDialWidget*>(QStringLiteral("continuousShapeDial"))->value(), 0.0);
    QCOMPARE(editor.findChild<RotaryDialWidget*>(QStringLiteral("continuousSkewDial"))->value(), 0.5);
    QCOMPARE(editor.findChild<RotaryDialWidget*>(QStringLiteral("continuousCharacterDial"))->value(), 0.0);
    QVERIFY(editor.findChild<WaveformPreviewWidget*>(QStringLiteral("continuousPreviewWidget"))->samples().size() > 0);
}

void MindWaveEditorTest::changingShapeSkewOrCharacterUpdatesAndEmits() {
    MindWaveEditor editor;
    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    combo->setCurrentIndex(combo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::Continuous))));
    std::optional<MindWave> received;
    connect(&editor, &MindWaveEditor::mindWaveChanged, [&](const MindWave& wave) { received = wave; });

    editor.findChild<NumericField*>(QStringLiteral("continuousShapeSpinBox"))->setValue(0.7);
    QVERIFY(received.has_value());
    QCOMPARE(received->continuousShape(), 0.7);

    editor.findChild<NumericField*>(QStringLiteral("continuousSkewSpinBox"))->setValue(0.2);
    QCOMPARE(received->continuousSkew(), 0.2);

    editor.findChild<NumericField*>(QStringLiteral("continuousCharacterSpinBox"))->setValue(0.9);
    QCOMPARE(received->continuousCharacter(), 0.9);

    // Each spin-box edit above also keeps its own paired dial in sync.
    QCOMPARE(editor.findChild<RotaryDialWidget*>(QStringLiteral("continuousShapeDial"))->value(), 0.7);
    QCOMPARE(editor.findChild<RotaryDialWidget*>(QStringLiteral("continuousSkewDial"))->value(), 0.2);
    QCOMPARE(editor.findChild<RotaryDialWidget*>(QStringLiteral("continuousCharacterDial"))->value(), 0.9);
}

void MindWaveEditorTest::draggingAContinuousDialUpdatesItsOwnSpinBoxAndEmits() {
    MindWaveEditor editor;
    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    combo->setCurrentIndex(combo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::Continuous))));
    auto* dial = editor.findChild<RotaryDialWidget*>(QStringLiteral("continuousShapeDial"));
    QVERIFY(dial != nullptr);
    dial->resize(48, 48);
    std::optional<MindWave> received;
    connect(&editor, &MindWaveEditor::mindWaveChanged, [&](const MindWave& wave) { received = wave; });

    // Dragging up by 90px sweeps 0.6 of this dial's own [0, 1] range - see
    // test_rotary_dial_widget.cpp's own identical `kPixelsPerFullSweep`
    // (150px per full sweep) for where that number comes from.
    const QPoint start(24, 24);
    QTest::mousePress(dial, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(dial, start - QPoint(0, 90));
    QTest::mouseRelease(dial, Qt::LeftButton, Qt::NoModifier, start - QPoint(0, 90));

    QVERIFY(received.has_value());
    QVERIFY(qAbs(received->continuousShape() - 0.6) < 0.01);
    QVERIFY(qAbs(editor.findChild<NumericField*>(QStringLiteral("continuousShapeSpinBox"))->value() - 0.6) < 0.01);
}

void MindWaveEditorTest::changingContinuousFieldsUpdatesTheLivePreview() {
    MindWaveEditor editor;
    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    combo->setCurrentIndex(combo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::Continuous))));
    auto* preview = editor.findChild<WaveformPreviewWidget*>(QStringLiteral("continuousPreviewWidget"));
    QVERIFY(preview != nullptr);
    const auto flatSamples = preview->samples();

    editor.findChild<NumericField*>(QStringLiteral("continuousShapeSpinBox"))->setValue(1.0);

    QVERIFY(preview->samples() != flatSamples);
}

void MindWaveEditorTest::loadingAContinuousMindWaveSyncsAllThreeKnobs() {
    MindWaveEditor editor;
    MindWave wave;
    wave.setType(GeneratorType::Continuous);
    wave.setContinuousShape(0.4);
    wave.setContinuousSkew(0.1);
    wave.setContinuousCharacter(0.8);

    editor.setMindWave(wave);

    QCOMPARE(editor.findChild<NumericField*>(QStringLiteral("continuousShapeSpinBox"))->value(), 0.4);
    QCOMPARE(editor.findChild<NumericField*>(QStringLiteral("continuousSkewSpinBox"))->value(), 0.1);
    QCOMPARE(editor.findChild<NumericField*>(QStringLiteral("continuousCharacterSpinBox"))->value(), 0.8);
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("continuousGroup"))->isHidden());
    QCOMPARE(editor.findChild<RotaryDialWidget*>(QStringLiteral("continuousShapeDial"))->value(), 0.4);
    QCOMPARE(editor.findChild<RotaryDialWidget*>(QStringLiteral("continuousSkewDial"))->value(), 0.1);
    QCOMPARE(editor.findChild<RotaryDialWidget*>(QStringLiteral("continuousCharacterDial"))->value(), 0.8);
    QVERIFY(editor.findChild<WaveformPreviewWidget*>(QStringLiteral("continuousPreviewWidget"))->samples().size() > 0);
}

void MindWaveEditorTest::switchingToResonantShowsOnlyItsOwnGroup() {
    MindWaveEditor editor;
    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));

    combo->setCurrentIndex(combo->findData(QVariant::fromValue(static_cast<int>(GeneratorType::Resonance))));

    QCOMPARE(editor.mindWave().type(), GeneratorType::Resonance);
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("resonanceGroup"))->isHidden());
    QVERIFY(editor.findChild<QGroupBox*>(QStringLiteral("periodicGroup"))->isHidden());
}

void MindWaveEditorTest::settingAvailableResonantProfilesWithNoneShowsThePlaceholder() {
    MindWaveEditor editor;

    editor.setAvailableResonanceProfiles({});

    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("resonanceProfileCombo"));
    QCOMPARE(combo->count(), 1);
    QCOMPARE(combo->currentText(), QStringLiteral("(none captured yet)"));
}

void MindWaveEditorTest::selectingAResonantProfileUpdatesTheWaveAndEmits() {
    MindWaveEditor editor;
    editor.setAvailableResonanceProfiles({NamedResonanceProfile{1, "Profile One", {0.1F, 0.2F}},
                                          NamedResonanceProfile{2, "Profile Two", {0.3F, 0.4F, 0.5F}}});
    std::optional<MindWave> received;
    connect(&editor, &MindWaveEditor::mindWaveChanged, [&](const MindWave& wave) { received = wave; });

    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("resonanceProfileCombo"));
    combo->setCurrentIndex(combo->findData(QVariant::fromValue(static_cast<qulonglong>(2))));

    QVERIFY(received.has_value());
    QCOMPARE(received->sourceResonanceProfileId().value(), sound_mind::core::ResonanceProfileId{2});
    QCOMPARE(received->resonanceSpectrum().size(), std::size_t{3});
    QCOMPARE(received->resonanceSpectrum()[0], 0.3F);
    QCOMPARE(editor.mindWave().resonanceSpectrum()[0], 0.3F);
}

void MindWaveEditorTest::loadingAResonantMindWaveSyncsThePickersSelection() {
    MindWaveEditor editor;
    editor.setAvailableResonanceProfiles({NamedResonanceProfile{1, "Profile One", {0.1F, 0.2F}},
                                          NamedResonanceProfile{2, "Profile Two", {0.3F, 0.4F, 0.5F}}});
    MindWave wave;
    wave.setType(GeneratorType::Resonance);
    wave.setResonanceSpectrum(sound_mind::core::ResonanceProfileId{2}, {0.3F, 0.4F, 0.5F});

    editor.setMindWave(wave);

    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("resonanceProfileCombo"));
    QCOMPARE(combo->currentData().toULongLong(), static_cast<qulonglong>(2));
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("resonanceGroup"))->isHidden());
}

void MindWaveEditorTest::loadingAResonantMindWaveWithNoSourceIdSelectsThePlaceholder() {
    MindWaveEditor editor;
    // No profiles made available at all - the only item is the
    // "(none captured yet)" placeholder, the same fallback
    // ToolConfigurationPanel::setToolConfiguration()'s own identical
    // `ResonanceConfiguration` branch relies on.
    MindWave wave;
    wave.setType(GeneratorType::Resonance);

    editor.setMindWave(wave);

    auto* combo = editor.findChild<QComboBox*>(QStringLiteral("resonanceProfileCombo"));
    QCOMPARE(combo->currentIndex(), 0);
    QCOMPARE(combo->currentText(), QStringLiteral("(none captured yet)"));
}

void MindWaveEditorTest::choosingNoiseShowsItsControlsAndEditsReachTheMindWave() {
    MindWaveEditor editor;
    std::optional<MindWave> received;
    connect(&editor, &MindWaveEditor::mindWaveChanged, [&](const MindWave& wave) { received = wave; });
    auto* type = editor.findChild<QComboBox*>(QStringLiteral("generatorTypeCombo"));
    auto* group = editor.findChild<QGroupBox*>(QStringLiteral("noiseGroup"));
    QVERIFY(group != nullptr);
    QVERIFY(group->isHidden());

    type->setCurrentIndex(type->findData(static_cast<int>(GeneratorType::Noise)));
    QVERIFY(!group->isHidden());
    auto* color = editor.findChild<QComboBox*>(QStringLiteral("noiseColorCombo"));
    QCOMPARE(color->findText(QStringLiteral("Gray (equal loudness)")), -1);  // layer-only.
    color->setCurrentIndex(color->findData(static_cast<int>(sound_mind::core::NoiseColor::Red)));
    editor.findChild<NumericField*>(QStringLiteral("noiseCellWidthSpinBox"))->setValue(0.5);

    QVERIFY(received.has_value());
    QCOMPARE(received->type(), GeneratorType::Noise);
    QCOMPARE(received->noiseColor(), sound_mind::core::NoiseColor::Red);
    QCOMPARE(received->noiseCellWidth(), 0.5);
    QVERIFY(!editor.findChild<NumericField*>(QStringLiteral("noiseSlopeSpinBox"))->isEnabled());  // Custom only.
}

void MindWaveEditorTest::setMindWaveLoadsNoiseSettings() {
    MindWaveEditor editor;
    MindWave wave;
    wave.setType(GeneratorType::Noise);
    wave.setNoiseColor(sound_mind::core::NoiseColor::Custom);
    wave.setNoiseSlope(1.5);
    wave.setNoiseCellHeight(8.0);
    QSignalSpy spy(&editor, &MindWaveEditor::mindWaveChanged);

    editor.setMindWave(wave);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(editor.findChild<NumericField*>(QStringLiteral("noiseSlopeSpinBox"))->value(), 1.5);
    QVERIFY(editor.findChild<NumericField*>(QStringLiteral("noiseSlopeSpinBox"))->isEnabled());
    QCOMPARE(editor.findChild<NumericField*>(QStringLiteral("noiseCellHeightSpinBox"))->value(), 8.0);
    QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("noiseGroup"))->isHidden());
}
