#include "test_midi_import_dialog.h"

#include <algorithm>

#include <QCheckBox>
#include <QListWidget>
#include <QtTest/QtTest>

#include "sound_mind/core/midi_import.h"
#include "sound_mind/studio/audio_snippet_picker_dialog.h"
#include "sound_mind/studio/midi_import_dialog.h"

using sound_mind::core::MidiChannelNotes;
using sound_mind::core::NoteEvent;
using sound_mind::studio::AudioSnippetPickerDialog;
using sound_mind::studio::MidiImportDialog;

namespace {

std::vector<MidiChannelNotes> twoChannels() {
    MidiChannelNotes first;
    first.channelNumber = 1;
    first.programNumber = 0;
    first.instrumentName = "Acoustic Grand Piano";
    first.notes = {NoteEvent{0.0, 0.5, 440.0}};

    MidiChannelNotes second;
    second.channelNumber = 2;
    second.programNumber = 4;
    second.instrumentName = "Electric Piano 1";
    second.notes = {NoteEvent{0.0, 0.5, 261.6256}, NoteEvent{0.5, 0.5, 329.6276}};

    return {first, second};
}

std::vector<AudioSnippetPickerDialog::RowData> twoSnippets() {
    AudioSnippetPickerDialog::RowData first;
    first.index = 0;
    first.startSeconds = 0.0;
    first.endSeconds = 10.0;

    AudioSnippetPickerDialog::RowData second;
    second.index = 1;
    second.startSeconds = 10.0;
    second.endSeconds = 15.0;

    return {first, second};
}

}  // namespace

void MidiImportDialogTest::listsOneRowPerChannelAndOneRowPerSnippet() {
    MidiImportDialog dialog(twoChannels(), twoSnippets());

    auto* channelList = dialog.findChild<QListWidget*>(QStringLiteral("channelList"));
    auto* snippetList = dialog.findChild<QListWidget*>(QStringLiteral("snippetList"));
    QVERIFY(channelList != nullptr);
    QVERIFY(snippetList != nullptr);
    QCOMPARE(channelList->count(), 2);
    QCOMPARE(snippetList->count(), 2);
}

void MidiImportDialogTest::allRowsStartCheckedAndSeparateLayerStartsChecked() {
    MidiImportDialog dialog(twoChannels(), twoSnippets());

    QCOMPARE(dialog.selectedChannelNumbers().size(), static_cast<std::size_t>(2));
    QCOMPARE(dialog.selectedSnippetIndices().size(), static_cast<std::size_t>(2));
    QVERIFY(dialog.separateLayerPerChannel());
}

void MidiImportDialogTest::selectedChannelNumbersReturnsOnlyCheckedRows() {
    MidiImportDialog dialog(twoChannels(), twoSnippets());
    auto* channelList = dialog.findChild<QListWidget*>(QStringLiteral("channelList"));
    QVERIFY(channelList != nullptr);
    channelList->item(0)->setCheckState(Qt::Unchecked);

    const auto selected = dialog.selectedChannelNumbers();
    QCOMPARE(selected.size(), static_cast<std::size_t>(1));
    QCOMPARE(selected.front(), 2);
}

void MidiImportDialogTest::selectedSnippetIndicesReturnsOnlyCheckedRows() {
    MidiImportDialog dialog(twoChannels(), twoSnippets());
    auto* snippetList = dialog.findChild<QListWidget*>(QStringLiteral("snippetList"));
    QVERIFY(snippetList != nullptr);
    snippetList->item(1)->setCheckState(Qt::Unchecked);

    const auto selected = dialog.selectedSnippetIndices();
    QCOMPARE(selected.size(), static_cast<std::size_t>(1));
    QCOMPARE(selected.front(), static_cast<std::size_t>(0));
}

void MidiImportDialogTest::selectAllChannelsCheckboxTogglesEveryChannelRow() {
    MidiImportDialog dialog(twoChannels(), twoSnippets());
    auto* selectAll = dialog.findChild<QCheckBox*>(QStringLiteral("selectAllChannelsCheckBox"));
    QVERIFY(selectAll != nullptr);

    selectAll->setChecked(false);
    QCOMPARE(dialog.selectedChannelNumbers().size(), static_cast<std::size_t>(0));

    selectAll->setChecked(true);
    QCOMPARE(dialog.selectedChannelNumbers().size(), static_cast<std::size_t>(2));
}

void MidiImportDialogTest::selectAllSnippetsCheckboxTogglesEverySnippetRow() {
    MidiImportDialog dialog(twoChannels(), twoSnippets());
    auto* selectAll = dialog.findChild<QCheckBox*>(QStringLiteral("selectAllSnippetsCheckBox"));
    QVERIFY(selectAll != nullptr);

    selectAll->setChecked(false);
    QCOMPARE(dialog.selectedSnippetIndices().size(), static_cast<std::size_t>(0));

    selectAll->setChecked(true);
    QCOMPARE(dialog.selectedSnippetIndices().size(), static_cast<std::size_t>(2));
}

void MidiImportDialogTest::uncheckingSeparateLayerCheckboxIsReflected() {
    MidiImportDialog dialog(twoChannels(), twoSnippets());
    auto* checkBox = dialog.findChild<QCheckBox*>(QStringLiteral("separateLayerCheckBox"));
    QVERIFY(checkBox != nullptr);

    checkBox->setChecked(false);

    QVERIFY(!dialog.separateLayerPerChannel());
}

void MidiImportDialogTest::wholeFileStartsCheckedAndDisablesSnippetControls() {
    const MidiImportDialog dialog(twoChannels(), twoSnippets());

    QVERIFY(dialog.wholeFile());
    auto* snippetList = dialog.findChild<QListWidget*>(QStringLiteral("snippetList"));
    auto* selectAllSnippets = dialog.findChild<QCheckBox*>(QStringLiteral("selectAllSnippetsCheckBox"));
    QVERIFY(snippetList != nullptr);
    QVERIFY(selectAllSnippets != nullptr);
    QVERIFY(!snippetList->isEnabled());
    QVERIFY(!selectAllSnippets->isEnabled());
}

void MidiImportDialogTest::uncheckingWholeFileEnablesSnippetControls() {
    MidiImportDialog dialog(twoChannels(), twoSnippets());
    auto* wholeFileCheckBox = dialog.findChild<QCheckBox*>(QStringLiteral("wholeFileCheckBox"));
    QVERIFY(wholeFileCheckBox != nullptr);

    wholeFileCheckBox->setChecked(false);

    QVERIFY(!dialog.wholeFile());
    auto* snippetList = dialog.findChild<QListWidget*>(QStringLiteral("snippetList"));
    auto* selectAllSnippets = dialog.findChild<QCheckBox*>(QStringLiteral("selectAllSnippetsCheckBox"));
    QVERIFY(snippetList->isEnabled());
    QVERIFY(selectAllSnippets->isEnabled());
}
