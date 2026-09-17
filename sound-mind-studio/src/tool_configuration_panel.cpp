#include "sound_mind/studio/tool_configuration_panel.h"

#include <array>
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
#include <QSpinBox>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/mind_grain.h"
#include "sound_mind/core/project.h"
#include "sound_mind/studio/color_conversion.h"

namespace sound_mind::studio {

namespace {

using sound_mind::core::BrushTipShape;
using sound_mind::core::FixedStampPlacementConfiguration;
using sound_mind::core::HealConfiguration;
using sound_mind::core::InstrumentConfiguration;
using sound_mind::core::MindGrainConfiguration;
using sound_mind::core::MindGrainId;
using sound_mind::core::MindShotConfiguration;
using sound_mind::core::MindShotId;
using sound_mind::core::OrderChaosConfiguration;
using sound_mind::core::ProceduralConfiguration;
using sound_mind::core::SmudgeConfiguration;
using sound_mind::core::SoftenConfiguration;
using sound_mind::core::StampMode;
using sound_mind::core::ToolConfiguration;
using sound_mind::core::ToolType;

/// @brief The style sheet applied to `mindGrainGroup_` while its currently-
/// configured Mind Grain can't be painted onto the active layer - see
/// `ToolConfigurationPanel::setActiveLayer()`'s own docs. A soft red wash,
/// not a harsh full-saturation red - legible against both light and dark
/// palettes, and consistent with `LayersPanel`'s own red delete-button text
/// color for "this is the thing to pay attention to" without reading as a
/// hard error dialog.
const char* const kMindGrainInvalidStyleSheet =
    "background-color: rgba(192, 64, 64, 60);";

/// @brief Every `StampMode` paired with its display name, in the same
/// order `docs/sound-mind-design.md`'s "Stamp Intervals" lists them.
constexpr std::array<std::pair<StampMode, const char*>, 4> kStampModes{{
    {StampMode::Stroke, "Stroke"},
    {StampMode::AlongCurve, "Along Curve"},
    {StampMode::TimeAxis, "Time Axis"},
    {StampMode::FrequencyAxis, "Frequency Axis"},
}};

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

/// @brief Every real (usable) `ToolType` paired with its display name - see
/// `ToolType`'s own docs on why `Clone` alone still isn't offered here.
/// `Heal`/`Soften`/`Smudge` add no group of their own (see each
/// `ToolConfiguration` subtype's own docs on why) - selecting any of them
/// just hides every other type's own group, leaving only the shared
/// Falloff/Brush Size/Stamp Mode/Color/Opacity controls visible, which is
/// all any of the three actually needs. `OrderChaos` is the exception - the
/// first tool type in this milestone with a field of its own
/// (`orderChaosGroup_`'s own Amount spin box).
constexpr std::array<std::pair<ToolType, const char*>, 8> kToolTypes{{
    {ToolType::Procedural, "Procedural"},
    {ToolType::Instrument, "Instrument"},
    {ToolType::MindShot, "Mind Shot"},
    {ToolType::MindGrain, "Mind Grain"},
    {ToolType::Heal, "Heal"},
    {ToolType::Soften, "Soften"},
    {ToolType::Smudge, "Smudge"},
    {ToolType::OrderChaos, "Order/Chaos"},
}};

/// @brief The most harmonics `harmonicCountSpinBox_` allows - generous
/// enough for a rich overtone series without the panel growing
/// unreasonably tall; not a limit `InstrumentConfiguration` itself
/// enforces (see its own docs).
constexpr int kMaxHarmonics = 16;

}  // namespace

ToolConfigurationPanel::ToolConfigurationPanel(QWidget* parent)
    : QDockWidget(tr("Tool Configuration"), parent), config_(std::make_unique<ProceduralConfiguration>()) {
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

    auto* topForm = new QFormLayout();
    toolTypeCombo_ = new QComboBox(container);
    toolTypeCombo_->setObjectName(QStringLiteral("toolTypeCombo"));
    for (const auto& [type, name] : kToolTypes) {
        toolTypeCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(type)));
    }
    connect(toolTypeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        changeToolType(static_cast<ToolType>(toolTypeCombo_->itemData(index).toInt()));
    });
    topForm->addRow(tr("Tool Type:"), toolTypeCombo_);
    root->addLayout(topForm);

    // --- Procedural's own group -----------------------------------------
    proceduralGroup_ = new QWidget(container);
    proceduralGroup_->setObjectName(QStringLiteral("proceduralGroup"));
    auto* proceduralForm = new QFormLayout(proceduralGroup_);
    proceduralForm->setContentsMargins(0, 0, 0, 0);

    tipShapeCombo_ = new QComboBox(proceduralGroup_);
    tipShapeCombo_->setObjectName(QStringLiteral("tipShapeCombo"));
    for (const auto& [shape, name] : kTipShapes) {
        tipShapeCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(shape)));
    }
    connect(tipShapeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (auto* procedural = dynamic_cast<ProceduralConfiguration*>(config_.get())) {
            procedural->setTipShape(static_cast<BrushTipShape>(tipShapeCombo_->itemData(index).toInt()));
            emitConfigChanged();
        }
    });
    proceduralForm->addRow(tr("Tip Shape:"), tipShapeCombo_);
    root->addWidget(proceduralGroup_);

    // --- Instrument's own group ------------------------------------------
    instrumentGroup_ = new QWidget(container);
    instrumentGroup_->setObjectName(QStringLiteral("instrumentGroup"));
    auto* instrumentLayout = new QVBoxLayout(instrumentGroup_);
    instrumentLayout->setContentsMargins(0, 0, 0, 0);

    auto* instrumentForm = new QFormLayout();
    harmonicCountSpinBox_ = new QSpinBox(instrumentGroup_);
    harmonicCountSpinBox_->setObjectName(QStringLiteral("harmonicCountSpinBox"));
    harmonicCountSpinBox_->setRange(1, kMaxHarmonics);
    connect(harmonicCountSpinBox_, &QSpinBox::valueChanged, this, [this](int count) {
        rebuildHarmonicStrengthRows(static_cast<std::size_t>(count));
        if (auto* instrument = dynamic_cast<InstrumentConfiguration*>(config_.get())) {
            instrument->setHarmonicStrengths(currentHarmonicStrengths());
            emitConfigChanged();
        }
    });
    instrumentForm->addRow(tr("Harmonics:"), harmonicCountSpinBox_);

    inharmonicitySpinBox_ = new QDoubleSpinBox(instrumentGroup_);
    inharmonicitySpinBox_->setObjectName(QStringLiteral("inharmonicitySpinBox"));
    inharmonicitySpinBox_->setRange(0.0, 1.0);
    inharmonicitySpinBox_->setSingleStep(0.001);
    inharmonicitySpinBox_->setDecimals(4);
    inharmonicitySpinBox_->setToolTip(
        tr("How far the harmonic series stretches sharp of a pure integer series, the way a real vibrating "
           "body's own overtones do - 0 is perfectly harmonic."));
    connect(inharmonicitySpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (auto* instrument = dynamic_cast<InstrumentConfiguration*>(config_.get())) {
            instrument->setInharmonicity(value);
            emitConfigChanged();
        }
    });
    instrumentForm->addRow(tr("Inharmonicity:"), inharmonicitySpinBox_);
    instrumentLayout->addLayout(instrumentForm);

    instrumentLayout->addWidget(new QLabel(tr("Harmonic Strengths (fundamental first):"), instrumentGroup_));
    harmonicStrengthsLayout_ = new QVBoxLayout();
    instrumentLayout->addLayout(harmonicStrengthsLayout_);

    root->addWidget(instrumentGroup_);

    // --- Mind Shot's own group --------------------------------------------
    mindShotGroup_ = new QWidget(container);
    mindShotGroup_->setObjectName(QStringLiteral("mindShotGroup"));
    auto* mindShotForm = new QFormLayout(mindShotGroup_);
    mindShotForm->setContentsMargins(0, 0, 0, 0);

    mindShotCombo_ = new QComboBox(mindShotGroup_);
    mindShotCombo_->setObjectName(QStringLiteral("mindShotCombo"));
    mindShotCombo_->addItem(tr("(none captured yet)"));
    connect(mindShotCombo_, &QComboBox::currentIndexChanged, this,
            &ToolConfigurationPanel::handleMindShotComboChanged);
    mindShotForm->addRow(tr("Mind Shot:"), mindShotCombo_);
    root->addWidget(mindShotGroup_);

    // --- Mind Grain's own group -------------------------------------------
    mindGrainGroup_ = new QWidget(container);
    mindGrainGroup_->setObjectName(QStringLiteral("mindGrainGroup"));
    mindGrainGroup_->setAutoFillBackground(true);  // so its own styleSheet background actually paints - see below.
    auto* mindGrainForm = new QFormLayout(mindGrainGroup_);
    mindGrainForm->setContentsMargins(0, 0, 0, 0);

    mindGrainCombo_ = new QComboBox(mindGrainGroup_);
    mindGrainCombo_->setObjectName(QStringLiteral("mindGrainCombo"));
    mindGrainCombo_->addItem(tr("(none captured yet)"));
    connect(mindGrainCombo_, &QComboBox::currentIndexChanged, this,
            &ToolConfigurationPanel::handleMindGrainComboChanged);
    mindGrainForm->addRow(tr("Mind Grain:"), mindGrainCombo_);
    root->addWidget(mindGrainGroup_);

    // --- Order/Chaos's own group -------------------------------------------
    orderChaosGroup_ = new QWidget(container);
    orderChaosGroup_->setObjectName(QStringLiteral("orderChaosGroup"));
    auto* orderChaosForm = new QFormLayout(orderChaosGroup_);
    orderChaosForm->setContentsMargins(0, 0, 0, 0);

    amountSpinBox_ = new QDoubleSpinBox(orderChaosGroup_);
    amountSpinBox_->setObjectName(QStringLiteral("amountSpinBox"));
    amountSpinBox_->setRange(-1.0, 1.0);
    amountSpinBox_->setSingleStep(0.05);
    amountSpinBox_->setDecimals(2);
    amountSpinBox_->setToolTip(
        tr("Negative pushes toward Chaos (randomly scrambling pixel intensities), positive toward Order "
           "(concentrating them into a horizontal/vertical cross) - 0 has no effect."));
    connect(amountSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (auto* orderChaos = dynamic_cast<OrderChaosConfiguration*>(config_.get())) {
            orderChaos->setAmount(value);
            emitConfigChanged();
        }
    });
    orderChaosForm->addRow(tr("Amount:"), amountSpinBox_);
    root->addWidget(orderChaosGroup_);

    sharedControlsForm_ = new QFormLayout();

    falloffSpinBox_ = new QDoubleSpinBox(container);
    falloffSpinBox_->setObjectName(QStringLiteral("falloffSpinBox"));
    falloffSpinBox_->setRange(0.0, 1.0);
    falloffSpinBox_->setSingleStep(0.05);
    falloffSpinBox_->setValue(config_->falloff());
    connect(falloffSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_->setFalloff(static_cast<float>(value));
        emitConfigChanged();
    });
    sharedControlsForm_->addRow(tr("Falloff:"), falloffSpinBox_);

    sizeSpinBox_ = new QDoubleSpinBox(container);
    sizeSpinBox_->setObjectName(QStringLiteral("sizeSpinBox"));
    sizeSpinBox_->setRange(0.001, 100.0);
    sizeSpinBox_->setSingleStep(0.01);
    sizeSpinBox_->setDecimals(3);
    sizeSpinBox_->setValue(config_->size());
    // Not a screen-pixel radius - see ToolConfiguration::size()'s own
    // docs. Spelled out here since the unit isn't otherwise visible
    // anywhere in the UI itself.
    sizeSpinBox_->setToolTip(
        tr("The brush tip's own radius: in seconds on the time axis, and "
           "the equivalent frequency span on the frequency axis (using "
           "this project's own Hz-per-second scale)."));
    connect(sizeSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_->setSize(value);
        emitConfigChanged();
    });
    sharedControlsForm_->addRow(tr("Brush Size:"), sizeSpinBox_);

    stampModeCombo_ = new QComboBox(container);
    stampModeCombo_->setObjectName(QStringLiteral("stampModeCombo"));
    for (const auto& [mode, name] : kStampModes) {
        stampModeCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(mode)));
    }
    connect(stampModeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        config_->setStampMode(static_cast<StampMode>(stampModeCombo_->itemData(index).toInt()));
        updateStampIntervalAppearance();
        emitConfigChanged();
    });
    sharedControlsForm_->addRow(tr("Stamp Mode:"), stampModeCombo_);

    stampIntervalSpinBox_ = new QDoubleSpinBox(container);
    stampIntervalSpinBox_->setObjectName(QStringLiteral("stampIntervalSpinBox"));
    stampIntervalSpinBox_->setRange(0.001, 20000.0);
    stampIntervalSpinBox_->setSingleStep(0.01);
    stampIntervalSpinBox_->setDecimals(3);
    stampIntervalSpinBox_->setValue(config_->stampInterval());
    connect(stampIntervalSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_->setStampInterval(value);
        emitConfigChanged();
    });
    sharedControlsForm_->addRow(tr("Stamp Interval:"), stampIntervalSpinBox_);
    updateStampIntervalAppearance();

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
    sharedControlsForm_->addRow(tr("Color:"), colorButton_);

    opacitySpinBox_ = new QDoubleSpinBox(container);
    opacitySpinBox_->setObjectName(QStringLiteral("opacitySpinBox"));
    opacitySpinBox_->setRange(0.0, 100.0);
    opacitySpinBox_->setSingleStep(5.0);
    opacitySpinBox_->setSuffix(tr("%"));
    opacitySpinBox_->setValue(100.0);
    connect(opacitySpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        auto stop = config_->defaultGradient().stops().front();
        stop.leftOpacity = static_cast<float>(value / 100.0);
        stop.rightOpacity = static_cast<float>(value / 100.0);
        config_->defaultGradient().setStopValues(0, stop);
        config_->defaultGradient().setStopValues(1, stop);
        emitConfigChanged();
    });
    sharedControlsForm_->addRow(tr("Opacity:"), opacitySpinBox_);

    root->addLayout(sharedControlsForm_);
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
        auto stop = config_->defaultGradient().stops().front();
        stop.leftIntensity = 0.0f;
        stop.rightIntensity = 0.0f;
        stop.leftOpacity = 1.0f;
        stop.rightOpacity = 1.0f;
        config_->defaultGradient().setStopValues(0, stop);
        config_->defaultGradient().setStopValues(1, stop);
    }
    updateColorButtonAppearance();

    // The Instrument group's own rows are built from config_'s current
    // (default) harmonicStrengths() even while Procedural is selected and
    // hidden - so switching to Instrument for the first time shows a
    // real, already-populated series rather than an empty one. Since
    // config_ itself starts as a ProceduralConfiguration, seed
    // harmonicCountSpinBox_ from a fresh InstrumentConfiguration's own
    // defaults directly rather than from config_.
    rebuildHarmonicStrengthRows(InstrumentConfiguration{}.harmonicStrengths().size());
    {
        const QSignalBlocker blocker(harmonicCountSpinBox_);
        harmonicCountSpinBox_->setValue(static_cast<int>(harmonicStrengthSpinBoxes_.size()));
    }
    {
        const InstrumentConfiguration defaults;
        for (std::size_t i = 0; i < harmonicStrengthSpinBoxes_.size() && i < defaults.harmonicStrengths().size();
             ++i) {
            const QSignalBlocker blocker(harmonicStrengthSpinBoxes_[i]);
            harmonicStrengthSpinBoxes_[i]->setValue(defaults.harmonicStrengths()[i]);
        }
    }
    updateVisibleToolTypeGroup();

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);
}

