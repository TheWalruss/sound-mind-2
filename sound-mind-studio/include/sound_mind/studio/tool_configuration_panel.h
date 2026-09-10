#pragma once

#include <QColor>
#include <QDockWidget>

#include "sound_mind/core/tool_configuration.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;

namespace sound_mind::studio {

/**
 * @brief A dockable panel for configuring the current painting tool - see
 *        `docs/sound-mind-design.md`'s "Tool Configuration".
 *
 * **Deliberately partial, for now**: the design doc describes a Wizard
 * button and a "Tool Preset" drop-down at the top of this panel, loading/
 * saving named configurations to/from the project - neither exists yet
 * (no Wizard has been built, and `Project` has no saved-preset list to
 * populate a drop-down from), so both are omitted entirely rather than
 * shown as dead controls, matching this codebase's own established
 * "don't build placeholder UI for a feature that doesn't work yet"
 * philosophy. What's here is the actual parameter area the design doc
 * says both entry points edit - just reached directly, by hand, for now.
 *
 * Also deliberately Procedural-only: the design doc's own "only the
 * parameters that apply to the current tool" dynamism has nothing to
 * dynamically switch between yet, since `Instrument`/`MindShot`/
 * `MindGrain`/`Smudge`/`OrderChaos`/`Heal`/`Soften`/`Clone` have no real
 * parameters of their own (see `ToolConfiguration`'s own docs) - a
 * `ToolType` selector will make sense once a second tool type actually
 * has fields to show.
 *
 * Purely presentational, the same division of responsibility as every
 * other dock panel: every edit emits toolConfigurationChanged() with the
 * panel's own current, complete `ToolConfiguration` - `MainWindow` is
 * what actually threads it into `PaintController`.
 *
 * **Color, not a separate "Intensity" control**: per
 * `docs/sound-mind-design.md`'s own "Color - stereo balance" framing, a
 * single RGB color swatch (red = left channel, green = right channel)
 * replaces what an earlier version of this panel exposed as a single
 * "Intensity" spin box driving both channels identically - see
 * setColor()'s own docs for the exact conversion.
 */
class ToolConfigurationPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief Builds the panel with a default `ToolConfiguration` (a
    ///        plain circular Procedural brush) and both overlay
    ///        checkboxes off, matching the design doc's own defaults.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit ToolConfigurationPanel(QWidget* parent = nullptr);

    /// @brief The tool configuration this panel's controls currently
    ///        describe.
    /// @return The current configuration.
    [[nodiscard]] const sound_mind::core::ToolConfiguration& toolConfiguration() const noexcept {
        return config_;
    }

    /**
     * @brief Sets the brush's color (stereo balance) directly - the
     *        testable core behind the "Color" swatch button's own
     *        QColorDialog, per `docs/sound-mind-design.md`'s "Color -
     *        stereo balance (left channel is one color, right channel
     *        another)": the color's red channel becomes the left
     *        channel's intensity, green becomes the right channel's,
     *        both applied to *both* of `config_`'s gradient stops
     *        uniformly (blue is unused - painting doesn't touch phase
     *        yet). Updates the swatch's own displayed color and emits
     *        toolConfigurationChanged().
     * @param color The color to apply.
     */
    void setColor(QColor color);

    /// @brief The color the panel's controls currently describe - the
    ///        exact inverse of setColor(), derived from `config_`'s own
    ///        current gradient stop intensities.
    /// @return The current color, per setColor()'s own red=left/
    ///         green=right convention (blue always `0`).
    [[nodiscard]] QColor color() const;

signals:
    /// @brief Emitted whenever any parameter control changes.
    /// @param config The panel's own new, complete configuration.
    void toolConfigurationChanged(const sound_mind::core::ToolConfiguration& config);

    /// @brief The "Show bounding boxes" checkbox changed.
    /// @param shown The new checked state.
    void showBoundingBoxesChanged(bool shown);

    /// @brief The "Show path geometry" checkbox changed.
    /// @param shown The new checked state.
    void showPathGeometryChanged(bool shown);

private:
    /// @brief Emits toolConfigurationChanged() with the current config_.
    void emitConfigChanged();

    /// @brief `colorButton_`'s own `clicked()` handler - shows a real,
    ///        modal `QColorDialog` seeded with color(), and calls
    ///        setColor() with the result if the user accepts it (a no-op
    ///        if cancelled). Kept separate from setColor() itself so
    ///        tests can call the latter directly without ever having to
    ///        drive a real modal dialog - the same "testable core, plus a
    ///        thin dialog-showing wrapper" shape `MainWindow`'s own
    ///        import/export actions already use.
    void openColorDialog();

    /// @brief Syncs colorButton_'s own displayed swatch (background fill
    ///        and hex-code text) to color()'s current value - called
    ///        after any change to config_'s color, so the button never
    ///        shows a stale swatch.
    void updateColorButtonAppearance();

    sound_mind::core::ToolConfiguration config_;
    QComboBox* tipShapeCombo_ = nullptr;
    QDoubleSpinBox* falloffSpinBox_ = nullptr;
    QDoubleSpinBox* sizeSpinBox_ = nullptr;
    QPushButton* colorButton_ = nullptr;
    QDoubleSpinBox* opacitySpinBox_ = nullptr;
    QCheckBox* showBoundingBoxesCheckBox_ = nullptr;
    QCheckBox* showPathGeometryCheckBox_ = nullptr;
};

}  // namespace sound_mind::studio
