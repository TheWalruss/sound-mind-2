#include "sound_mind/studio/audio_snippet_picker_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QListWidget>
#include <QVBoxLayout>

namespace sound_mind::studio {

namespace {

/// @brief Formats a duration in seconds as "M:SS.s" (matching the
/// codebase's other transport-position displays' general style).
QString formatTimestamp(double seconds) {
    const int minutes = static_cast<int>(seconds) / 60;
    const double remainingSeconds = seconds - static_cast<double>(minutes) * 60.0;
    return QStringLiteral("%1:%2")
        .arg(minutes)
        .arg(remainingSeconds, 4, 'f', 1, QLatin1Char('0'));
}

}  // namespace

AudioSnippetPickerDialog::AudioSnippetPickerDialog(const std::vector<RowData>& snippets, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Choose Snippets to Import"));

    auto* root = new QVBoxLayout(this);

    auto* offsetForm = new QFormLayout();
    offsetSpinBox_ = new QDoubleSpinBox(this);
    offsetSpinBox_->setObjectName(QStringLiteral("offsetSpinBox"));
    offsetSpinBox_->setRange(0.0, 3600.0);
    offsetSpinBox_->setSingleStep(0.5);
    offsetSpinBox_->setDecimals(2);
    offsetSpinBox_->setSuffix(tr(" s"));
    offsetSpinBox_->setToolTip(
        tr("Discards this much audio off the very start before splitting, so the grid restarts fresh at that "
           "point - lets a point of interest land exactly on a snippet boundary."));
    connect(offsetSpinBox_, &QDoubleSpinBox::valueChanged, this, &AudioSnippetPickerDialog::offsetChanged);
    offsetForm->addRow(tr("Offset:"), offsetSpinBox_);
    root->addLayout(offsetForm);

    auto* selectAllCheckBox = new QCheckBox(tr("Select All"), this);
    selectAllCheckBox->setObjectName(QStringLiteral("selectAllCheckBox"));
    selectAllCheckBox->setChecked(true);
    connect(selectAllCheckBox, &QCheckBox::toggled, this, &AudioSnippetPickerDialog::setAllChecked);
    root->addWidget(selectAllCheckBox);

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("snippetList"));
    root->addWidget(list_);
    populateList(snippets);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->setObjectName(QStringLiteral("buttonBox"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox);
}

std::vector<std::size_t> AudioSnippetPickerDialog::selectedIndices() const {
    std::vector<std::size_t> result;
    for (int i = 0; i < list_->count(); ++i) {
        const QListWidgetItem* item = list_->item(i);
        if (item->checkState() == Qt::Checked) {
            result.push_back(static_cast<std::size_t>(item->data(Qt::UserRole).toULongLong()));
        }
    }
    return result;
}

double AudioSnippetPickerDialog::offsetSeconds() const { return offsetSpinBox_->value(); }

void AudioSnippetPickerDialog::setSnippets(const std::vector<RowData>& snippets) { populateList(snippets); }

void AudioSnippetPickerDialog::populateList(const std::vector<RowData>& snippets) {
    list_->clear();
    for (const RowData& snippet : snippets) {
        auto* item = new QListWidgetItem(
            tr("Snippet %1: %2 - %3")
                .arg(snippet.index, 4, 10, QLatin1Char('0'))
                .arg(formatTimestamp(snippet.startSeconds), formatTimestamp(snippet.endSeconds)),
            list_);
        item->setData(Qt::UserRole, static_cast<qulonglong>(snippet.index));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);  // every row checked by default - see the class docs.
    }
}

void AudioSnippetPickerDialog::setAllChecked(bool checked) {
    for (int i = 0; i < list_->count(); ++i) {
        list_->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
}

}  // namespace sound_mind::studio
