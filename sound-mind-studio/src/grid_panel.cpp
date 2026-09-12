#include "sound_mind/studio/grid_panel.h"

#include <array>
#include <utility>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStringList>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

namespace sound_mind::studio {

namespace {

/// @brief Every `Qt::PenStyle` this panel offers, paired with its
/// display name - just the three a hand-drawn reference line reasonably
/// needs (a continuous line, an evenly-dashed one, and a fine dotted
/// one), not every style `Qt::PenStyle` itself defines.
constexpr std::array<std::pair<Qt::PenStyle, const char*>, 3> kDashStyles{{
    {Qt::SolidLine, "Solid"},
    {Qt::DashLine, "Dash"},
    {Qt::DotLine, "Dot"},
}};

/// @brief Every `TimingGridMode` paired with its display name.
constexpr std::array<std::pair<TimingGridMode, const char*>, 3> kTimingGridModes{{
    {TimingGridMode::Off, "Off"},
    {TimingGridMode::Interval, "Fixed Interval"},
    {TimingGridMode::Tempo, "Tempo"},
}};

/// @brief Every note-value subdivision the Timing Grid's own Tempo mode
/// offers, paired with its own fraction of a quarter-note beat - down to
/// a thirty-second note, matching `docs/sound-mind-design.md`'s own
/// "subdivisions down to the finest rhythmic value in use" phrasing with
/// a fixed, musically-legible list rather than an open-ended spin box.
constexpr std::array<std::pair<double, const char*>, 6> kTempoSubdivisions{{
    {4.0, "Whole"},
    {2.0, "Half"},
    {1.0, "Quarter"},
    {0.5, "Eighth"},
    {0.25, "Sixteenth"},
    {0.125, "Thirty-second"},
}};

}  // namespace

GridPanel::GridPanel(QWidget* parent) : QDockWidget(tr("Grid"), parent) {
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    auto* axisForm = new QFormLayout();

    verticalAxisCombo_ = new QComboBox(container);
    verticalAxisCombo_->setObjectName(QStringLiteral("verticalAxisCombo"));
    verticalAxisCombo_->addItem(tr("Off"), QVariant::fromValue(static_cast<int>(VerticalAxisLabelMode::Off)));
    verticalAxisCombo_->addItem(tr("Hz"), QVariant::fromValue(static_cast<int>(VerticalAxisLabelMode::Hertz)));
    verticalAxisCombo_->addItem(tr("Notes"), QVariant::fromValue(static_cast<int>(VerticalAxisLabelMode::Notes)));
    verticalAxisCombo_->addItem(tr("Bin index"),
                                 QVariant::fromValue(static_cast<int>(VerticalAxisLabelMode::BinIndex)));
    connect(verticalAxisCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        verticalAxisLabelMode_ = static_cast<VerticalAxisLabelMode>(verticalAxisCombo_->itemData(index).toInt());
        emit verticalAxisLabelModeChanged(verticalAxisLabelMode_);
    });
    axisForm->addRow(tr("Vertical axis (frequency):"), verticalAxisCombo_);

