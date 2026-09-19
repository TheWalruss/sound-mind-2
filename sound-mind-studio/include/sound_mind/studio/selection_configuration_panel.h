#pragma once

#include <QDockWidget>

#include "sound_mind/core/blend_mode.h"
#include "sound_mind/studio/selection_controller.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QWidget;

namespace sound_mind::studio {

/**
 * @brief A dockable panel choosing which shape Select mode draws next, and
 *        Wand's own parameters when that's the shape chosen -
 *        `docs/sound-mind-design.md`'s "Selection", `v0.Y.35.1`
 *        Installments A/B.
 *
 * Deliberately its own panel, separate from `ToolConfigurationPanel`:
 * Select is its own `CanvasWidget::ToolMode`, independent of Paint's own
 * Tool Type - conflating the two into one panel would mean showing
 * Paint-only controls (Falloff, Brush Size, Color, Opacity, ...) while
 * Select mode is active, where none of them apply at all. Mirrors
 * `ToolConfigurationPanel`'s own "one dropdown selects which shape the
 * current tool draws, plus a per-type group for that shape's own
 * parameters" pattern - `wandGroup_` (Tolerance/Harmonics-aware) is shown
 * only while Selection Type is Wand, the same way `ToolConfigurationPanel`'s
 * own per-type groups (Instrument/Mind Shot/Mind Grain/Order-Chaos) work.
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
    ///        `SelectionController`'s own default - with `wandGroup_`
    ///        hidden accordingly.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit SelectionConfigurationPanel(QWidget* parent = nullptr);

    /// @brief The currently-selected shape.
    /// @return The Selection Type dropdown's own current value.
    [[nodiscard]] SelectionShape selectionShape() const;

    /// @brief Wand's own current tolerance.
    /// @return The Tolerance spin box's own current value (`0`-`100`).
    [[nodiscard]] double wandTolerance() const;

    /// @brief Whether Wand's own Harmonics-aware checkbox is checked.
    /// @return `true` if checked.
    [[nodiscard]] bool wandHarmonicsAware() const;

    /**
     * @brief Which blend mode `Edit -> Paste` should use - `v0.Y.37.1`
     *        (Deferred Blend Modes). Paste has no other tool configuration
     *        panel of its own, so this lives here rather than in
     *        `ToolConfigurationPanel`.
     *
     * Read on demand at the moment of a Paste (`MainWindow`'s own `Edit ->
     * Paste` handler), not pushed live to a controller the way
     * `selectionShapeChanged()`/`wandToleranceChanged()`/
     * `wandHarmonicsAwareChanged()` are - a blend mode only matters at the
     * instant a paste actually happens, unlike Selection Type/Wand's own
     * parameters, which affect an in-progress selection as it's drawn.
     *
     * @return The Paste Blend Mode dropdown's own current value;
     *         `sound_mind::core::BlendMode::Overwrite` by default,
     *         matching `PasteOperation`'s own pre-`v0.Y.37.1` behavior.
     */
    [[nodiscard]] sound_mind::core::BlendMode pasteBlendMode() const;

signals:
    /// @brief Emitted whenever the Selection Type dropdown changes - a
    ///        listener (`MainWindow`, in particular) forwards this
    ///        straight to `ToolPaletteController::setSelectionShape()`.
    /// @param shape The newly-selected shape.
    void selectionShapeChanged(SelectionShape shape);

    /// @brief Emitted whenever the Tolerance spin box changes - forwards
    ///        to `ToolPaletteController::setWandTolerance()`.
    /// @param tolerancePercent The new value.
    void wandToleranceChanged(double tolerancePercent);

    /// @brief Emitted whenever the Harmonics-aware checkbox changes -
    ///        forwards to `ToolPaletteController::setWandHarmonicsAware()`.
    /// @param harmonicsAware The new value.
    void wandHarmonicsAwareChanged(bool harmonicsAware);

private:
    /// @brief Shows wandGroup_ only when the Selection Type dropdown is
    ///        currently Wand - called from the constructor and whenever
    ///        the dropdown changes.
    void updateWandGroupVisibility();

    QComboBox* selectionTypeCombo_ = nullptr;
    QWidget* wandGroup_ = nullptr;
    QDoubleSpinBox* wandToleranceSpinBox_ = nullptr;
    QCheckBox* wandHarmonicsAwareCheckBox_ = nullptr;
    QComboBox* pasteBlendModeCombo_ = nullptr;
};

}  // namespace sound_mind::studio