void ToolConfigurationPanel::emitConfigChanged() { emit toolConfigurationChanged(*config_); }

void ToolConfigurationPanel::changeToolType(ToolType type) {
    if (config_->type() == type) {
        return;
    }

    std::unique_ptr<ToolConfiguration> replacement;
    if (type == ToolType::Procedural) {
        replacement = std::make_unique<ProceduralConfiguration>();
    } else if (type == ToolType::Instrument) {
        auto instrument = std::make_unique<InstrumentConfiguration>();
        instrument->setHarmonicStrengths(currentHarmonicStrengths());
        instrument->setInharmonicity(inharmonicitySpinBox_->value());
        replacement = std::move(instrument);
    } else if (type == ToolType::MindShot) {
        auto mindShot = std::make_unique<MindShotConfiguration>();
        // Selects whatever mindShotCombo_ currently shows, if it's a real
        // entry - so switching to MindShot with something already picked
        // in the combo (from a prior visit to this group) doesn't silently
        // reset to "nothing selected".
        if (const QVariant data = mindShotCombo_->currentData(); data.isValid() && project_ != nullptr) {
            const auto id = static_cast<MindShotId>(data.toULongLong());
            if (const auto* named = project_->mindShotById(id)) {
                mindShot->setClip(id, named->clip);
            }
        }
        replacement = std::move(mindShot);
    } else if (type == ToolType::MindGrain) {
        auto mindGrain = std::make_unique<MindGrainConfiguration>();
        // Same reasoning as MindShot's own branch above.
        if (const QVariant data = mindGrainCombo_->currentData(); data.isValid() && project_ != nullptr) {
            const auto id = static_cast<MindGrainId>(data.toULongLong());
            if (const auto* named = project_->mindGrainById(id)) {
                mindGrain->setReference(id, named->sourceLayerId, named->bounds);
            }
        }
        replacement = std::move(mindGrain);
    } else if (type == ToolType::Heal) {
        replacement = std::make_unique<HealConfiguration>();
    } else if (type == ToolType::Soften) {
        replacement = std::make_unique<SoftenConfiguration>();
    } else if (type == ToolType::Smudge) {
        replacement = std::make_unique<SmudgeConfiguration>();
    } else if (type == ToolType::OrderChaos) {
        auto orderChaos = std::make_unique<OrderChaosConfiguration>();
        orderChaos->setAmount(amountSpinBox_->value());
        replacement = std::move(orderChaos);
    } else {
        return;  // Defensive: toolTypeCombo_ only ever offers real types.
    }

    // Every shared base field carries over from the outgoing configuration
    // - see the class's own docs. Stamp Mode/Interval specifically carry
    // over via storedStampMode()/storedStampInterval() (the literal stored
    // value), not the plain stampMode()/stampInterval() getters - if the
    // *outgoing* configuration is one of FixedStampPlacementConfiguration's
    // own subtypes, those getters report the forced AlongCurve/66%-of-size
    // value, not the user's own real prior preference (still sitting,
    // unread, in that object's own stored fields) - carrying the *forced*
    // value forward would silently overwrite whatever Stamp Mode the user
    // had actually chosen before switching into Heal/Soften/Smudge/
    // OrderChaos, the moment they switch back out to a type where it's a
    // real, live setting again. For a non-forced outgoing type, the stored
    // and reported values are identical anyway, so this is unconditionally
    // correct either way.
    replacement->setStampMode(config_->storedStampMode());
    replacement->setStampInterval(config_->storedStampInterval());
    replacement->setName(config_->name());
    replacement->setFalloff(config_->falloff());
    replacement->setSize(config_->size());
    replacement->defaultGradient() = config_->defaultGradient();

    config_ = std::move(replacement);
    updateVisibleToolTypeGroup();
    emitConfigChanged();
}

