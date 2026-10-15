#include "test_record_panel.h"

#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/record_panel.h"

using sound_mind::studio::RecordPanel;

void RecordPanelTest::toggleButtonEmitsToggleRequested() {
    RecordPanel panel;
    QSignalSpy spy(&panel, &RecordPanel::toggleRequested);

    auto* button = panel.findChild<QPushButton*>(QStringLiteral("recordToggleButton"));
    QVERIFY(button != nullptr);
    button->click();

    QCOMPARE(spy.count(), 1);
}

void RecordPanelTest::setRecordingUpdatesButtonLabelAndCheckedState() {
    RecordPanel panel;
    auto* button = panel.findChild<QPushButton*>(QStringLiteral("recordToggleButton"));
    QVERIFY(button != nullptr);

    panel.setRecording(true);
    QVERIFY(button->isChecked());
    QCOMPARE(button->text(), QStringLiteral("Stop Recording"));

    panel.setRecording(false);
    QVERIFY(!button->isChecked());
    QCOMPARE(button->text(), QStringLiteral("Start Recording"));
}

namespace {

std::vector<std::pair<sound_mind::core::LayerId, QString>> twoRecordedLayers() {
    return {{sound_mind::core::LayerId{4}, QStringLiteral("Recording")},
            {sound_mind::core::LayerId{9}, QStringLiteral("Vocals")}};
}

}  // namespace

void RecordPanelTest::theTargetDefaultsToANewLayerNamedRecording() {
    RecordPanel panel;
    QVERIFY(!panel.targetLayerId().has_value());
    QCOMPARE(panel.newLayerName(), QStringLiteral("Recording"));
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("recordTargetCombo"));
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->count(), 1);
}

void RecordPanelTest::recordedLayersAreListedAndSelectable() {
    RecordPanel panel;
    panel.setRecordedLayers(twoRecordedLayers());
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("recordTargetCombo"));
    QCOMPARE(combo->count(), 3);
    QCOMPARE(combo->itemText(2), QStringLiteral("Vocals"));

    combo->setCurrentIndex(2);

    QCOMPARE(panel.targetLayerId(), std::optional<sound_mind::core::LayerId>(9));
}

void RecordPanelTest::theNameFieldShowsOnlyForANewLayer() {
    RecordPanel panel;
    panel.setRecordedLayers(twoRecordedLayers());
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("recordTargetCombo"));
    auto* name = panel.findChild<QLineEdit*>(QStringLiteral("recordNameEdit"));
    QVERIFY(name != nullptr);
    QVERIFY(!name->isHidden());

    name->setText(QStringLiteral("  Guitar  "));
    QCOMPARE(panel.newLayerName(), QStringLiteral("Guitar"));
    name->setText(QString());
    QCOMPARE(panel.newLayerName(), QStringLiteral("Recording"));

    combo->setCurrentIndex(1);
    QVERIFY(name->isHidden());
}

void RecordPanelTest::aRemovedTargetFallsBackToANewLayer() {
    RecordPanel panel;
    panel.setRecordedLayers(twoRecordedLayers());
    panel.findChild<QComboBox*>(QStringLiteral("recordTargetCombo"))->setCurrentIndex(2);

    panel.setRecordedLayers({{sound_mind::core::LayerId{4}, QStringLiteral("Recording")}});

    QVERIFY(!panel.targetLayerId().has_value());
}

void RecordPanelTest::targetControlsAreLockedWhileRecording() {
    RecordPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("recordTargetCombo"));
    auto* name = panel.findChild<QLineEdit*>(QStringLiteral("recordNameEdit"));

    panel.setRecording(true);
    QVERIFY(!combo->isEnabled());
    QVERIFY(!name->isEnabled());
    panel.setRecording(false);
    QVERIFY(combo->isEnabled());
}

void RecordPanelTest::liveViewCheckBoxEmitsLiveViewToggled() {
    RecordPanel panel;
    QSignalSpy spy(&panel, &RecordPanel::liveViewToggled);
    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("liveViewCheckBox"));
    QVERIFY(checkBox != nullptr);
    QVERIFY(!checkBox->isChecked());

    checkBox->setChecked(true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
}
