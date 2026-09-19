#include "test_mind_waves_panel.h"

#include <optional>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QSpinBox>
#include <QtTest/QtTest>

#include "sound_mind/studio/mind_wave_editor.h"
#include "sound_mind/studio/mind_waves_panel.h"

using sound_mind::core::MindWave;
using sound_mind::core::MindWaveId;
using sound_mind::core::SuperpositionBlendMode;
using sound_mind::studio::MindWaveEditor;
using sound_mind::studio::MindWavesPanel;

namespace {

std::vector<MindWavesPanel::RowData> twoRows() {
    MindWavesPanel::RowData first;
    first.id = 1;
    first.name = QStringLiteral("First");
    MindWavesPanel::RowData second;
    second.id = 2;
    second.name = QStringLiteral("Second");
    return {first, second};
}

}  // namespace

void MindWavesPanelTest::freshPanelHasNoRowsAndNoSelection() {
    const MindWavesPanel panel;
    QVERIFY(!panel.selectedMindWaveId().has_value());
    QCOMPARE(panel.findChild<QListWidget*>(QStringLiteral("mindWavesList"))->count(), 0);
    QVERIFY(!panel.findChild<MindWaveEditor*>(QStringLiteral("mindWaveEditor"))->isEnabled());
}

void MindWavesPanelTest::setMindWavesPopulatesTheListAndClickingARowSelectsIt() {
    MindWavesPanel panel;
    QSignalSpy spy(&panel, &MindWavesPanel::selectionChanged);

    panel.setMindWaves(twoRows());
    QCOMPARE(panel.findChild<QListWidget*>(QStringLiteral("mindWavesList"))->count(), 2);

    const auto nameLabels = panel.findChildren<QLabel*>(QStringLiteral("nameLabel"));
    QCOMPARE(nameLabels.size(), 2);
    QTest::mouseClick(nameLabels.at(0), Qt::LeftButton);

    QCOMPARE(spy.count(), 1);
    QVERIFY(panel.selectedMindWaveId().has_value());
    QCOMPARE(*panel.selectedMindWaveId(), MindWaveId{1});
    QVERIFY(panel.findChild<MindWaveEditor*>(QStringLiteral("mindWaveEditor"))->isEnabled());
}

void MindWavesPanelTest::addButtonEmitsAddRequested() {
    MindWavesPanel panel;
    QSignalSpy spy(&panel, &MindWavesPanel::addRequested);

    QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("addMindWaveButton")), Qt::LeftButton);

    QCOMPARE(spy.count(), 1);
}

void MindWavesPanelTest::deletingARowEmitsDeleteRequestedWithItsId() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());
    QSignalSpy spy(&panel, &MindWavesPanel::deleteRequested);

    const auto buttons = panel.findChildren<QPushButton*>(QStringLiteral("deleteButton"));
    QCOMPARE(buttons.size(), 2);
    QTest::mouseClick(buttons.at(1), Qt::LeftButton);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<MindWaveId>(), MindWaveId{2});
}

void MindWavesPanelTest::renamingARowEmitsRenameRequestedWithItsId() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());
    QSignalSpy spy(&panel, &MindWavesPanel::renameRequested);

    const auto nameLabels = panel.findChildren<QLabel*>(QStringLiteral("nameLabel"));
    QTest::mouseDClick(nameLabels.at(0), Qt::LeftButton);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<MindWaveId>(), MindWaveId{1});
}

void MindWavesPanelTest::editingTheTopEditorEmitsMindWaveChangedWithTheSelectedId() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());
    panel.selectMindWave(2);
    // A lambda capture, not QSignalSpy::value<T>() - MindWave isn't a
    // registered Qt meta type, matching FilterConfigurationPanelTest's own
    // established idiom for capturing a custom Core type off a signal.
    int emitCount = 0;
    std::optional<MindWaveId> receivedId;
    std::optional<MindWave> receivedWave;
    connect(&panel, &MindWavesPanel::mindWaveChanged, [&](MindWaveId id, const MindWave& wave) {
        ++emitCount;
        receivedId = id;
        receivedWave = wave;
    });

    panel.findChild<MindWaveEditor*>(QStringLiteral("mindWaveEditor"))
        ->findChild<QDoubleSpinBox*>(QStringLiteral("periodSpinBox"))
        ->setValue(4.0);

    QCOMPARE(emitCount, 1);
    QCOMPARE(receivedId, std::optional<MindWaveId>(2));
    QVERIFY(receivedWave.has_value());
    QCOMPARE(receivedWave->period(), 4.0);
}