void ToolConfigurationPanel::updateVisibleToolTypeGroup() {
    const ToolType type = config_->type();
    proceduralGroup_->setVisible(type == ToolType::Procedural);
    instrumentGroup_->setVisible(type == ToolType::Instrument);
    mindShotGroup_->setVisible(type == ToolType::MindShot);
    mindGrainGroup_->setVisible(type == ToolType::MindGrain);
    orderChaosGroup_->setVisible(type == ToolType::OrderChaos);
    updateMindGrainValidity();
    updateSharedControlVisibility();
}

void ToolConfigurationPanel::updateSharedControlVisibility() {
    const ToolType type = config_->type();
    const bool isMindShotOrGrain = type == ToolType::MindShot || type == ToolType::MindGrain;
    const bool isFixedPlacement = dynamic_cast<const FixedStampPlacementConfiguration*>(config_.get()) != nullptr;

    const bool showFalloffSizeAndOpacity = !isMindShotOrGrain;
    const bool showStampModeAndInterval = !isFixedPlacement;
    const bool showColor = !isMindShotOrGrain && !isFixedPlacement;

    sharedControlsForm_->setRowVisible(falloffSpinBox_, showFalloffSizeAndOpacity);
    sharedControlsForm_->setRowVisible(sizeSpinBox_, showFalloffSizeAndOpacity);
    sharedControlsForm_->setRowVisible(stampModeCombo_, showStampModeAndInterval);
    sharedControlsForm_->setRowVisible(stampIntervalSpinBox_, showStampModeAndInterval);
    sharedControlsForm_->setRowVisible(colorButton_, showColor);
    sharedControlsForm_->setRowVisible(opacitySpinBox_, showFalloffSizeAndOpacity);
}

