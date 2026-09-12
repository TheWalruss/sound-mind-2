#include "sound_mind/studio/filter_configuration_panel.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>

namespace sound_mind::studio {

namespace {

using sound_mind::core::GradientStop;

/// @brief A `[-96, 0]` dB spin box, matching `silenceGradient()`'s own
/// floor and `color_mapping.cpp`'s own display range - the same range
/// every other dB-valued control in this codebase already uses.
QDoubleSpinBox* makeIntensitySpinBox(QWidget* parent, const QString& objectName) {
    auto* spinBox = new QDoubleSpinBox(parent);
    spinBox->setObjectName(objectName);
    spinBox->setRange(-96.0, 0.0);
    spinBox->setSingleStep(1.0);
    spinBox->setSuffix(QStringLiteral(" dB"));
    return spinBox;
}

/// @brief A `[0, 1]` opacity spin box.
QDoubleSpinBox* makeOpacitySpinBox(QWidget* parent, const QString& objectName) {
    auto* spinBox = new QDoubleSpinBox(parent);
    spinBox->setObjectName(objectName);
    spinBox->setRange(0.0, 1.0);
    spinBox->setSingleStep(0.05);
    spinBox->setDecimals(2);
    return spinBox;
}

}  // namespace

FilterConfigurationPanel::FilterConfigurationPanel(QWidget* parent)
    : QDockWidget(tr("Filter Configuration"), parent) {
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    frequencyGradientLabel_ = new QLabel(tr("Frequency-Axis Gradient"), container);
    frequencyGradientLabel_->setObjectName(QStringLiteral("frequencyGradientLabel"));
    root->addWidget(frequencyGradientLabel_);

    auto* startGroup = new QGroupBox(tr("Start (t=0, lowest frequency)"), container);
    auto* startForm = new QFormLayout(startGroup);
    startLeftIntensitySpinBox_ = makeIntensitySpinBox(startGroup, QStringLiteral("startLeftIntensitySpinBox"));
    startForm->addRow(tr("Left Intensity:"), startLeftIntensitySpinBox_);
    startLeftOpacitySpinBox_ = makeOpacitySpinBox(startGroup, QStringLiteral("startLeftOpacitySpinBox"));
    startForm->addRow(tr("Left Opacity:"), startLeftOpacitySpinBox_);
    startRightIntensitySpinBox_ = makeIntensitySpinBox(startGroup, QStringLiteral("startRightIntensitySpinBox"));
    startForm->addRow(tr("Right Intensity:"), startRightIntensitySpinBox_);
    startRightOpacitySpinBox_ = makeOpacitySpinBox(startGroup, QStringLiteral("startRightOpacitySpinBox"));
    startForm->addRow(tr("Right Opacity:"), startRightOpacitySpinBox_);
    root->addWidget(startGroup);

    auto* endGroup = new QGroupBox(tr("End (t=1, highest frequency)"), container);
    auto* endForm = new QFormLayout(endGroup);
    endLeftIntensitySpinBox_ = makeIntensitySpinBox(endGroup, QStringLiteral("endLeftIntensitySpinBox"));
    endForm->addRow(tr("Left Intensity:"), endLeftIntensitySpinBox_);
    endLeftOpacitySpinBox_ = makeOpacitySpinBox(endGroup, QStringLiteral("endLeftOpacitySpinBox"));
    endForm->addRow(tr("Left Opacity:"), endLeftOpacitySpinBox_);
    endRightIntensitySpinBox_ = makeIntensitySpinBox(endGroup, QStringLiteral("endRightIntensitySpinBox"));
    endForm->addRow(tr("Right Intensity:"), endRightIntensitySpinBox_);
    endRightOpacitySpinBox_ = makeOpacitySpinBox(endGroup, QStringLiteral("endRightOpacitySpinBox"));
    endForm->addRow(tr("Right Opacity:"), endRightOpacitySpinBox_);
    root->addWidget(endGroup);

    // Every spin box in a group re-reads all four of that group's own
    // current values and writes them back as one stop - simpler than
    // updating a single field in place, and no less correct, since
    // every control is always visible/current at once (unlike, say, a
    // multi-page wizard where only one field might be on-screen).
    const auto applyStart = [this]() {
        GradientStop stop = config_.frequencyGradient().stops().front();
        stop.leftIntensity = static_cast<float>(startLeftIntensitySpinBox_->value());
        stop.leftOpacity = static_cast<float>(startLeftOpacitySpinBox_->value());
        stop.rightIntensity = static_cast<float>(startRightIntensitySpinBox_->value());
        stop.rightOpacity = static_cast<float>(startRightOpacitySpinBox_->value());
        config_.frequencyGradient().setStopValues(0, stop);
        emitConfigChanged();
    };
    connect(startLeftIntensitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyStart](double) { applyStart(); });
    connect(startLeftOpacitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyStart](double) { applyStart(); });
    connect(startRightIntensitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyStart](double) { applyStart(); });
    connect(startRightOpacitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyStart](double) { applyStart(); });

    const auto applyEnd = [this]() {
        GradientStop stop = config_.frequencyGradient().stops().back();
        stop.leftIntensity = static_cast<float>(endLeftIntensitySpinBox_->value());
        stop.leftOpacity = static_cast<float>(endLeftOpacitySpinBox_->value());
        stop.rightIntensity = static_cast<float>(endRightIntensitySpinBox_->value());
        stop.rightOpacity = static_cast<float>(endRightOpacitySpinBox_->value());
        const auto& stops = config_.frequencyGradient().stops();
        config_.frequencyGradient().setStopValues(stops.size() - 1, stop);
        emitConfigChanged();
    };
    connect(endLeftIntensitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyEnd](double) { applyEnd(); });
    connect(endLeftOpacitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyEnd](double) { applyEnd(); });
    connect(endRightIntensitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyEnd](double) { applyEnd(); });
    connect(endRightOpacitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyEnd](double) { applyEnd(); });

    root->addStretch();

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);
}

