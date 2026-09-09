#include "sound_mind/studio/record_panel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace sound_mind::studio {

namespace {

const QString kSystemDefaultLabel = QObject::tr("(System Default)");

/// @brief Replaces `combo`'s items with a "(System Default)" entry
/// followed by `deviceNames` - see `LoopPanel`'s identical helper for why
/// device name (not display index) is what's preserved across a refresh.
void populateDeviceCombo(QComboBox* combo, const QStringList& deviceNames) {
    const QString previousSelection = combo->currentData().toString();

    combo->blockSignals(true);
    combo->clear();
    combo->addItem(kSystemDefaultLabel, QString());
    for (const QString& name : deviceNames) {
        combo->addItem(name, name);
    }
    const int previousIndex = combo->findData(previousSelection);
    combo->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
    combo->blockSignals(false);
}

}  // namespace

RecordPanel::RecordPanel(QWidget* parent) : QDockWidget(tr("Record"), parent) {
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    toggleButton_ = new QPushButton(tr("Start Recording"), container);
    toggleButton_->setObjectName(QStringLiteral("recordToggleButton"));
    toggleButton_->setCheckable(true);
    connect(toggleButton_, &QPushButton::clicked, this, &RecordPanel::toggleRequested);
    root->addWidget(toggleButton_);

    auto* inputRow = new QHBoxLayout();
    inputRow->addWidget(new QLabel(tr("Input:"), container));
    inputDeviceCombo_ = new QComboBox(container);
    inputDeviceCombo_->setObjectName(QStringLiteral("inputDeviceCombo"));
    connect(inputDeviceCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit inputDeviceChanged(inputDeviceCombo_->itemData(index).toString());
    });
    inputRow->addWidget(inputDeviceCombo_, 1);
    root->addLayout(inputRow);

    root->addStretch();

    populateDeviceCombo(inputDeviceCombo_, {});

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);
}

void RecordPanel::setRecording(bool recording) {
    toggleButton_->setChecked(recording);
    toggleButton_->setText(recording ? tr("Stop Recording") : tr("Start Recording"));
    inputDeviceCombo_->setEnabled(!recording);
}

void RecordPanel::setInputDevices(const QStringList& deviceNames) {
    populateDeviceCombo(inputDeviceCombo_, deviceNames);
}

}  // namespace sound_mind::studio