void ToolConfigurationPanel::rebuildHarmonicStrengthRows(std::size_t count) {
    // Removes any existing rows past the very end first if shrinking, or
    // adds new ones (each defaulting to 1.0) if growing - whichever rows
    // survive keep their own already-displayed value untouched. Deleted
    // immediately (not deleteLater()) - shrinking is only ever triggered
    // by harmonicCountSpinBox_'s own change or a fresh
    // setToolConfiguration() sync, never by one of these very spin boxes'
    // own signal still on the call stack, so an immediate delete is safe
    // and keeps findChild() (as tests use to check a shrunk row is really
    // gone) accurate without waiting on the event loop.
    while (harmonicStrengthSpinBoxes_.size() > count) {
        QDoubleSpinBox* spinBox = harmonicStrengthSpinBoxes_.back();
        harmonicStrengthSpinBoxes_.pop_back();
        harmonicStrengthsLayout_->removeWidget(spinBox);
        delete spinBox;
    }
    while (harmonicStrengthSpinBoxes_.size() < count) {
        const int harmonicNumber = static_cast<int>(harmonicStrengthSpinBoxes_.size()) + 1;
        auto* spinBox = new QDoubleSpinBox(instrumentGroup_);
        spinBox->setObjectName(QStringLiteral("harmonicStrengthSpinBox%1").arg(harmonicNumber));
        spinBox->setRange(0.0, 10.0);
        spinBox->setSingleStep(0.05);
        spinBox->setDecimals(3);
        spinBox->setValue(harmonicNumber == 1 ? 1.0 : 0.0);
        spinBox->setPrefix(harmonicNumber == 1 ? tr("Fundamental: ") : tr("Harmonic %1: ").arg(harmonicNumber));
        connect(spinBox, &QDoubleSpinBox::valueChanged, this, [this](double) {
            if (auto* instrument = dynamic_cast<InstrumentConfiguration*>(config_.get())) {
                instrument->setHarmonicStrengths(currentHarmonicStrengths());
                emitConfigChanged();
            }
        });
        harmonicStrengthsLayout_->addWidget(spinBox);
        harmonicStrengthSpinBoxes_.push_back(spinBox);
    }
}

