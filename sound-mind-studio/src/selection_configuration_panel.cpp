#include "sound_mind/studio/selection_configuration_panel.h"

#include <array>
#include <utility>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

namespace sound_mind::studio {

namespace {

/// @brief The Selection Type dropdown's own entries, in display order -
/// the same `{enumerator, display name}` table shape
/// `tool_configuration_panel.cpp`'s own `kToolTypes` already establishes.
constexpr std::array<std::pair<SelectionShape, const char*>, 3> kSelectionShapes = {{
    {SelectionShape::Rectangle, "Rectangle"},
    {SelectionShape::Lasso, "Lasso"},
    {SelectionShape::Wand, "Wand"},
}};

/// @brief The Paste Blend Mode dropdown's own entries, in display order -
/// every `sound_mind::core::BlendMode` value, `v0.Y.37.1` (Deferred Blend
/// Modes). `Overwrite` listed first, matching its own role as the default
/// (old-project-compatible) choice.
constexpr std::array<std::pair<sound_mind::core::BlendMode, const char*>, 7> kPasteBlendModes = {{
    {sound_mind::core::BlendMode::Overwrite, "Overwrite"},
    {sound_mind::core::BlendMode::Normal, "Normal"},
    {sound_mind::core::BlendMode::Multiply, "Multiply"},
    {sound_mind::core::BlendMode::Screen, "Screen"},
    {sound_mind::core::BlendMode::Overlay, "Overlay"},
    {sound_mind::core::BlendMode::Difference, "Difference"},
    {sound_mind::core::BlendMode::Add, "Add"},
}};

}  // namespace

SelectionConfigurationPanel::SelectionConfigurationPanel(QWidget* parent)
    : QDockWidget(tr("Selection Configuration"), parent) {
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    auto* topForm = new QFormLayout();
    selectionTypeCombo_ = new QComboBox(container);
    selectionTypeCombo_->setObjectName(QStringLiteral("selectionTypeCombo"));
    for (const auto& [shape, name] : kSelectionShapes) {
        selectionTypeCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(shape)));
    }
    connect(selectionTypeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit selectionShapeChanged(static_cast<SelectionShape>(selectionTypeCombo_->itemData(index).toInt()));
        updateWandGroupVisibility();
    });
    topForm->addRow(tr("Selection Type:"), selectionTypeCombo_);
    root->addLayout(topForm);

    // --- Wand's own group -------------------------------------------------
    wandGroup_ = new QWidget(container);
    wandGroup_->setObjectName(QStringLiteral("wandGroup"));
    auto* wandForm = new QFormLayout(wandGroup_);
    wandForm->setContentsMargins(0, 0, 0, 0);

    wandToleranceSpinBox_ = new QDoubleSpinBox(wandGroup_);
    wandToleranceSpinBox_->setObjectName(QStringLiteral("wandToleranceSpinBox"));
    wandToleranceSpinBox_->setRange(0.0, 100.0);
    wandToleranceSpinBox_->setSuffix(QStringLiteral("%"));
    wandToleranceSpinBox_->setValue(10.0);  // Matches SelectionController's own default.
    connect(wandToleranceSpinBox_, &QDoubleSpinBox::valueChanged, this,
            &SelectionConfigurationPanel::wandToleranceChanged);
    wandForm->addRow(tr("Tolerance:"), wandToleranceSpinBox_);

    wandHarmonicsAwareCheckBox_ = new QCheckBox(tr("Harmonics-aware"), wandGroup_);
    wandHarmonicsAwareCheckBox_->setObjectName(QStringLiteral("wandHarmonicsAwareCheckBox"));
    connect(wandHarmonicsAwareCheckBox_, &QCheckBox::toggled, this,
            &SelectionConfigurationPanel::wandHarmonicsAwareChanged);
    wandForm->addRow(wandHarmonicsAwareCheckBox_);

    root->addWidget(wandGroup_);

    // --- Paste Blend Mode --------------------------------------------------
    auto* pasteForm = new QFormLayout();
    pasteBlendModeCombo_ = new QComboBox(container);
    pasteBlendModeCombo_->setObjectName(QStringLiteral("pasteBlendModeCombo"));
    for (const auto& [mode, name] : kPasteBlendModes) {
        pasteBlendModeCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(mode)));
    }
    connect(pasteBlendModeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit pasteBlendModeChanged(static_cast<sound_mind::core::BlendMode>(pasteBlendModeCombo_->itemData(index).toInt()));
    });
    pasteForm->addRow(tr("Paste Blend Mode:"), pasteBlendModeCombo_);
    root->addLayout(pasteForm);

    // Real-world testing pass, 2026-09-20, finding #6: the underlying
    // action already existed (Edit -> Deselect, Ctrl+D), but nothing
    // surfaced it from this panel itself while actually configuring a
    // selection.
    auto* deselectButton = new QPushButton(tr("Deselect"), container);
    deselectButton->setObjectName(QStringLiteral("deselectButton"));
    connect(deselectButton, &QPushButton::clicked, this, &SelectionConfigurationPanel::deselectRequested);
    root->addWidget(deselectButton);

    setWidget(container);
    updateWandGroupVisibility();
}

SelectionShape SelectionConfigurationPanel::selectionShape() const {
    return static_cast<SelectionShape>(selectionTypeCombo_->currentData().toInt());
}

double SelectionConfigurationPanel::wandTolerance() const { return wandToleranceSpinBox_->value(); }

bool SelectionConfigurationPanel::wandHarmonicsAware() const { return wandHarmonicsAwareCheckBox_->isChecked(); }

sound_mind::core::BlendMode SelectionConfigurationPanel::pasteBlendMode() const {
    return static_cast<sound_mind::core::BlendMode>(pasteBlendModeCombo_->currentData().toInt());
}

void SelectionConfigurationPanel::setPasteBlendMode(sound_mind::core::BlendMode mode) {
    const QSignalBlocker blocker(pasteBlendModeCombo_);
    const int index = pasteBlendModeCombo_->findData(QVariant::fromValue(static_cast<int>(mode)));
    if (index >= 0) {
        pasteBlendModeCombo_->setCurrentIndex(index);
    }
}

void SelectionConfigurationPanel::updateWandGroupVisibility() {
    wandGroup_->setVisible(selectionShape() == SelectionShape::Wand);
}

}  // namespace sound_mind::studio