void FilterConfigurationPanel::emitConfigChanged() { emit filterConfigurationChanged(config_); }

void FilterConfigurationPanel::setFilterConfiguration(const sound_mind::core::FilterConfiguration& config) {
    config_ = config;

    // QSignalBlocker on every spin box - see ToolConfigurationPanel::
    // setToolConfiguration()'s own docs for why: only the *display*
    // should change here, not re-trigger each control's own change
    // handler (which would both redundantly re-set config_ to the value
    // it already has and spuriously emit filterConfigurationChanged()).
    const QSignalBlocker startLeftIntensityBlocker(startLeftIntensitySpinBox_);
    const QSignalBlocker startLeftOpacityBlocker(startLeftOpacitySpinBox_);
    const QSignalBlocker startRightIntensityBlocker(startRightIntensitySpinBox_);
    const QSignalBlocker startRightOpacityBlocker(startRightOpacitySpinBox_);
    const QSignalBlocker endLeftIntensityBlocker(endLeftIntensitySpinBox_);
    const QSignalBlocker endLeftOpacityBlocker(endLeftOpacitySpinBox_);
    const QSignalBlocker endRightIntensityBlocker(endRightIntensitySpinBox_);
    const QSignalBlocker endRightOpacityBlocker(endRightOpacitySpinBox_);

    const auto& stops = config_.frequencyGradient().stops();
    startLeftIntensitySpinBox_->setValue(stops.front().leftIntensity);
    startLeftOpacitySpinBox_->setValue(stops.front().leftOpacity);
    startRightIntensitySpinBox_->setValue(stops.front().rightIntensity);
    startRightOpacitySpinBox_->setValue(stops.front().rightOpacity);
    endLeftIntensitySpinBox_->setValue(stops.back().leftIntensity);
    endLeftOpacitySpinBox_->setValue(stops.back().leftOpacity);
    endRightIntensitySpinBox_->setValue(stops.back().rightIntensity);
    endRightOpacitySpinBox_->setValue(stops.back().rightOpacity);
}

}  // namespace sound_mind::studio
