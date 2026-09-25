#include "sound_mind/studio/gradient_editor_widget.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "sound_mind/studio/gradient_bar_widget.h"

namespace sound_mind::studio {

namespace {

/// @brief `GradientEditorWidget::setCutMode()`'s own silence floor - see
/// its docs, and `sound_mind::core::silenceGradient()`'s matching
/// constant.
constexpr float kSilenceFloorDb = -96.0f;

/// @brief A `[-96, 0]` dB spin box, matching `makeIntensitySpinBox()`'s
/// own precedent in `filter_configuration_panel.cpp` (the same range
/// `silenceGradient()`'s own floor and `color_mapping.cpp`'s own display
/// range already use).
QDoubleSpinBox* makeIntensitySpinBox(QWidget* parent, const QString& objectName) {
    auto* spinBox = new QDoubleSpinBox(parent);
    spinBox->setObjectName(objectName);
    spinBox->setRange(-96.0, 0.0);
    spinBox->setSingleStep(1.0);
    spinBox->setSuffix(QStringLiteral(" dB"));
    return spinBox;
}

/// @brief A `[0, 1]` opacity spin box, matching `makeOpacitySpinBox()`'s
/// own precedent in `filter_configuration_panel.cpp`.
QDoubleSpinBox* makeOpacitySpinBox(QWidget* parent, const QString& objectName) {
    auto* spinBox = new QDoubleSpinBox(parent);
    spinBox->setObjectName(objectName);
    spinBox->setRange(0.0, 1.0);
    spinBox->setSingleStep(0.05);
    spinBox->setDecimals(2);
    return spinBox;
}

}  // namespace

GradientEditorWidget::GradientEditorWidget(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    bar_ = new GradientBarWidget(this);
    bar_->setObjectName(QStringLiteral("gradientBar"));
    root->addWidget(bar_);
    connect(bar_, &GradientBarWidget::gradientChanged, this,
            [this](const sound_mind::core::Gradient& gradient) { emit gradientChanged(gradient); });
    connect(bar_, &GradientBarWidget::selectionChanged, this, &GradientEditorWidget::handleSelectionChanged);

    auto* valuesForm = new QFormLayout();
    leftIntensitySpinBox_ = makeIntensitySpinBox(this, QStringLiteral("gradientLeftIntensitySpinBox"));
    rightIntensitySpinBox_ = makeIntensitySpinBox(this, QStringLiteral("gradientRightIntensitySpinBox"));
    leftOpacitySpinBox_ = makeOpacitySpinBox(this, QStringLiteral("gradientLeftOpacitySpinBox"));
    rightOpacitySpinBox_ = makeOpacitySpinBox(this, QStringLiteral("gradientRightOpacitySpinBox"));
    valuesForm->addRow(tr("Left Intensity:"), leftIntensitySpinBox_);
    valuesForm->addRow(tr("Right Intensity:"), rightIntensitySpinBox_);
    valuesForm->addRow(tr("Left Opacity:"), leftOpacitySpinBox_);
    valuesForm->addRow(tr("Right Opacity:"), rightOpacitySpinBox_);
    root->addLayout(valuesForm);
    leftIntensityLabel_ = qobject_cast<QLabel*>(valuesForm->labelForField(leftIntensitySpinBox_));
    rightIntensityLabel_ = qobject_cast<QLabel*>(valuesForm->labelForField(rightIntensitySpinBox_));
    leftOpacityLabel_ = qobject_cast<QLabel*>(valuesForm->labelForField(leftOpacitySpinBox_));
    rightOpacityLabel_ = qobject_cast<QLabel*>(valuesForm->labelForField(rightOpacitySpinBox_));

    // Link Channels - each spin box mirrors onto its own opposite-channel
    // counterpart first (only while checked), then both feed the same
    // applyEditedStopValues() - see this class's own docs.
    linkChannelsCheckBox_ = new QCheckBox(tr("Link Channels"), this);
    linkChannelsCheckBox_->setObjectName(QStringLiteral("gradientLinkChannelsCheckBox"));
    connect(linkChannelsCheckBox_, &QCheckBox::toggled, this, [this](bool linked) { bar_->setLinkChannels(linked); });

    auto* bottomRow = new QHBoxLayout();
    bottomRow->addWidget(linkChannelsCheckBox_);
    deleteStopButton_ = new QPushButton(tr("Delete Stop"), this);
    deleteStopButton_->setObjectName(QStringLiteral("gradientDeleteStopButton"));
    connect(deleteStopButton_, &QPushButton::clicked, this, [this]() { bar_->removeSelectedStop(); });
    bottomRow->addWidget(deleteStopButton_);
    root->addLayout(bottomRow);

    connect(leftIntensitySpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (linkChannelsCheckBox_->isChecked()) {
            const QSignalBlocker blocker(rightIntensitySpinBox_);
            rightIntensitySpinBox_->setValue(value);
        }
        applyEditedStopValues();
    });
    connect(rightIntensitySpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (linkChannelsCheckBox_->isChecked()) {
            const QSignalBlocker blocker(leftIntensitySpinBox_);
            leftIntensitySpinBox_->setValue(value);
        }
        applyEditedStopValues();
    });
    connect(leftOpacitySpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (linkChannelsCheckBox_->isChecked()) {
            const QSignalBlocker blocker(rightOpacitySpinBox_);
            rightOpacitySpinBox_->setValue(value);
        }
        applyEditedStopValues();
    });
    connect(rightOpacitySpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (linkChannelsCheckBox_->isChecked()) {
            const QSignalBlocker blocker(leftOpacitySpinBox_);
            leftOpacitySpinBox_->setValue(value);
        }
        applyEditedStopValues();
    });

    handleSelectionChanged(bar_->selectedIndex());
}

