#include "test_checklist_dialog.h"

#include <vector>

#include <QCheckBox>
#include <QPushButton>
#include <QtTest/QtTest>

#include "sound_mind/studio/checklist_dialog.h"

using sound_mind::studio::CheckListDialog;

void CheckListDialogTest::buildsOneCheckBoxPerLabelWithTheGivenInitialStates() {
    CheckListDialog dialog(QStringLiteral("Notes"), {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")},
                            {true, false, true});

    const auto checkBoxes = dialog.findChildren<QCheckBox*>();
    QCOMPARE(checkBoxes.size(), 3);
    QCOMPARE(checkBoxes.at(0)->text(), QStringLiteral("A"));
    QCOMPARE(checkBoxes.at(1)->text(), QStringLiteral("B"));
    QCOMPARE(checkBoxes.at(2)->text(), QStringLiteral("C"));
    QCOMPARE(dialog.checkedStates(), std::vector<bool>({true, false, true}));
}

void CheckListDialogTest::aShorterInitiallyCheckedListLeavesTheMissingTailUnchecked() {
    CheckListDialog dialog(QStringLiteral("Notes"), {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")},
                            {true});

    QCOMPARE(dialog.checkedStates(), std::vector<bool>({true, false, false}));
}

void CheckListDialogTest::selectAllChecksEveryBox() {
    CheckListDialog dialog(QStringLiteral("Notes"), {QStringLiteral("A"), QStringLiteral("B")}, {false, false});
    auto* selectAllButton = dialog.findChild<QPushButton*>(QStringLiteral("selectAllButton"));
    QVERIFY(selectAllButton != nullptr);
    QCOMPARE(selectAllButton->text(), QStringLiteral("Select All"));

    QTest::mouseClick(selectAllButton, Qt::LeftButton);

    QCOMPARE(dialog.checkedStates(), std::vector<bool>({true, true}));
}

void CheckListDialogTest::deselectAllUnchecksEveryBox() {
    CheckListDialog dialog(QStringLiteral("Notes"), {QStringLiteral("A"), QStringLiteral("B")}, {true, true});
    auto* deselectAllButton = dialog.findChild<QPushButton*>(QStringLiteral("deselectAllButton"));
    QVERIFY(deselectAllButton != nullptr);
    QCOMPARE(deselectAllButton->text(), QStringLiteral("Deselect All"));

    QTest::mouseClick(deselectAllButton, Qt::LeftButton);

    QCOMPARE(dialog.checkedStates(), std::vector<bool>({false, false}));
}

void CheckListDialogTest::checkedStatesReflectsManualToggles() {
    CheckListDialog dialog(QStringLiteral("Notes"), {QStringLiteral("A"), QStringLiteral("B")}, {false, false});
    const auto checkBoxes = dialog.findChildren<QCheckBox*>();

    checkBoxes.at(1)->setChecked(true);

    QCOMPARE(dialog.checkedStates(), std::vector<bool>({false, true}));
}
