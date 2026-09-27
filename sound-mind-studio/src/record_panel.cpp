#include "sound_mind/studio/record_panel.h"

#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace sound_mind::studio {

RecordPanel::RecordPanel(QWidget* parent) : QDockWidget(tr("Record"), parent) {
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    toggleButton_ = new QPushButton(tr("Start Recording"), container);
    toggleButton_->setObjectName(QStringLiteral("recordToggleButton"));
    toggleButton_->setCheckable(true);
    connect(toggleButton_, &QPushButton::clicked, this, &RecordPanel::toggleRequested);
    root->addWidget(toggleButton_);

    auto* configureDevicesButton = new QPushButton(tr("Configure Devices"), container);
    configureDevicesButton->setObjectName(QStringLiteral("configureDevicesButton"));
    connect(configureDevicesButton, &QPushButton::clicked, this, &RecordPanel::configureDevicesRequested);
    root->addWidget(configureDevicesButton);

    root->addStretch();

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);
}

void RecordPanel::setRecording(bool recording) {
    toggleButton_->setChecked(recording);
    toggleButton_->setText(recording ? tr("Stop Recording") : tr("Start Recording"));
}

}  // namespace sound_mind::studio
