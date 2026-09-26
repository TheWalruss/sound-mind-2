#include "test_audio_snippet_picker_dialog.h"

#include <algorithm>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QListWidget>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/audio_snippet_picker_dialog.h"

using sound_mind::studio::AudioSnippetPickerDialog;

namespace {

std::vector<AudioSnippetPickerDialog::RowData> threeSnippets() {
    AudioSnippetPickerDialog::RowData first;
    first.index = 0;
    first.startSeconds = 0.0;
    first.endSeconds = 10.0;

    AudioSnippetPickerDialog::RowData second;
    second.index = 1;
    second.startSeconds = 10.0;
    second.endSeconds = 20.0;

    AudioSnippetPickerDialog::RowData third;
    third.index = 2;
    third.startSeconds = 20.0;
    third.endSeconds = 24.5;  // a shorter, final snippet.

    return {first, second, third};
}

}  // namespace

void AudioSnippetPickerDialogTest::listsOneRowPerSnippetWithItsTimespan() {
    AudioSnippetPickerDialog dialog(threeSnippets());

    auto* list = dialog.findChild<QListWidget*>(QStringLiteral("snippetList"));
    QVERIFY(list != nullptr);
    QCOMPARE(list->count(), 3);
}

void AudioSnippetPickerDialogTest::allRowsStartChecked() {
    AudioSnippetPickerDialog dialog(threeSnippets());

    QCOMPARE(dialog.selectedIndices().size(), static_cast<std::size_t>(3));
}

void AudioSnippetPickerDialogTest::selectedIndicesReturnsOnlyCheckedRows() {
    AudioSnippetPickerDialog dialog(threeSnippets());

    auto* list = dialog.findChild<QListWidget*>(QStringLiteral("snippetList"));
    QVERIFY(list != nullptr);
    list->item(1)->setCheckState(Qt::Unchecked);

    const auto selected = dialog.selectedIndices();
    QCOMPARE(selected.size(), static_cast<std::size_t>(2));
    QVERIFY(std::find(selected.begin(), selected.end(), 0u) != selected.end());
    QVERIFY(std::find(selected.begin(), selected.end(), 1u) == selected.end());
    QVERIFY(std::find(selected.begin(), selected.end(), 2u) != selected.end());
}

void AudioSnippetPickerDialogTest::selectAllCheckboxTogglesEveryRow() {
    AudioSnippetPickerDialog dialog(threeSnippets());
    auto* selectAll = dialog.findChild<QCheckBox*>(QStringLiteral("selectAllCheckBox"));
    QVERIFY(selectAll != nullptr);

    selectAll->setChecked(false);
    QCOMPARE(dialog.selectedIndices().size(), static_cast<std::size_t>(0));

    selectAll->setChecked(true);
    QCOMPARE(dialog.selectedIndices().size(), static_cast<std::size_t>(3));
}

void AudioSnippetPickerDialogTest::freshDialogHasAZeroOffset() {
    const AudioSnippetPickerDialog dialog(threeSnippets());

    QCOMPARE(dialog.offsetSeconds(), 0.0);
}

void AudioSnippetPickerDialogTest::changingTheOffsetSpinBoxEmitsOffsetChanged() {
    AudioSnippetPickerDialog dialog(threeSnippets());
    QSignalSpy spy(&dialog, &AudioSnippetPickerDialog::offsetChanged);

    auto* offsetSpinBox = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("offsetSpinBox"));
    QVERIFY(offsetSpinBox != nullptr);
    offsetSpinBox->setValue(1.5);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.constFirst().at(0).toDouble(), 1.5);
    QCOMPARE(dialog.offsetSeconds(), 1.5);
}

void AudioSnippetPickerDialogTest::setSnippetsReplacesTheListAndResetsEveryRowToChecked() {
    AudioSnippetPickerDialog dialog(threeSnippets());
    auto* list = dialog.findChild<QListWidget*>(QStringLiteral("snippetList"));
    QVERIFY(list != nullptr);
    list->item(0)->setCheckState(Qt::Unchecked);  // an edit that a real recompute should discard.

    AudioSnippetPickerDialog::RowData onlyOne;
    onlyOne.index = 0;
    onlyOne.startSeconds = 1.5;
    onlyOne.endSeconds = 11.5;
    dialog.setSnippets({onlyOne});

    QCOMPARE(list->count(), 1);
    // Every row resets to checked - a changed offset is a genuinely new
    // split, not an edit to the old one - see setSnippets()'s own docs.
    QCOMPARE(dialog.selectedIndices().size(), static_cast<std::size_t>(1));
}
