#pragma once

#include <QDockWidget>

#include "sound_mind/studio/selection_controller.h"

class QComboBox;

namespace sound_mind::studio {

/**
 * @brief A dockable panel choosing which shape Select mode draws next -
 *        `docs/sound-mind-design.md`'s "Selection", `v0.Y.35.1`
 *        Installment A.
 *
 * Deliberately its own panel, separate from `ToolConfigurationPanel`:
 * Select is its own `CanvasWidget::ToolMode`, independent of Paint's own
 * Tool Type - conflating the two into one panel would mean showing
 * Paint-only controls (Falloff, Brush Size, Color, Opacity, ...) while
 * Select mode is active, where none of them apply at all. Mirrors
 * `ToolConfigurationPanel`'s own "one dropdown selects which shape the
 * current tool draws" shape, at the (currently) much smaller scale a
 * single Selection Type dropdown needs - a future Wand installment would
 * grow this panel with its own per-type controls (a tolerance slider, a
 * harmonics-aware checkbox) the same way `ToolConfigurationPanel` grew
 * per-type groups for Instrument/Mind Shot/Mind Grain/Order-Chaos.
 *
 * Off (hidden) by default, alongside every other dockable panel
 * (`docs/sound-mind-design.md`'s own "off by default" convention) -
 * toggled via its own `toggleViewAction()`, added to the transport
 * toolbar the same way `ToolConfigurationPanel`'s own is.
 */
class SelectionConfigurationPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief Constructs a panel defaulted to `SelectionShape::Rectangle`
    ///        - the shape every selection used before Lasso existed, and
    ///        `SelectionController`'s own default.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit SelectionConfigurationPanel(QWidget* parent = nullptr);

    /// @brief The currently-selected shape.
    /// @return The Selection Type dropdown's own current value.
    [[nodiscard]] SelectionShape selectionShape() const;

signals:
    /// @brief Emitted whenever the Selection Type dropdown changes - a
    ///        listener (`MainWindow`, in particular) forwards this
    ///        straight to `ToolPaletteController::setSelectionShape()`.
    /// @param shape The newly-selected shape.
    void selectionShapeChanged(SelectionShape shape);

private:
    QComboBox* selectionTypeCombo_ = nullptr;
};

}  // namespace sound_mind::studio
