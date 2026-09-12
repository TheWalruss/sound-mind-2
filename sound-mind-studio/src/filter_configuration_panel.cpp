#include "sound_mind/studio/filter_configuration_panel.h"

#include <array>
#include <utility>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

namespace sound_mind::studio {

namespace {

using sound_mind::core::FilterType;
using sound_mind::core::GradientStop;

/// @brief The `FilterType`s with a real algorithm behind them, in
/// `docs/sound-mind-design.md`'s own family order - see this panel's own
/// docs for why `ToneCurve` isn't listed yet.
constexpr std::array<std::pair<FilterType, const char*>, 5> kSelectableFilterTypes{{
    {FilterType::UniformBlur, "Uniform Blur"},
    {FilterType::EdgePreservingBlur, "Edge-Preserving Blur"},
    {FilterType::DirectionalBlur, "Directional Blur"},
    {FilterType::Sharpen, "Sharpen"},
    {FilterType::FrequencyAxisGradient, "Frequency-Axis Gradient"},
}};

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

    auto* typeForm = new QFormLayout();
    filterTypeCombo_ = new QComboBox(container);
    filterTypeCombo_->setObjectName(QStringLiteral("filterTypeCombo"));
    for (const auto& [type, name] : kSelectableFilterTypes) {
        filterTypeCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(type)));
    }
    connect(filterTypeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        config_.setType(static_cast<FilterType>(filterTypeCombo_->itemData(index).toInt()));
        updateVisibleGroup();
        emitConfigChanged();
    });
    typeForm->addRow(tr("Filter Type:"), filterTypeCombo_);
    root->addLayout(typeForm);

    auto* frequencyGroupContainer = new QWidget(container);
    frequencyGroupContainer->setObjectName(QStringLiteral("frequencyAxisGradientSection"));
    auto* frequencyLayout = new QVBoxLayout(frequencyGroupContainer);
    frequencyLayout->setContentsMargins(0, 0, 0, 0);
    frequencyAxisGradientSection_ = frequencyGroupContainer;

    frequencyGradientLabel_ = new QLabel(tr("Frequency-Axis Gradient"), frequencyGroupContainer);
    frequencyGradientLabel_->setObjectName(QStringLiteral("frequencyGradientLabel"));
    frequencyLayout->addWidget(frequencyGradientLabel_);

    auto* startGroup = new QGroupBox(tr("Start (t=0, lowest frequency)"), frequencyGroupContainer);
    auto* startForm = new QFormLayout(startGroup);
    startLeftIntensitySpinBox_ = makeIntensitySpinBox(startGroup, QStringLiteral("startLeftIntensitySpinBox"));
    startForm->addRow(tr("Left Intensity:"), startLeftIntensitySpinBox_);
    startLeftOpacitySpinBox_ = makeOpacitySpinBox(startGroup, QStringLiteral("startLeftOpacitySpinBox"));
    startForm->addRow(tr("Left Opacity:"), startLeftOpacitySpinBox_);
    startRightIntensitySpinBox_ = makeIntensitySpinBox(startGroup, QStringLiteral("startRightIntensitySpinBox"));
    startForm->addRow(tr("Right Intensity:"), startRightIntensitySpinBox_);
    startRightOpacitySpinBox_ = makeOpacitySpinBox(startGroup, QStringLiteral("startRightOpacitySpinBox"));
    startForm->addRow(tr("Right Opacity:"), startRightOpacitySpinBox_);
    frequencyLayout->addWidget(startGroup);

    auto* endGroup = new QGroupBox(tr("End (t=1, highest frequency)"), frequencyGroupContainer);
    auto* endForm = new QFormLayout(endGroup);
    endLeftIntensitySpinBox_ = makeIntensitySpinBox(endGroup, QStringLiteral("endLeftIntensitySpinBox"));
    endForm->addRow(tr("Left Intensity:"), endLeftIntensitySpinBox_);
    endLeftOpacitySpinBox_ = makeOpacitySpinBox(endGroup, QStringLiteral("endLeftOpacitySpinBox"));
    endForm->addRow(tr("Left Opacity:"), endLeftOpacitySpinBox_);
    endRightIntensitySpinBox_ = makeIntensitySpinBox(endGroup, QStringLiteral("endRightIntensitySpinBox"));
    endForm->addRow(tr("Right Intensity:"), endRightIntensitySpinBox_);
    endRightOpacitySpinBox_ = makeOpacitySpinBox(endGroup, QStringLiteral("endRightOpacitySpinBox"));
    endForm->addRow(tr("Right Opacity:"), endRightOpacitySpinBox_);
    frequencyLayout->addWidget(endGroup);

    root->addWidget(frequencyGroupContainer);

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

    uniformBlurGroup_ = new QGroupBox(tr("Uniform Blur"), container);
    uniformBlurGroup_->setObjectName(QStringLiteral("uniformBlurGroup"));
    auto* uniformBlurForm = new QFormLayout(uniformBlurGroup_);
    blurSigmaSpinBox_ = new QDoubleSpinBox(uniformBlurGroup_);
    blurSigmaSpinBox_->setObjectName(QStringLiteral("blurSigmaSpinBox"));
    blurSigmaSpinBox_->setRange(0.1, 50.0);
    blurSigmaSpinBox_->setSingleStep(0.1);
    blurSigmaSpinBox_->setDecimals(1);
    connect(blurSigmaSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setBlurSigma(static_cast<float>(value));
        emitConfigChanged();
    });
    uniformBlurForm->addRow(tr("Sigma:"), blurSigmaSpinBox_);
    root->addWidget(uniformBlurGroup_);

    edgePreservingBlurGroup_ = new QGroupBox(tr("Edge-Preserving Blur"), container);
    edgePreservingBlurGroup_->setObjectName(QStringLiteral("edgePreservingBlurGroup"));
    auto* edgePreservingBlurForm = new QFormLayout(edgePreservingBlurGroup_);
    medianSizeSpinBox_ = new QSpinBox(edgePreservingBlurGroup_);
    medianSizeSpinBox_->setObjectName(QStringLiteral("medianSizeSpinBox"));
    medianSizeSpinBox_->setRange(3, 31);
    medianSizeSpinBox_->setToolTip(
        tr("The median window's own size, in bins/columns - an even value behaves as if rounded up to the next "
           "odd one."));
    connect(medianSizeSpinBox_, &QSpinBox::valueChanged, this, [this](int value) {
        config_.setMedianSize(value);
        emitConfigChanged();
    });
    edgePreservingBlurForm->addRow(tr("Size:"), medianSizeSpinBox_);
    root->addWidget(edgePreservingBlurGroup_);

    directionalBlurGroup_ = new QGroupBox(tr("Directional Blur"), container);
    directionalBlurGroup_->setObjectName(QStringLiteral("directionalBlurGroup"));
    auto* directionalBlurForm = new QFormLayout(directionalBlurGroup_);
    directionalBlurLengthSpinBox_ = new QSpinBox(directionalBlurGroup_);
    directionalBlurLengthSpinBox_->setObjectName(QStringLiteral("directionalBlurLengthSpinBox"));
    directionalBlurLengthSpinBox_->setRange(1, 200);
    connect(directionalBlurLengthSpinBox_, &QSpinBox::valueChanged, this, [this](int value) {
        config_.setDirectionalBlurLength(value);
        emitConfigChanged();
    });
    directionalBlurForm->addRow(tr("Length:"), directionalBlurLengthSpinBox_);
    directionalBlurAngleSpinBox_ = new QDoubleSpinBox(directionalBlurGroup_);
    directionalBlurAngleSpinBox_->setObjectName(QStringLiteral("directionalBlurAngleSpinBox"));
    directionalBlurAngleSpinBox_->setRange(0.0, 360.0);
    directionalBlurAngleSpinBox_->setSingleStep(1.0);
    directionalBlurAngleSpinBox_->setDecimals(1);
    directionalBlurAngleSpinBox_->setSuffix(QStringLiteral("°"));
    directionalBlurAngleSpinBox_->setToolTip(
        tr("0° blurs along the time axis (columns), 90° along the frequency axis (bins)."));
    connect(directionalBlurAngleSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setDirectionalBlurAngleDegrees(static_cast<float>(value));
        emitConfigChanged();
    });
    directionalBlurForm->addRow(tr("Angle:"), directionalBlurAngleSpinBox_);
    root->addWidget(directionalBlurGroup_);

    sharpenGroup_ = new QGroupBox(tr("Sharpen"), container);
    sharpenGroup_->setObjectName(QStringLiteral("sharpenGroup"));
    auto* sharpenForm = new QFormLayout(sharpenGroup_);
    sharpenAmountSpinBox_ = new QDoubleSpinBox(sharpenGroup_);
    sharpenAmountSpinBox_->setObjectName(QStringLiteral("sharpenAmountSpinBox"));
    sharpenAmountSpinBox_->setRange(0.1, 5.0);
    sharpenAmountSpinBox_->setSingleStep(0.1);
    sharpenAmountSpinBox_->setDecimals(1);
    connect(sharpenAmountSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setSharpenAmount(static_cast<float>(value));
        emitConfigChanged();
    });
    sharpenForm->addRow(tr("Amount:"), sharpenAmountSpinBox_);
    root->addWidget(sharpenGroup_);

    root->addStretch();

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);

    updateVisibleGroup();
}