void MindWavesPanelTest::addingAStackMemberGrowsTheStackAndEmits() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());
    panel.selectMindWave(1);
    int emitCount = 0;
    std::optional<MindWave> receivedWave;
    connect(&panel, &MindWavesPanel::mindWaveChanged,
            [&](MindWaveId, const MindWave& wave) { ++emitCount; receivedWave = wave; });

    QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("addMemberButton")), Qt::LeftButton);

    QCOMPARE(emitCount, 1);
    QVERIFY(receivedWave.has_value());
    QCOMPARE(receivedWave->superpositionStack().size(), std::size_t{1});
    QCOMPARE(panel.findChild<QListWidget*>(QStringLiteral("stackList"))->count(), 1);
}

void MindWavesPanelTest::removingASelectedStackMemberShrinksTheStackAndEmits() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());
    panel.selectMindWave(1);
    QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("addMemberButton")), Qt::LeftButton);
    auto* stackList = panel.findChild<QListWidget*>(QStringLiteral("stackList"));
    stackList->setCurrentRow(0);
    int emitCount = 0;
    std::optional<MindWave> receivedWave;
    connect(&panel, &MindWavesPanel::mindWaveChanged,
            [&](MindWaveId, const MindWave& wave) { ++emitCount; receivedWave = wave; });

    QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("removeMemberButton")), Qt::LeftButton);

    QCOMPARE(emitCount, 1);
    QVERIFY(receivedWave.has_value());
    QCOMPARE(receivedWave->superpositionStack().size(), std::size_t{0});
    QCOMPARE(stackList->count(), 0);
}

void MindWavesPanelTest::editingASelectedStackMemberUpdatesThatIndexAndEmits() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());
    panel.selectMindWave(1);
    QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("addMemberButton")), Qt::LeftButton);
    panel.findChild<QListWidget*>(QStringLiteral("stackList"))->setCurrentRow(0);
    int emitCount = 0;
    std::optional<MindWave> receivedWave;
    connect(&panel, &MindWavesPanel::mindWaveChanged,
            [&](MindWaveId, const MindWave& wave) { ++emitCount; receivedWave = wave; });

    panel.findChild<MindWaveEditor*>(QStringLiteral("stackMemberEditor"))
        ->findChild<QDoubleSpinBox*>(QStringLiteral("periodSpinBox"))
        ->setValue(9.0);

    QCOMPARE(emitCount, 1);
    QVERIFY(receivedWave.has_value());
    const auto stack = receivedWave->superpositionStack();
    QCOMPARE(stack.size(), std::size_t{1});
    QCOMPARE(stack.front().period(), 9.0);
}

void MindWavesPanelTest::changingBlendModeEmits() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());
    panel.selectMindWave(1);
    int emitCount = 0;
    std::optional<MindWave> receivedWave;
    connect(&panel, &MindWavesPanel::mindWaveChanged,
            [&](MindWaveId, const MindWave& wave) { ++emitCount; receivedWave = wave; });

    auto* blendCombo = panel.findChild<QComboBox*>(QStringLiteral("blendModeCombo"));
    blendCombo->setCurrentIndex(blendCombo->findData(QVariant::fromValue(static_cast<int>(SuperpositionBlendMode::Add))));

    QCOMPARE(emitCount, 1);
    QVERIFY(receivedWave.has_value());
    QCOMPARE(receivedWave->superpositionBlendMode(), SuperpositionBlendMode::Add);
}

void MindWavesPanelTest::enablingWarpEmitsAndSetsHasWarpSourceOnTheComposite() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());
    panel.selectMindWave(1);
    QVERIFY(!panel.findChild<MindWaveEditor*>(QStringLiteral("warpSourceEditor"))->isEnabled());
    int emitCount = 0;
    std::optional<MindWave> receivedWave;
    connect(&panel, &MindWavesPanel::mindWaveChanged,
            [&](MindWaveId, const MindWave& wave) { ++emitCount; receivedWave = wave; });

    QTest::mouseClick(panel.findChild<QCheckBox*>(QStringLiteral("warpEnabledCheckBox")), Qt::LeftButton);

    QCOMPARE(emitCount, 1);
    QVERIFY(receivedWave.has_value());
    QVERIFY(receivedWave->hasWarpSource());
    QVERIFY(panel.findChild<MindWaveEditor*>(QStringLiteral("warpSourceEditor"))->isEnabled());
}

