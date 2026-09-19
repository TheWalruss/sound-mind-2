#include "test_chord_generator_panel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSignalSpy>
#include <QSpinBox>
#include <QWidget>
#include <QtTest/QtTest>

#include "sound_mind/core/chord_generator.h"
#include "sound_mind/studio/chord_generator_panel.h"

using sound_mind::core::ArpeggioOrder;
using sound_mind::core::ChordCategory;
using sound_mind::core::ChordGeneratorParams;
using sound_mind::core::ChordPlaybackMode;
using sound_mind::studio::ChordGeneratorPanel;

void ChordGeneratorPanelTest::freshPanelDefaultsToACMajorTriadInBlockMode() {
    ChordGeneratorPanel panel;

    QCOMPARE(panel.params().rootMidiNote, 60);
    QCOMPARE(static_cast<int>(panel.params().category), static_cast<int>(ChordCategory::Triads));
    QCOMPARE(panel.params().chordIndex, std::size_t{0});
    QCOMPARE(static_cast<int>(panel.params().mode), static_cast<int>(ChordPlaybackMode::Block));
}

void ChordGeneratorPanelTest::changingCategoryRebuildsTheChordComboAndEmitsParamsChanged() {
    ChordGeneratorPanel panel;
    auto* categoryCombo = panel.findChild<QComboBox*>(QStringLiteral("chordCategoryCombo"));
    auto* chordCombo = panel.findChild<QComboBox*>(QStringLiteral("chordNameCombo"));
    QVERIFY(categoryCombo != nullptr);
    QVERIFY(chordCombo != nullptr);
    QCOMPARE(chordCombo->count(), 7);  // Triads has 7 chords.
    QSignalSpy spy(&panel, &ChordGeneratorPanel::paramsChanged);

    const int sevenIndex = categoryCombo->findData(QVariant::fromValue(static_cast<int>(ChordCategory::Sevenths)));
    categoryCombo->setCurrentIndex(sevenIndex);

    QCOMPARE(chordCombo->count(), 10);  // Sevenths has 10 chords.
    QVERIFY(spy.count() >= 1);
    QCOMPARE(static_cast<int>(panel.params().category), static_cast<int>(ChordCategory::Sevenths));
}

void ChordGeneratorPanelTest::changingRootOrOctaveUpdatesRootMidiNote() {
    ChordGeneratorPanel panel;
    auto* rootCombo = panel.findChild<QComboBox*>(QStringLiteral("chordRootCombo"));
    auto* octaveSpinBox = panel.findChild<QSpinBox*>(QStringLiteral("chordOctaveSpinBox"));
    QVERIFY(rootCombo != nullptr);
    QVERIFY(octaveSpinBox != nullptr);

    rootCombo->setCurrentIndex(9);  // "A"
    octaveSpinBox->setValue(4);

    QCOMPARE(panel.params().rootMidiNote, 69);  // A4
}

void ChordGeneratorPanelTest::switchingToArpeggioModeShowsTheArpeggioGroupAndHidesTheBlockGroup() {
    ChordGeneratorPanel panel;
    panel.show();
    auto* modeCombo = panel.findChild<QComboBox*>(QStringLiteral("chordModeCombo"));
    auto* blockGroup = panel.findChild<QWidget*>(QStringLiteral("chordBlockGroup"));
    auto* arpeggioGroup = panel.findChild<QWidget*>(QStringLiteral("chordArpeggioGroup"));
    QVERIFY(modeCombo != nullptr);
    QVERIFY(blockGroup->isVisible());
    QVERIFY(!arpeggioGroup->isVisible());

    modeCombo->setCurrentIndex(1);  // Arpeggio

    QVERIFY(!blockGroup->isVisible());
    QVERIFY(arpeggioGroup->isVisible());
}

void ChordGeneratorPanelTest::customOrderOnlyShowsTheCustomSequenceLineEdit() {
    ChordGeneratorPanel panel;
    panel.show();
    auto* modeCombo = panel.findChild<QComboBox*>(QStringLiteral("chordModeCombo"));
    auto* orderCombo = panel.findChild<QComboBox*>(QStringLiteral("chordOrderCombo"));
    auto* customLineEdit = panel.findChild<QLineEdit*>(QStringLiteral("chordCustomIndicesLineEdit"));
    modeCombo->setCurrentIndex(1);  // Arpeggio
    QVERIFY(!customLineEdit->isVisible());

    const int customIndex = orderCombo->findData(QVariant::fromValue(static_cast<int>(ArpeggioOrder::Custom)));
    orderCombo->setCurrentIndex(customIndex);

    QVERIFY(customLineEdit->isVisible());
    QCOMPARE(static_cast<int>(panel.params().order), static_cast<int>(ArpeggioOrder::Custom));
}

void ChordGeneratorPanelTest::randomOrderOnlyShowsTheRandomSeedSpinBox() {
    ChordGeneratorPanel panel;
    panel.show();
    auto* modeCombo = panel.findChild<QComboBox*>(QStringLiteral("chordModeCombo"));
    auto* orderCombo = panel.findChild<QComboBox*>(QStringLiteral("chordOrderCombo"));
    auto* seedSpinBox = panel.findChild<QSpinBox*>(QStringLiteral("chordRandomSeedSpinBox"));
    modeCombo->setCurrentIndex(1);  // Arpeggio
    QVERIFY(!seedSpinBox->isVisible());

    const int randomIndex = orderCombo->findData(QVariant::fromValue(static_cast<int>(ArpeggioOrder::Random)));
    orderCombo->setCurrentIndex(randomIndex);

    QVERIFY(seedSpinBox->isVisible());
}

void ChordGeneratorPanelTest::editingCustomSequenceParsesCommaSeparatedIndices() {
    ChordGeneratorPanel panel;
    auto* modeCombo = panel.findChild<QComboBox*>(QStringLiteral("chordModeCombo"));
    auto* orderCombo = panel.findChild<QComboBox*>(QStringLiteral("chordOrderCombo"));
    auto* customLineEdit = panel.findChild<QLineEdit*>(QStringLiteral("chordCustomIndicesLineEdit"));
    modeCombo->setCurrentIndex(1);  // Arpeggio
    const int customIndex = orderCombo->findData(QVariant::fromValue(static_cast<int>(ArpeggioOrder::Custom)));
    orderCombo->setCurrentIndex(customIndex);

    customLineEdit->setText(QStringLiteral("0, 2, 1, notanumber, 2"));

    QCOMPARE(panel.params().customOrderIndices, (std::vector<int>{0, 2, 1, 2}));
}

void ChordGeneratorPanelTest::subdivisionComboSetsStepBeats() {
    ChordGeneratorPanel panel;
    auto* modeCombo = panel.findChild<QComboBox*>(QStringLiteral("chordModeCombo"));
    auto* subdivisionCombo = panel.findChild<QComboBox*>(QStringLiteral("chordSubdivisionCombo"));
    modeCombo->setCurrentIndex(1);  // Arpeggio

    subdivisionCombo->setCurrentIndex(2);  // "1/4"

    QCOMPARE(panel.params().stepBeats, 1.0);
}