const sound_mind::core::Gradient& GradientEditorWidget::gradient() const noexcept { return bar_->gradient(); }

void GradientEditorWidget::setGradient(sound_mind::core::Gradient gradient) {
    bar_->setGradient(std::move(gradient));
    const QSignalBlocker linkChannelsBlocker(linkChannelsCheckBox_);
    linkChannelsCheckBox_->setChecked(bar_->gradient().linkChannels());
    handleSelectionChanged(bar_->selectedIndex());
}

void GradientEditorWidget::handleSelectionChanged(std::size_t index) {
    const auto& stops = bar_->gradient().stops();
    const auto& stop = stops.at(index);

    const QSignalBlocker leftIntensityBlocker(leftIntensitySpinBox_);
    const QSignalBlocker rightIntensityBlocker(rightIntensitySpinBox_);
    const QSignalBlocker leftOpacityBlocker(leftOpacitySpinBox_);
    const QSignalBlocker rightOpacityBlocker(rightOpacitySpinBox_);
    leftIntensitySpinBox_->setValue(stop.leftIntensity);
    rightIntensitySpinBox_->setValue(stop.rightIntensity);
    leftOpacitySpinBox_->setValue(stop.leftOpacity);
    rightOpacitySpinBox_->setValue(stop.rightOpacity);

    // Endpoints (t=0, t=1) can't be removed - see GradientBarWidget::
    // removeSelectedStop()'s own docs.
    deleteStopButton_->setEnabled(index != 0 && index + 1 != stops.size());
}

void GradientEditorWidget::applyEditedStopValues() {
    sound_mind::core::GradientStop values = bar_->gradient().stops().at(bar_->selectedIndex());
    // Cut mode forces intensity to the silence floor regardless of
    // whatever the (hidden) intensity spin boxes still hold - see
    // setCutMode()'s own docs.
    values.leftIntensity = cutMode_ ? kSilenceFloorDb : static_cast<float>(leftIntensitySpinBox_->value());
    values.rightIntensity = cutMode_ ? kSilenceFloorDb : static_cast<float>(rightIntensitySpinBox_->value());
    values.leftOpacity = static_cast<float>(leftOpacitySpinBox_->value());
    values.rightOpacity = static_cast<float>(rightOpacitySpinBox_->value());
    // setSelectedStopValues() itself emits gradientChanged(), forwarded
    // by this class's own constructor-time connection to bar_ - no
    // separate emit needed here.
    bar_->setSelectedStopValues(values);
}

void GradientEditorWidget::setCutMode(bool cutMode) {
    cutMode_ = cutMode;
    updateIntensityFieldVisibility();
    if (leftOpacityLabel_ != nullptr) {
        leftOpacityLabel_->setText(cutMode ? tr("Left Cut:") : tr("Left Opacity:"));
    }
    if (rightOpacityLabel_ != nullptr) {
        rightOpacityLabel_->setText(cutMode ? tr("Right Cut:") : tr("Right Opacity:"));
    }
}

void GradientEditorWidget::setIntensityVisible(bool visible) {
    intensityVisible_ = visible;
    updateIntensityFieldVisibility();
}

void GradientEditorWidget::updateIntensityFieldVisibility() {
    const bool show = intensityVisible_ && !cutMode_;
    leftIntensitySpinBox_->setVisible(show);
    rightIntensitySpinBox_->setVisible(show);
    if (leftIntensityLabel_ != nullptr) {
        leftIntensityLabel_->setVisible(show);
    }
    if (rightIntensityLabel_ != nullptr) {
        rightIntensityLabel_->setVisible(show);
    }
}

}  // namespace sound_mind::studio