void MindWavesPanelTest::disablingWarpClearsTheWarpSourceAndEmits() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());
    panel.selectMindWave(1);
    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("warpEnabledCheckBox"));
    QTest::mouseClick(checkBox, Qt::LeftButton);  // Enable first.
    int emitCount = 0;
    std::optional<MindWave> receivedWave;
    connect(&panel, &MindWavesPanel::mindWaveChanged,
            [&](MindWaveId, const MindWave& wave) { ++emitCount; receivedWave = wave; });

    QTest::mouseClick(checkBox, Qt::LeftButton);  // Disable.

    QCOMPARE(emitCount, 1);
    QVERIFY(receivedWave.has_value());
    QVERIFY(!receivedWave->hasWarpSource());
    QVERIFY(!panel.findChild<MindWaveEditor*>(QStringLiteral("warpSourceEditor"))->isEnabled());
}

void MindWavesPanelTest::editingTheWarpSourceEditorUpdatesItAndEmits() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());
    panel.selectMindWave(1);
    QTest::mouseClick(panel.findChild<QCheckBox*>(QStringLiteral("warpEnabledCheckBox")), Qt::LeftButton);
    int emitCount = 0;
    std::optional<MindWave> receivedWave;
    connect(&panel, &MindWavesPanel::mindWaveChanged,
            [&](MindWaveId, const MindWave& wave) { ++emitCount; receivedWave = wave; });

    panel.findChild<MindWaveEditor*>(QStringLiteral("warpSourceEditor"))
        ->findChild<QDoubleSpinBox*>(QStringLiteral("periodSpinBox"))
        ->setValue(7.0);

    QCOMPARE(emitCount, 1);
    QVERIFY(receivedWave.has_value());
    QVERIFY(receivedWave->hasWarpSource());
    QCOMPARE(receivedWave->warpSource().period(), 7.0);
}

void MindWavesPanelTest::changingWarpStrengthEmitsWhileWarpIsEnabled() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());
    panel.selectMindWave(1);
    QTest::mouseClick(panel.findChild<QCheckBox*>(QStringLiteral("warpEnabledCheckBox")), Qt::LeftButton);
    int emitCount = 0;
    std::optional<MindWave> receivedWave;
    connect(&panel, &MindWavesPanel::mindWaveChanged,
            [&](MindWaveId, const MindWave& wave) { ++emitCount; receivedWave = wave; });

    panel.findChild<QDoubleSpinBox*>(QStringLiteral("warpStrengthSpinBox"))->setValue(2.5);

    QCOMPARE(emitCount, 1);
    QVERIFY(receivedWave.has_value());
    QCOMPARE(receivedWave->warpStrength(), 2.5);
}

void MindWavesPanelTest::selectingARowWithNoWarpSourceLeavesTheWarpControlsDisabled() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());

    panel.selectMindWave(2);

    QVERIFY(!panel.findChild<QCheckBox*>(QStringLiteral("warpEnabledCheckBox"))->isChecked());
    QVERIFY(!panel.findChild<MindWaveEditor*>(QStringLiteral("warpSourceEditor"))->isEnabled());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("warpStrengthSpinBox"))->isEnabled());
}

void MindWavesPanelTest::selectingARowWithAnExistingWarpSourcePopulatesTheCheckboxStrengthAndEditor() {
    MindWave source;
    source.setPeriod(3.0);
    MindWave warped;
    warped.setWarpSource(source);
    warped.setWarpStrength(1.5);
    MindWavesPanel::RowData row;
    row.id = 9;
    row.name = QStringLiteral("Warped");
    row.wave = warped;

    MindWavesPanel panel;
    panel.setMindWaves({row});

    panel.selectMindWave(9);

    QVERIFY(panel.findChild<QCheckBox*>(QStringLiteral("warpEnabledCheckBox"))->isChecked());
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("warpStrengthSpinBox"))->value(), 1.5);
    QVERIFY(panel.findChild<MindWaveEditor*>(QStringLiteral("warpSourceEditor"))->isEnabled());
    QCOMPARE(panel.findChild<MindWaveEditor*>(QStringLiteral("warpSourceEditor"))->mindWave().period(), 3.0);
}

