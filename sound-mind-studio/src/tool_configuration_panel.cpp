#include "sound_mind/studio/tool_configuration_panel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

namespace sound_mind::studio {

namespace {

using sound_mind::core::BrushTipShape;

/// @brief `color_mapping.cpp`'s own dB<->display-byte range
/// (`kDisplayMinDb`/`kDisplayMaxDb`) - not exported from there (an
/// anonymous-namespace implementation detail of `sound-mind-codec`, which
/// `sound-mind-studio` doesn't depend on for this), so the Color swatch
/// re-derives it locally - the same "Codec's own formula is in a private
/// header" precedent `paint_application.cpp`'s own
/// frequencyToBinIndex()/timeToFrameIndex() already set (Decision #36).
constexpr float kDisplayMinDb = -96.0f;
constexpr float kDisplayMaxDb = 0.0f;

/// @brief A gradient stop intensity (dB), as the 0-255 byte a color
/// picker's own red/green channel shows for it.
int dbToByteColor(float db) {
    const float clamped = std::clamp(db, kDisplayMinDb, kDisplayMaxDb);
    const float normalized = (clamped - kDisplayMinDb) / (kDisplayMaxDb - kDisplayMinDb);
    return static_cast<int>(std::lround(normalized * 255.0f));
}

/// @brief The exact inverse of dbToByteColor().
float byteToDbColor(int value) {
    const float normalized = static_cast<float>(std::clamp(value, 0, 255)) / 255.0f;
    return kDisplayMinDb + normalized * (kDisplayMaxDb - kDisplayMinDb);
}

/// @brief Every `BrushTipShape` paired with its display name, in the same
/// order `docs/sound-mind-design.md`'s "Procedural Brushes" lists them -
/// basic fills first, then lines/compounds, then scattered textures.
/// `Circle`/`Square`/`Diamond` have real, distinct footprints so far (see
/// `applyPaintOperation()`'s own docs) - the rest fall back to `Circle`'s
/// until each is actually built, same as every listed-but-not-yet-real
/// `ToolType` past `Procedural`.
constexpr std::array<std::pair<BrushTipShape, const char*>, 11> kTipShapes{{
    {BrushTipShape::Circle, "Circle"},
    {BrushTipShape::Square, "Square"},
    {BrushTipShape::Diamond, "Diamond"},
    {BrushTipShape::Triangle, "Triangle"},
    {BrushTipShape::SingleStroke, "Single Stroke"},
    {BrushTipShape::Cross, "Cross"},
    {BrushTipShape::Star, "Star"},
    {BrushTipShape::Corner, "Corner"},
    {BrushTipShape::Arc, "Arc"},
    {BrushTipShape::DotSpatter, "Dot Spatter"},
    {BrushTipShape::Dapple, "Dapple"},
}};

}  // namespace

ToolConfigurationPanel::ToolConfigurationPanel(QWidget* parent) : QDockWidget(tr("Tool Configuration"), parent) {
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    auto* overlayRow = new QVBoxLayout();
    showBoundingBoxesCheckBox_ = new QCheckBox(tr("Show bounding boxes"), container);
    showBoundingBoxesCheckBox_->setObjectName(QStringLiteral("showBoundingBoxesCheckBox"));
    connect(showBoundingBoxesCheckBox_, &QCheckBox::toggled, this, &ToolConfigurationPanel::showBoundingBoxesChanged);
    overlayRow->addWidget(showBoundingBoxesCheckBox_);

    showPathGeometryCheckBox_ = new QCheckBox(tr("Show path geometry"), container);
    showPathGeometryCheckBox_->setObjectName(QStringLiteral("showPathGeometryCheckBox"));
    connect(showPathGeometryCheckBox_, &QCheckBox::toggled, this, &ToolConfigurationPanel::showPathGeometryChanged);
    overlayRow->addWidget(showPathGeometryCheckBox_);
    root->addLayout(overlayRow);

    auto* form = new QFormLayout();

    tipShapeCombo_ = new QComboBox(container);
    tipShapeCombo_->setObjectName(QStringLiteral("tipShapeCombo"));
    for (const auto& [shape, name] : kTipShapes) {
        tipShapeCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(shape)));
    }
    connect(tipShapeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        config_.setTipShape(static_cast<BrushTipShape>(tipShapeCombo_->itemData(index).toInt()));
        emitConfigChanged();
    });
    form->addRow(tr("Tip Shape:"), tipShapeCombo_);

    falloffSpinBox_ = new QDoubleSpinBox(container);
    falloffSpinBox_->setObjectName(QStringLiteral("falloffSpinBox"));
    falloffSpinBox_->setRange(0.0, 1.0);
    falloffSpinBox_->setSingleStep(0.05);
    falloffSpinBox_->setValue(config_.falloff());
    connect(falloffSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setFalloff(static_cast<float>(value));
        emitConfigChanged();
    });
    form->addRow(tr("Falloff:"), falloffSpinBox_);

    sizeSpinBox_ = new QDoubleSpinBox(container);
    sizeSpinBox_->setObjectName(QStringLiteral("sizeSpinBox"));
    sizeSpinBox_->setRange(0.001, 100.0);
    sizeSpinBox_->setSingleStep(0.01);
    sizeSpinBox_->setDecimals(3);
    sizeSpinBox_->setValue(config_.size());
    connect(sizeSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setSize(value);
        emitConfigChanged();
    });
    form->addRow(tr("Brush Size:"), sizeSpinBox_);

    // Color/opacity directly below set *both* gradient stops uniformly -
    // the design doc's own "Uniform color" example (see
    // sound-mind-design.md's Gradients) - a real, full multi-stop
    // gradient editor is a separate, later feature (see this class's own
    // docs); this is the simplest control that still lets a stroke
    // actually paint something visible.
    colorButton_ = new QPushButton(container);
    colorButton_->setObjectName(QStringLiteral("colorButton"));
    colorButton_->setToolTip(
        tr("Brush color (stereo balance) - red: left channel, green: right channel"));
    connect(colorButton_, &QPushButton::clicked, this, &ToolConfigurationPanel::openColorDialog);
    form->addRow(tr("Color:"), colorButton_);

    opacitySpinBox_ = new QDoubleSpinBox(container);
    opacitySpinBox_->setObjectName(QStringLiteral("opacitySpinBox"));
    opacitySpinBox_->setRange(0.0, 100.0);
    opacitySpinBox_->setSingleStep(5.0);
    opacitySpinBox_->setSuffix(tr("%"));
    opacitySpinBox_->setValue(100.0);
    connect(opacitySpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        auto stop = config_.defaultGradient().stops().front();
        stop.leftOpacity = static_cast<float>(value / 100.0);
        stop.rightOpacity = static_cast<float>(value / 100.0);
        config_.defaultGradient().setStopValues(0, stop);
        config_.defaultGradient().setStopValues(1, stop);
        emitConfigChanged();
    });
    form->addRow(tr("Opacity:"), opacitySpinBox_);

    root->addLayout(form);
    root->addStretch();

    // Panel-own default: a real, fully-opaque brush (not the transparent
    // default a bare ToolConfiguration starts with - see its own docs) -
    // so a fresh stroke, painted before ever touching a control, is
    // already visible rather than silently doing nothing. Opacity's spin
    // box above is already constructed with the matching displayed value
    // (100%), so this just makes config_ itself agree with it; the color
    // swatch is synced to it right after (updateColorButtonAppearance()
    // needs colorButton_ to already exist, which it now does).
    {
        auto stop = config_.defaultGradient().stops().front();
        stop.leftIntensity = 0.0f;
        stop.rightIntensity = 0.0f;
        stop.leftOpacity = 1.0f;
        stop.rightOpacity = 1.0f;
        config_.defaultGradient().setStopValues(0, stop);
        config_.defaultGradient().setStopValues(1, stop);
    }
    updateColorButtonAppearance();

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);
}