    horizontalAxisCombo_ = new QComboBox(container);
    horizontalAxisCombo_->setObjectName(QStringLiteral("horizontalAxisCombo"));
    horizontalAxisCombo_->addItem(tr("Off"), QVariant::fromValue(static_cast<int>(HorizontalAxisLabelMode::Off)));
    horizontalAxisCombo_->addItem(tr("Seconds"),
                                   QVariant::fromValue(static_cast<int>(HorizontalAxisLabelMode::Seconds)));
    horizontalAxisCombo_->addItem(
        tr("Milliseconds"), QVariant::fromValue(static_cast<int>(HorizontalAxisLabelMode::Milliseconds)));
    horizontalAxisCombo_->addItem(tr("Frame index"),
                                   QVariant::fromValue(static_cast<int>(HorizontalAxisLabelMode::FrameIndex)));
    connect(horizontalAxisCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        horizontalAxisLabelMode_ =
            static_cast<HorizontalAxisLabelMode>(horizontalAxisCombo_->itemData(index).toInt());
        emit horizontalAxisLabelModeChanged(horizontalAxisLabelMode_);
    });
    axisForm->addRow(tr("Horizontal axis (time):"), horizontalAxisCombo_);
    root->addLayout(axisForm);

    // --- Frequency Grid -----------------------------------------------
    auto* frequencyGroup = new QGroupBox(tr("Frequency Grid"), container);
    auto* frequencyForm = new QFormLayout(frequencyGroup);

    noteGridCheckBox_ = new QCheckBox(tr("Note grid"), frequencyGroup);
    noteGridCheckBox_->setObjectName(QStringLiteral("noteGridCheckBox"));
    connect(noteGridCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        frequencyGridConfig_.noteGridEnabled = checked;
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(noteGridCheckBox_);

    harmonicSeriesCheckBox_ = new QCheckBox(tr("Harmonic series"), frequencyGroup);
    harmonicSeriesCheckBox_->setObjectName(QStringLiteral("harmonicSeriesCheckBox"));
    connect(harmonicSeriesCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        frequencyGridConfig_.harmonicSeriesEnabled = checked;
        updateFrequencyGridControlsEnabled();
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(harmonicSeriesCheckBox_);

    harmonicFundamentalSpinBox_ = new QDoubleSpinBox(frequencyGroup);
    harmonicFundamentalSpinBox_->setObjectName(QStringLiteral("harmonicFundamentalSpinBox"));
    harmonicFundamentalSpinBox_->setRange(1.0, 20000.0);
    harmonicFundamentalSpinBox_->setSuffix(QStringLiteral(" Hz"));
    harmonicFundamentalSpinBox_->setValue(frequencyGridConfig_.harmonicFundamentalHz);
    connect(harmonicFundamentalSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        frequencyGridConfig_.harmonicFundamentalHz = value;
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(tr("Fundamental:"), harmonicFundamentalSpinBox_);

    customFrequenciesCheckBox_ = new QCheckBox(tr("Custom frequencies"), frequencyGroup);
    customFrequenciesCheckBox_->setObjectName(QStringLiteral("customFrequenciesCheckBox"));
    connect(customFrequenciesCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        frequencyGridConfig_.customFrequenciesEnabled = checked;
        updateFrequencyGridControlsEnabled();
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(customFrequenciesCheckBox_);

    customFrequenciesLineEdit_ = new QLineEdit(frequencyGroup);
    customFrequenciesLineEdit_->setObjectName(QStringLiteral("customFrequenciesLineEdit"));
    customFrequenciesLineEdit_->setPlaceholderText(tr("e.g. 220, 440, 880"));
    connect(customFrequenciesLineEdit_, &QLineEdit::textChanged, this, [this](const QString& /*text*/) {
        updateCustomFrequenciesFromText();
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(tr("Frequencies (Hz):"), customFrequenciesLineEdit_);

    frequencyGridColorButton_ = new QPushButton(frequencyGroup);
    frequencyGridColorButton_->setObjectName(QStringLiteral("frequencyGridColorButton"));
    frequencyGridColorButton_->setText(frequencyGridConfig_.lineColor.name());
    frequencyGridColorButton_->setStyleSheet(
        QStringLiteral("background-color: %1;").arg(frequencyGridConfig_.lineColor.name()));
    connect(frequencyGridColorButton_, &QPushButton::clicked, this, [this]() {
        const QColor picked = QColorDialog::getColor(frequencyGridConfig_.lineColor, this, tr("Choose Line Color"));
        if (!picked.isValid()) {
            return;
        }
        frequencyGridConfig_.lineColor = picked;
        frequencyGridColorButton_->setText(picked.name());
        frequencyGridColorButton_->setStyleSheet(QStringLiteral("background-color: %1;").arg(picked.name()));
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(tr("Line color:"), frequencyGridColorButton_);

    frequencyGridWidthSpinBox_ = new QDoubleSpinBox(frequencyGroup);
    frequencyGridWidthSpinBox_->setObjectName(QStringLiteral("frequencyGridWidthSpinBox"));
    frequencyGridWidthSpinBox_->setRange(1.0, 10.0);
    frequencyGridWidthSpinBox_->setSuffix(QStringLiteral(" px"));
    frequencyGridWidthSpinBox_->setValue(frequencyGridConfig_.lineWidthPixels);
    connect(frequencyGridWidthSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        frequencyGridConfig_.lineWidthPixels = value;
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(tr("Line width:"), frequencyGridWidthSpinBox_);

    frequencyGridDashStyleCombo_ = new QComboBox(frequencyGroup);
    frequencyGridDashStyleCombo_->setObjectName(QStringLiteral("frequencyGridDashStyleCombo"));
    for (const auto& [style, name] : kDashStyles) {
        frequencyGridDashStyleCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(style)));
    }
    connect(frequencyGridDashStyleCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        frequencyGridConfig_.lineStyle =
            static_cast<Qt::PenStyle>(frequencyGridDashStyleCombo_->itemData(index).toInt());
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(tr("Line style:"), frequencyGridDashStyleCombo_);

    root->addWidget(frequencyGroup);

    // --- Timing Grid ---------------------------------------------------
    auto* timingGroup = new QGroupBox(tr("Timing Grid"), container);
    auto* timingForm = new QFormLayout(timingGroup);

    timingGridModeCombo_ = new QComboBox(timingGroup);
    timingGridModeCombo_->setObjectName(QStringLiteral("timingGridModeCombo"));
    for (const auto& [mode, name] : kTimingGridModes) {
        timingGridModeCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(mode)));
    }
    connect(timingGridModeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        timingGridConfig_.mode = static_cast<TimingGridMode>(timingGridModeCombo_->itemData(index).toInt());
        updateTimingGridControlsEnabled();
        emitTimingGridConfigChanged();
    });
    timingForm->addRow(tr("Mode:"), timingGridModeCombo_);

    timingGridIntervalSpinBox_ = new QDoubleSpinBox(timingGroup);
    timingGridIntervalSpinBox_->setObjectName(QStringLiteral("timingGridIntervalSpinBox"));
    timingGridIntervalSpinBox_->setRange(0.01, 3600.0);
    timingGridIntervalSpinBox_->setSuffix(QStringLiteral(" s"));
    timingGridIntervalSpinBox_->setValue(timingGridConfig_.intervalSeconds);
    connect(timingGridIntervalSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        timingGridConfig_.intervalSeconds = value;
        emitTimingGridConfigChanged();
    });
    timingForm->addRow(tr("Interval:"), timingGridIntervalSpinBox_);

    timingGridSubdivisionCombo_ = new QComboBox(timingGroup);
    timingGridSubdivisionCombo_->setObjectName(QStringLiteral("timingGridSubdivisionCombo"));
    for (const auto& [fraction, name] : kTempoSubdivisions) {
        timingGridSubdivisionCombo_->addItem(tr(name), QVariant::fromValue(fraction));
    }
    timingGridSubdivisionCombo_->setCurrentIndex(2);  // Quarter - matches timingGridConfig_'s own default (1.0).
    connect(timingGridSubdivisionCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        timingGridConfig_.tempoBeatFraction = timingGridSubdivisionCombo_->itemData(index).toDouble();
        emitTimingGridConfigChanged();
    });
    timingForm->addRow(tr("Subdivision:"), timingGridSubdivisionCombo_);

    timingGridColorButton_ = new QPushButton(timingGroup);
    timingGridColorButton_->setObjectName(QStringLiteral("timingGridColorButton"));
    timingGridColorButton_->setText(timingGridConfig_.lineColor.name());
    timingGridColorButton_->setStyleSheet(
        QStringLiteral("background-color: %1;").arg(timingGridConfig_.lineColor.name()));
    connect(timingGridColorButton_, &QPushButton::clicked, this, [this]() {
        const QColor picked = QColorDialog::getColor(timingGridConfig_.lineColor, this, tr("Choose Line Color"));
        if (!picked.isValid()) {
            return;
        }
        timingGridConfig_.lineColor = picked;
        timingGridColorButton_->setText(picked.name());
        timingGridColorButton_->setStyleSheet(QStringLiteral("background-color: %1;").arg(picked.name()));
        emitTimingGridConfigChanged();
    });
    timingForm->addRow(tr("Line color:"), timingGridColorButton_);

    timingGridWidthSpinBox_ = new QDoubleSpinBox(timingGroup);
    timingGridWidthSpinBox_->setObjectName(QStringLiteral("timingGridWidthSpinBox"));
    timingGridWidthSpinBox_->setRange(1.0, 10.0);
    timingGridWidthSpinBox_->setSuffix(QStringLiteral(" px"));
    timingGridWidthSpinBox_->setValue(timingGridConfig_.lineWidthPixels);
    connect(timingGridWidthSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        timingGridConfig_.lineWidthPixels = value;
        emitTimingGridConfigChanged();
    });
    timingForm->addRow(tr("Line width:"), timingGridWidthSpinBox_);

    timingGridDashStyleCombo_ = new QComboBox(timingGroup);
    timingGridDashStyleCombo_->setObjectName(QStringLiteral("timingGridDashStyleCombo"));
    for (const auto& [style, name] : kDashStyles) {
        timingGridDashStyleCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(style)));
    }
    connect(timingGridDashStyleCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        timingGridConfig_.lineStyle = static_cast<Qt::PenStyle>(timingGridDashStyleCombo_->itemData(index).toInt());
        emitTimingGridConfigChanged();
    });
    timingForm->addRow(tr("Line style:"), timingGridDashStyleCombo_);

    root->addWidget(timingGroup);

    // --- Snap to Grid ----------------------------------------------------
    snapToGridCheckBox_ = new QCheckBox(tr("Snap to Grid"), container);
    snapToGridCheckBox_->setObjectName(QStringLiteral("snapToGridCheckBox"));
    connect(snapToGridCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        snapToGridEnabled_ = checked;
        emit snapToGridChanged(checked);
    });
    root->addWidget(snapToGridCheckBox_);

    root->addStretch();

    updateFrequencyGridControlsEnabled();
    updateTimingGridControlsEnabled();

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);
}