void MindWavesPanelTest::selectMindWaveSelectsAnExistingRowAndNoOpsForUnknownId() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());

    panel.selectMindWave(2);
    QCOMPARE(panel.selectedMindWaveId(), std::optional<MindWaveId>(2));

    panel.selectMindWave(999);
    // No matching row - selection unchanged, per selectMindWave()'s own docs.
    QCOMPARE(panel.selectedMindWaveId(), std::optional<MindWaveId>(2));
}

void MindWavesPanelTest::setMindWavesPreservesSelectionAndRedisplaysFromTheNewData() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());
    panel.selectMindWave(1);

    auto rows = twoRows();
    rows[0].wave.setPeriod(7.0);
    panel.setMindWaves(rows);

    QCOMPARE(panel.selectedMindWaveId(), std::optional<MindWaveId>(1));
    QCOMPARE(panel.findChild<MindWaveEditor*>(QStringLiteral("mindWaveEditor"))->mindWave().period(), 7.0);
}

void MindWavesPanelTest::setMindWavesDropsSelectionWhenTheIdIsGone() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());
    panel.selectMindWave(1);
    QSignalSpy spy(&panel, &MindWavesPanel::selectionChanged);

    panel.setMindWaves({twoRows()[1]});  // id 1 no longer present.

    QVERIFY(!panel.selectedMindWaveId().has_value());
    QCOMPARE(spy.count(), 1);
    QVERIFY(!panel.findChild<MindWaveEditor*>(QStringLiteral("mindWaveEditor"))->isEnabled());
}

void MindWavesPanelTest::clearSelectionDisablesBothEditorsAndEmits() {
    MindWavesPanel panel;
    panel.setMindWaves(twoRows());
    panel.selectMindWave(1);
    QSignalSpy spy(&panel, &MindWavesPanel::selectionChanged);

    panel.clearSelection();

    QVERIFY(!panel.selectedMindWaveId().has_value());
    QCOMPARE(spy.count(), 1);
    QVERIFY(!panel.findChild<MindWaveEditor*>(QStringLiteral("mindWaveEditor"))->isEnabled());
    QVERIFY(!panel.findChild<MindWaveEditor*>(QStringLiteral("stackMemberEditor"))->isEnabled());
}

void MindWavesPanelTest::contentIsInAResizableScrollAreaSoThePanelCanShrinkBelowItsFullHeight() {
    MindWavesPanel panel;

    auto* scrollArea = qobject_cast<QScrollArea*>(panel.widget());
    QVERIFY(scrollArea != nullptr);
    QVERIFY(scrollArea->widgetResizable());

    // Select a MindWave so the top MindWaveEditor is actually populated
    // and enabled - this is the "MindWave editor is too tall" case being
    // guarded against: stacked below the (height-capped) library list,
    // an uncapped MindWaveEditor plus the superposition controls plus a
    // second uncapped MindWaveEditor comfortably exceed a typical dock's
    // available height once laid out in full. The panel's own
    // minimumSizeHint() must stay small regardless of that real content
    // height - proving the QScrollArea, not the panel's own top-level
    // layout, is what actually absorbs it, so the dock itself can still
    // be resized down (and a scrollbar appears for the rest) instead of
    // the panel being forced to grow to fit everything at once.
    panel.setMindWaves(twoRows());
    panel.selectMindWave(1);

    QVERIFY(panel.minimumSizeHint().height() < 250);
}

void MindWavesPanelTest::freshPanelHasPreviewOff() {
    const MindWavesPanel panel;
    QVERIFY(!panel.previewEnabled());
}

void MindWavesPanelTest::clickingThePreviewButtonEmitsPreviewToggled() {
    MindWavesPanel panel;
    QSignalSpy spy(&panel, &MindWavesPanel::previewToggled);

    auto* previewButton = panel.findChild<QPushButton*>(QStringLiteral("previewButton"));
    QVERIFY(previewButton != nullptr);
    previewButton->click();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.constFirst().at(0).toBool(), true);
    QVERIFY(panel.previewEnabled());

    previewButton->click();

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.constLast().at(0).toBool(), false);
    QVERIFY(!panel.previewEnabled());
}

void MindWavesPanelTest::setPreviewEnabledChangesTheButtonWithoutEmitting() {
    MindWavesPanel panel;
    QSignalSpy spy(&panel, &MindWavesPanel::previewToggled);

    panel.setPreviewEnabled(true);

    QVERIFY(panel.previewEnabled());
    QCOMPARE(spy.count(), 0);
}
