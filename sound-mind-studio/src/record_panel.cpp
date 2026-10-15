#include "sound_mind/studio/record_panel.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
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

    auto* targetForm = new QFormLayout();
    targetCombo_ = new QComboBox(container);
    targetCombo_->setObjectName(QStringLiteral("recordTargetCombo"));
    targetCombo_->setToolTip(tr("Record into a new layer, or over an existing recorded layer (replacing its content)"));
    targetCombo_->addItem(tr("New recording layer"));
    targetForm->addRow(tr("Record into:"), targetCombo_);
    nameEdit_ = new QLineEdit(container);
    nameEdit_->setObjectName(QStringLiteral("recordNameEdit"));
    nameEdit_->setPlaceholderText(tr("Recording"));
    nameEdit_->setText(tr("Recording"));
    targetForm->addRow(tr("Name:"), nameEdit_);
    nameLabel_ = qobject_cast<QLabel*>(targetForm->labelForField(nameEdit_));
    root->addLayout(targetForm);

    liveViewCheckBox_ = new QCheckBox(tr("Live-encoding view"), container);
    liveViewCheckBox_->setObjectName(QStringLiteral("liveViewCheckBox"));
    liveViewCheckBox_->setToolTip(tr("Show the take being recorded, encoded live, drawn on top of the canvas (not at its place in the layer stack, which would be too slow while recording)"));
    connect(liveViewCheckBox_, &QCheckBox::toggled, this, &RecordPanel::liveViewToggled);
    root->addWidget(liveViewCheckBox_);
    connect(targetCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const bool newLayer = index <= 0;
        nameEdit_->setVisible(newLayer);
        if (nameLabel_ != nullptr) {
            nameLabel_->setVisible(newLayer);
        }
    });

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
    targetCombo_->setEnabled(!recording);
    nameEdit_->setEnabled(!recording);
}

void RecordPanel::setRecordedLayers(const std::vector<std::pair<sound_mind::core::LayerId, QString>>& layers) {
    const auto previous = targetLayerId();
    {
        const QSignalBlocker blocker(targetCombo_);
        while (targetCombo_->count() > 1) {
            targetCombo_->removeItem(1);
        }
        for (const auto& [id, name] : layers) {
            targetCombo_->addItem(name, QVariant::fromValue(static_cast<qulonglong>(id)));
        }
        const int index =
            previous.has_value() ? targetCombo_->findData(QVariant::fromValue(static_cast<qulonglong>(*previous))) : 0;
        targetCombo_->setCurrentIndex(std::max(index, 0));
    }
    emit targetCombo_->currentIndexChanged(targetCombo_->currentIndex());
}

std::optional<sound_mind::core::LayerId> RecordPanel::targetLayerId() const {
    const QVariant data = targetCombo_->currentData();
    if (!data.isValid()) {
        return std::nullopt;
    }
    return static_cast<sound_mind::core::LayerId>(data.toULongLong());
}

QString RecordPanel::newLayerName() const {
    const QString name = nameEdit_->text().trimmed();
    return name.isEmpty() ? tr("Recording") : name;
}

}  // namespace sound_mind::studio