void GridPanel::updateCustomFrequenciesFromText() {
    frequencyGridConfig_.customFrequenciesHz.clear();
    const QStringList tokens =
        customFrequenciesLineEdit_->text().split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
    for (const QString& token : tokens) {
        bool ok = false;
        const double frequencyHz = token.toDouble(&ok);
        if (ok && frequencyHz > 0.0) {
            frequencyGridConfig_.customFrequenciesHz.push_back(frequencyHz);
        }
    }
}

void GridPanel::updateFrequencyGridControlsEnabled() {
    harmonicFundamentalSpinBox_->setEnabled(harmonicSeriesCheckBox_->isChecked());
    customFrequenciesLineEdit_->setEnabled(customFrequenciesCheckBox_->isChecked());
}

void GridPanel::updateTimingGridControlsEnabled() {
    timingGridIntervalSpinBox_->setEnabled(timingGridConfig_.mode == TimingGridMode::Interval);
    timingGridSubdivisionCombo_->setEnabled(timingGridConfig_.mode == TimingGridMode::Tempo);
}

void GridPanel::emitFrequencyGridConfigChanged() { emit frequencyGridConfigChanged(frequencyGridConfig_); }

void GridPanel::emitTimingGridConfigChanged() { emit timingGridConfigChanged(timingGridConfig_); }

}  // namespace sound_mind::studio
