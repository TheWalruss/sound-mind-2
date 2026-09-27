#include "sound_mind/studio/midi_import_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

namespace sound_mind::studio {

namespace {

/// @brief Formats a duration in seconds as "M:SS.s" - a local copy of
///        AudioSnippetPickerDialog's own identical helper (a different
///        translation unit).
QString formatTimestamp(double seconds) {
    const int minutes = static_cast<int>(seconds) / 60;
    const double remainingSeconds = seconds - static_cast<double>(minutes) * 60.0;
    return QStringLiteral("%1:%2").arg(minutes).arg(remainingSeconds, 4, 'f', 1, QLatin1Char('0'));
}

}  // namespace

MidiImportDialog::MidiImportDialog(const std::vector<sound_mind::core::MidiChannelNotes>& channels,
                                    const std::vector<AudioSnippetPickerDialog::RowData>& snippets, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Import MIDI"));

    auto* root = new QVBoxLayout(this);

    root->addWidget(new QLabel(tr("Channels:"), this));
    auto* selectAllChannelsCheckBox = new QCheckBox(tr("Select All Channels"), this);
    selectAllChannelsCheckBox->setObjectName(QStringLiteral("selectAllChannelsCheckBox"));
    selectAllChannelsCheckBox->setChecked(true);
    connect(selectAllChannelsCheckBox, &QCheckBox::toggled, this, &MidiImportDialog::setAllChannelsChecked);
    root->addWidget(selectAllChannelsCheckBox);

    channelList_ = new QListWidget(this);
    channelList_->setObjectName(QStringLiteral("channelList"));
    for (const auto& channel : channels) {
        auto* item = new QListWidgetItem(
            tr("Channel %1: %2 (%3 notes)")
                .arg(channel.channelNumber)
                .arg(QString::fromStdString(channel.instrumentName))
                .arg(channel.notes.size()),
            channelList_);
        item->setData(Qt::UserRole, channel.channelNumber);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
    root->addWidget(channelList_);

    separateLayerCheckBox_ = new QCheckBox(tr("Separate layer per channel"), this);
    separateLayerCheckBox_->setObjectName(QStringLiteral("separateLayerCheckBox"));
    separateLayerCheckBox_->setChecked(true);
    root->addWidget(separateLayerCheckBox_);

    wholeFileCheckBox_ = new QCheckBox(tr("Import whole file (don't split into snippets)"), this);
    wholeFileCheckBox_->setObjectName(QStringLiteral("wholeFileCheckBox"));
    wholeFileCheckBox_->setChecked(true);
    wholeFileCheckBox_->setToolTip(
        tr("Import every selected channel's entire content as-is. Uncheck to choose which project-length "
           "snippet(s) of a longer file to import instead."));
    connect(wholeFileCheckBox_, &QCheckBox::toggled, this,
            [this](bool wholeFileChecked) { setSnippetControlsEnabled(!wholeFileChecked); });
    root->addWidget(wholeFileCheckBox_);

    root->addWidget(new QLabel(tr("Snippets:"), this));
    selectAllSnippetsCheckBox_ = new QCheckBox(tr("Select All Snippets"), this);
    selectAllSnippetsCheckBox_->setObjectName(QStringLiteral("selectAllSnippetsCheckBox"));
    selectAllSnippetsCheckBox_->setChecked(true);
    connect(selectAllSnippetsCheckBox_, &QCheckBox::toggled, this, &MidiImportDialog::setAllSnippetsChecked);
    root->addWidget(selectAllSnippetsCheckBox_);

    snippetList_ = new QListWidget(this);
    snippetList_->setObjectName(QStringLiteral("snippetList"));
    for (const auto& snippet : snippets) {
        auto* item = new QListWidgetItem(tr("Snippet %1: %2 - %3")
                                              .arg(snippet.index, 4, 10, QLatin1Char('0'))
                                              .arg(formatTimestamp(snippet.startSeconds),
                                                   formatTimestamp(snippet.endSeconds)),
                                          snippetList_);
        item->setData(Qt::UserRole, static_cast<qulonglong>(snippet.index));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
    root->addWidget(snippetList_);
    setSnippetControlsEnabled(!wholeFileCheckBox_->isChecked());

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->setObjectName(QStringLiteral("buttonBox"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox);
}

std::vector<int> MidiImportDialog::selectedChannelNumbers() const {
    std::vector<int> result;
    for (int i = 0; i < channelList_->count(); ++i) {
        const QListWidgetItem* item = channelList_->item(i);
        if (item->checkState() == Qt::Checked) {
            result.push_back(item->data(Qt::UserRole).toInt());
        }
    }
    return result;
}

std::vector<std::size_t> MidiImportDialog::selectedSnippetIndices() const {
    std::vector<std::size_t> result;
    for (int i = 0; i < snippetList_->count(); ++i) {
        const QListWidgetItem* item = snippetList_->item(i);
        if (item->checkState() == Qt::Checked) {
            result.push_back(static_cast<std::size_t>(item->data(Qt::UserRole).toULongLong()));
        }
    }
    return result;
}

bool MidiImportDialog::separateLayerPerChannel() const { return separateLayerCheckBox_->isChecked(); }

bool MidiImportDialog::wholeFile() const { return wholeFileCheckBox_->isChecked(); }

void MidiImportDialog::setAllChannelsChecked(bool checked) {
    for (int i = 0; i < channelList_->count(); ++i) {
        channelList_->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
}

void MidiImportDialog::setAllSnippetsChecked(bool checked) {
    for (int i = 0; i < snippetList_->count(); ++i) {
        snippetList_->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
}

void MidiImportDialog::setSnippetControlsEnabled(bool enabled) {
    snippetList_->setEnabled(enabled);
    selectAllSnippetsCheckBox_->setEnabled(enabled);
}

}  // namespace sound_mind::studio
