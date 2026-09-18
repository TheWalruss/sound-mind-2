#include "sound_mind/studio/selection_configuration_panel.h"

#include <array>
#include <utility>

#include <QComboBox>
#include <QFormLayout>
#include <QVariant>
#include <QWidget>

namespace sound_mind::studio {

namespace {

/// @brief The Selection Type dropdown's own entries, in display order -
/// the same `{enumerator, display name}` table shape
/// `tool_configuration_panel.cpp`'s own `kToolTypes` already establishes.
constexpr std::array<std::pair<SelectionShape, const char*>, 2> kSelectionShapes = {{
    {SelectionShape::Rectangle, "Rectangle"},
    {SelectionShape::Lasso, "Lasso"},
}};

}  // namespace

SelectionConfigurationPanel::SelectionConfigurationPanel(QWidget* parent)
    : QDockWidget(tr("Selection Configuration"), parent) {
    auto* container = new QWidget(this);
    auto* form = new QFormLayout(container);

    selectionTypeCombo_ = new QComboBox(container);
    selectionTypeCombo_->setObjectName(QStringLiteral("selectionTypeCombo"));
    for (const auto& [shape, name] : kSelectionShapes) {
        selectionTypeCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(shape)));
    }
    connect(selectionTypeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit selectionShapeChanged(static_cast<SelectionShape>(selectionTypeCombo_->itemData(index).toInt()));
    });
    form->addRow(tr("Selection Type:"), selectionTypeCombo_);

    setWidget(container);
}

SelectionShape SelectionConfigurationPanel::selectionShape() const {
    return static_cast<SelectionShape>(selectionTypeCombo_->currentData().toInt());
}

}  // namespace sound_mind::studio