std::vector<double> ToolConfigurationPanel::currentHarmonicStrengths() const {
    std::vector<double> strengths;
    strengths.reserve(harmonicStrengthSpinBoxes_.size());
    for (const QDoubleSpinBox* spinBox : harmonicStrengthSpinBoxes_) {
        strengths.push_back(spinBox->value());
    }
    return strengths;
}

void ToolConfigurationPanel::setToolConfiguration(const sound_mind::core::ToolConfiguration& config) {
    config_ = config.clone();

    // Each control's own valueChanged/currentIndexChanged handler both
    // mutates config_ (redundant here, already set above) and calls
    // emitConfigChanged() - QSignalBlocker suppresses both side effects
    // while only the *display* is meant to change, per this method's own
    // "does not emit toolConfigurationChanged()" contract.
    {
        const QSignalBlocker blocker(toolTypeCombo_);
        const int index = toolTypeCombo_->findData(QVariant::fromValue(static_cast<int>(config_->type())));
        toolTypeCombo_->setCurrentIndex(index >= 0 ? index : 0);
    }
    if (const auto* procedural = dynamic_cast<const ProceduralConfiguration*>(config_.get())) {
        const QSignalBlocker blocker(tipShapeCombo_);
        const int index = tipShapeCombo_->findData(QVariant::fromValue(static_cast<int>(procedural->tipShape())));
        tipShapeCombo_->setCurrentIndex(index >= 0 ? index : 0);
    } else if (const auto* instrument = dynamic_cast<const InstrumentConfiguration*>(config_.get())) {
        rebuildHarmonicStrengthRows(instrument->harmonicStrengths().size());
        {
            const QSignalBlocker blocker(harmonicCountSpinBox_);
            harmonicCountSpinBox_->setValue(static_cast<int>(instrument->harmonicStrengths().size()));
        }
        for (std::size_t i = 0; i < harmonicStrengthSpinBoxes_.size(); ++i) {
            const QSignalBlocker blocker(harmonicStrengthSpinBoxes_[i]);
            harmonicStrengthSpinBoxes_[i]->setValue(instrument->harmonicStrengths()[i]);
        }
        {
            const QSignalBlocker blocker(inharmonicitySpinBox_);
            inharmonicitySpinBox_->setValue(instrument->inharmonicity());
        }
    } else if (const auto* mindShot = dynamic_cast<const MindShotConfiguration*>(config_.get())) {
        const QSignalBlocker blocker(mindShotCombo_);
        int index = -1;
        if (const auto sourceId = mindShot->sourceMindShotId(); sourceId.has_value()) {
            index = mindShotCombo_->findData(QVariant::fromValue(static_cast<qulonglong>(*sourceId)));
        }
        mindShotCombo_->setCurrentIndex(index >= 0 ? index : 0);
    } else if (const auto* mindGrain = dynamic_cast<const MindGrainConfiguration*>(config_.get())) {
        const QSignalBlocker blocker(mindGrainCombo_);
        int index = -1;
        if (const auto sourceId = mindGrain->sourceMindGrainId(); sourceId.has_value()) {
            index = mindGrainCombo_->findData(QVariant::fromValue(static_cast<qulonglong>(*sourceId)));
        }
        mindGrainCombo_->setCurrentIndex(index >= 0 ? index : 0);
    } else if (const auto* orderChaos = dynamic_cast<const OrderChaosConfiguration*>(config_.get())) {
        const QSignalBlocker blocker(amountSpinBox_);
        amountSpinBox_->setValue(orderChaos->amount());
    }
    updateVisibleToolTypeGroup();
    {
        const QSignalBlocker blocker(falloffSpinBox_);
        falloffSpinBox_->setValue(config_->falloff());
    }
    {
        const QSignalBlocker blocker(sizeSpinBox_);
        sizeSpinBox_->setValue(config_->size());
    }
    {
        const QSignalBlocker blocker(stampModeCombo_);
        const int index = stampModeCombo_->findData(QVariant::fromValue(static_cast<int>(config_->stampMode())));
        stampModeCombo_->setCurrentIndex(index >= 0 ? index : 0);
    }
    {
        const QSignalBlocker blocker(stampIntervalSpinBox_);
        stampIntervalSpinBox_->setValue(config_->stampInterval());
    }
    updateStampIntervalAppearance();
    {
        const QSignalBlocker blocker(opacitySpinBox_);
        opacitySpinBox_->setValue(static_cast<double>(config_->defaultGradient().stops().front().leftOpacity) *
                                   100.0);
    }
    updateColorButtonAppearance();
}