void ToolConfigurationPanel::emitConfigChanged() { emit toolConfigurationChanged(config_); }

void ToolConfigurationPanel::setToolConfiguration(const sound_mind::core::ToolConfiguration& config) {
    config_ = config;

    // Each control's own valueChanged/currentIndexChanged handler both
    // mutates config_ (redundant here, already set above) and calls
    // emitConfigChanged() - QSignalBlocker suppresses both side effects
    // while only the *display* is meant to change, per this method's own
    // "does not emit toolConfigurationChanged()" contract.
    {
        const QSignalBlocker blocker(tipShapeCombo_);
        const int index = tipShapeCombo_->findData(QVariant::fromValue(static_cast<int>(config_.tipShape())));
        tipShapeCombo_->setCurrentIndex(index >= 0 ? index : 0);
    }
    {
        const QSignalBlocker blocker(falloffSpinBox_);
        falloffSpinBox_->setValue(config_.falloff());
    }
    {
        const QSignalBlocker blocker(sizeSpinBox_);
        sizeSpinBox_->setValue(config_.size());
    }
    {
        const QSignalBlocker blocker(opacitySpinBox_);
        opacitySpinBox_->setValue(static_cast<double>(config_.defaultGradient().stops().front().leftOpacity) * 100.0);
    }
    updateColorButtonAppearance();
}

QColor ToolConfigurationPanel::color() const {
    const auto& stop = config_.defaultGradient().stops().front();
    return QColor(dbToByteColor(stop.leftIntensity), dbToByteColor(stop.rightIntensity), 0);
}

void ToolConfigurationPanel::setColor(QColor color) {
    auto stop = config_.defaultGradient().stops().front();
    stop.leftIntensity = byteToDbColor(color.red());
    stop.rightIntensity = byteToDbColor(color.green());
    config_.defaultGradient().setStopValues(0, stop);
    config_.defaultGradient().setStopValues(1, stop);
    updateColorButtonAppearance();
    emitConfigChanged();
}

void ToolConfigurationPanel::openColorDialog() {
    // getColor() returns an invalid QColor for Cancel - see its own docs -
    // left as a no-op rather than applying it, matching every other
    // dialog-driven action in this codebase (e.g. MainWindow's own Import/
    // Export) treating a cancelled dialog as "nothing happened".
    const QColor picked = QColorDialog::getColor(color(), this, tr("Choose Brush Color"));
    if (picked.isValid()) {
        setColor(picked);
    }
}

void ToolConfigurationPanel::updateColorButtonAppearance() {
    const QColor current = color();
    colorButton_->setStyleSheet(QStringLiteral("background-color: %1;").arg(current.name()));
    // A readable hex label alongside the swatch fill - accessible even
    // where color alone isn't (e.g. colorblindness), and gives tests a
    // stable text() to check instead of parsing a stylesheet string.
    colorButton_->setText(current.name());
}

}  // namespace sound_mind::studio
