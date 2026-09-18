#include "sound_mind/studio/selection_configuration_panel.h"

#include <array>
#include <utility>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
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

    setWidget(container);
    updateWandGroupVisibility();
}

SelectionShape SelectionConfigurationPanel::selectionShape() const {
    return static_cast<SelectionShape>(selectionTypeCombo_->currentData().toInt());
}

double SelectionConfigurationPanel::wandTolerance() const { return wandToleranceSpinBox_->value(); }

bool SelectionConfigurationPanel::wandHarmonicsAware() const { return wandHarmonicsAwareCheckBox_->isChecked(); }

void SelectionConfigurationPanel::updateWandGroupVisibility() {
    wandGroup_->setVisible(selectionShape() == SelectionShape::Wand);
}

}  // namespace sound_mind::studio