QColor ToolConfigurationPanel::color() const {
    const auto& stop = config_->defaultGradient().stops().front();
    return QColor(dbToDisplayByte(stop.leftIntensity), dbToDisplayByte(stop.rightIntensity), 0);
}

void ToolConfigurationPanel::setColor(QColor color) {
    auto stop = config_->defaultGradient().stops().front();
    stop.leftIntensity = displayByteToDb(color.red());
    stop.rightIntensity = displayByteToDb(color.green());
    config_->defaultGradient().setStopValues(0, stop);
    config_->defaultGradient().setStopValues(1, stop);
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

void ToolConfigurationPanel::updateStampIntervalAppearance() {
    const bool active = config_->stampMode() != StampMode::Stroke;
    stampIntervalSpinBox_->setEnabled(active);

    switch (config_->stampMode()) {
        case StampMode::Stroke:
            stampIntervalSpinBox_->setSuffix(QString());
            stampIntervalSpinBox_->setToolTip(
                tr("Meaningless for Stroke - stamps are already spaced exactly as densely as the stroke's own "
                   "raw input was drawn."));
            break;
        case StampMode::AlongCurve:
            stampIntervalSpinBox_->setSuffix(QStringLiteral(" s (along curve)"));
            stampIntervalSpinBox_->setToolTip(
                tr("Spacing between stamps, measured along the path's own arc length - in the same seconds-"
                   "equivalent units as Brush Size (see its own tooltip)."));
            break;
        case StampMode::TimeAxis:
            stampIntervalSpinBox_->setSuffix(QStringLiteral(" s"));
            stampIntervalSpinBox_->setToolTip(
                tr("Stamps everywhere the path crosses a time-axis line this many seconds apart, regardless of "
                   "the path's own shape."));
            break;
        case StampMode::FrequencyAxis:
            stampIntervalSpinBox_->setSuffix(QStringLiteral(" Hz"));
            stampIntervalSpinBox_->setToolTip(
                tr("Stamps everywhere the path crosses a frequency-axis line this many Hz apart, regardless of "
                   "the path's own shape."));
            break;
    }
}

void ToolConfigurationPanel::setProject(sound_mind::core::Project* project) {
    project_ = project;
    refreshMindShots();
    refreshMindGrains();
}

void ToolConfigurationPanel::refreshMindShots() {
    // Preserve the current selection's own id (if any), so a still-
    // existing entry stays selected across the rebuild below.
    const QVariant previousData = mindShotCombo_->currentData();

    const QSignalBlocker blocker(mindShotCombo_);
    mindShotCombo_->clear();
    if (project_ == nullptr || project_->mindShots().empty()) {
        mindShotCombo_->addItem(tr("(none captured yet)"));
        return;
    }
    for (const auto& named : project_->mindShots()) {
        mindShotCombo_->addItem(QString::fromStdString(named.name),
                                 QVariant::fromValue(static_cast<qulonglong>(named.id)));
    }
    const int index = mindShotCombo_->findData(previousData);
    mindShotCombo_->setCurrentIndex(index >= 0 ? index : 0);
}

void ToolConfigurationPanel::handleMindShotComboChanged(int index) {
    if (project_ == nullptr) {
        return;
    }
    const QVariant data = mindShotCombo_->itemData(index);
    if (!data.isValid()) {
        return;  // The "(none captured yet)" placeholder.
    }
    const auto id = static_cast<MindShotId>(data.toULongLong());
    const auto* named = project_->mindShotById(id);
    if (named == nullptr) {
        return;
    }
    if (auto* mindShot = dynamic_cast<MindShotConfiguration*>(config_.get())) {
        mindShot->setClip(id, named->clip);
        emitConfigChanged();
    }
}

void ToolConfigurationPanel::refreshMindGrains() {
    // Same reasoning as refreshMindShots()'s own identical structure.
    const QVariant previousData = mindGrainCombo_->currentData();

    const QSignalBlocker blocker(mindGrainCombo_);
    mindGrainCombo_->clear();
    if (project_ == nullptr || project_->mindGrains().empty()) {
        mindGrainCombo_->addItem(tr("(none captured yet)"));
    } else {
        for (const auto& named : project_->mindGrains()) {
            mindGrainCombo_->addItem(QString::fromStdString(named.name),
                                       QVariant::fromValue(static_cast<qulonglong>(named.id)));
        }
        const int index = mindGrainCombo_->findData(previousData);
        mindGrainCombo_->setCurrentIndex(index >= 0 ? index : 0);
    }
    updateMindGrainValidity();
}

void ToolConfigurationPanel::handleMindGrainComboChanged(int index) {
    if (project_ != nullptr) {
        const QVariant data = mindGrainCombo_->itemData(index);
        if (data.isValid()) {  // Not the "(none captured yet)" placeholder.
            const auto id = static_cast<MindGrainId>(data.toULongLong());
            if (const auto* named = project_->mindGrainById(id)) {
                if (auto* mindGrain = dynamic_cast<MindGrainConfiguration*>(config_.get())) {
                    mindGrain->setReference(id, named->sourceLayerId, named->bounds);
                    emitConfigChanged();
                }
            }
        }
    }
    updateMindGrainValidity();
}

void ToolConfigurationPanel::setActiveLayer(sound_mind::core::LayerId layer) {
    activeLayer_ = layer;
    updateMindGrainValidity();
}

void ToolConfigurationPanel::updateMindGrainValidity() {
    bool invalid = false;
    QString reason;
    if (const auto* mindGrain = dynamic_cast<const MindGrainConfiguration*>(config_.get())) {
        if (project_ != nullptr &&
            !sound_mind::core::isLayerAbove(*project_, activeLayer_, mindGrain->sourceLayerId())) {
            invalid = true;
            const sound_mind::core::Layer* sourceLayer = project_->layerById(mindGrain->sourceLayerId());
            reason = tr("This Mind Grain can only paint onto a layer above \"%1\" (its own source) - "
                        "the active layer isn't. Select a layer higher in the stack, or reorder the layers, "
                        "before painting with it.")
                         .arg(sourceLayer != nullptr ? QString::fromStdString(sourceLayer->name())
                                                       : tr("its source layer"));
        }
    }
    mindGrainGroup_->setStyleSheet(invalid ? QString::fromUtf8(kMindGrainInvalidStyleSheet) : QString());
    mindGrainGroup_->setToolTip(reason);
    mindGrainCombo_->setToolTip(reason);
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