void FilterConfigurationPanel::emitConfigChanged() { emit filterConfigurationChanged(config_); }

void FilterConfigurationPanel::updateVisibleGroup() {
    const FilterType type = config_.type();
    frequencyAxisGradientSection_->setVisible(type == FilterType::FrequencyAxisGradient);
    uniformBlurGroup_->setVisible(type == FilterType::UniformBlur);
    edgePreservingBlurGroup_->setVisible(type == FilterType::EdgePreservingBlur);
    directionalBlurGroup_->setVisible(type == FilterType::DirectionalBlur);
    sharpenGroup_->setVisible(type == FilterType::Sharpen);
}

void FilterConfigurationPanel::setFilterConfiguration(const sound_mind::core::FilterConfiguration& config) {
    config_ = config;

    // QSignalBlocker on every control - see ToolConfigurationPanel::
    // setToolConfiguration()'s own docs for why: only the *display*
    // should change here, not re-trigger each control's own change
    // handler (which would both redundantly re-set config_ to the value
    // it already has and spuriously emit filterConfigurationChanged()).
    const QSignalBlocker filterTypeBlocker(filterTypeCombo_);
    const QSignalBlocker startLeftIntensityBlocker(startLeftIntensitySpinBox_);
    const QSignalBlocker startLeftOpacityBlocker(startLeftOpacitySpinBox_);
    const QSignalBlocker startRightIntensityBlocker(startRightIntensitySpinBox_);
    const QSignalBlocker startRightOpacityBlocker(startRightOpacitySpinBox_);
    const QSignalBlocker endLeftIntensityBlocker(endLeftIntensitySpinBox_);
    const QSignalBlocker endLeftOpacityBlocker(endLeftOpacitySpinBox_);
    const QSignalBlocker endRightIntensityBlocker(endRightIntensitySpinBox_);
    const QSignalBlocker endRightOpacityBlocker(endRightOpacitySpinBox_);
    const QSignalBlocker blurSigmaBlocker(blurSigmaSpinBox_);
    const QSignalBlocker medianSizeBlocker(medianSizeSpinBox_);
    const QSignalBlocker directionalBlurLengthBlocker(directionalBlurLengthSpinBox_);
    const QSignalBlocker directionalBlurAngleBlocker(directionalBlurAngleSpinBox_);
    const QSignalBlocker sharpenAmountBlocker(sharpenAmountSpinBox_);

    // Falls back to index 0 for a stored type not on the list (ToneCurve -
    // not selectable here yet, see this class's own docs) without
    // mutating config_.type() itself, matching ToolConfigurationPanel's
    // own fallback for an out-of-list stored value.
    const int typeIndex = filterTypeCombo_->findData(QVariant::fromValue(static_cast<int>(config_.type())));
    filterTypeCombo_->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);

    const auto& stops = config_.frequencyGradient().stops();
    startLeftIntensitySpinBox_->setValue(stops.front().leftIntensity);
    startLeftOpacitySpinBox_->setValue(stops.front().leftOpacity);
    startRightIntensitySpinBox_->setValue(stops.front().rightIntensity);
    startRightOpacitySpinBox_->setValue(stops.front().rightOpacity);
    endLeftIntensitySpinBox_->setValue(stops.back().leftIntensity);
    endLeftOpacitySpinBox_->setValue(stops.back().leftOpacity);
    endRightIntensitySpinBox_->setValue(stops.back().rightIntensity);
    endRightOpacitySpinBox_->setValue(stops.back().rightOpacity);

    blurSigmaSpinBox_->setValue(config_.blurSigma());
    medianSizeSpinBox_->setValue(config_.medianSize());
    directionalBlurLengthSpinBox_->setValue(config_.directionalBlurLength());
    directionalBlurAngleSpinBox_->setValue(config_.directionalBlurAngleDegrees());
    sharpenAmountSpinBox_->setValue(config_.sharpenAmount());

    updateVisibleGroup();
}

}  // namespace sound_mind::studio
