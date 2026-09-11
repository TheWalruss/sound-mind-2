#pragma once

#include <QDockWidget>

#include "sound_mind/studio/axis_labels.h"

class QComboBox;

namespace sound_mind::studio {

/**
 * @brief A dockable panel for configuring canvas overlays - Axis Labels
 *        for now, growing into Overlay Grids (Frequency/Timing) and Snap
 *        to Grid in later installments of the same `v0.Y.26.1` milestone
 *        (see `docs/sound-mind-design.md`'s "Overlay Grids"/"Axis
 *        Labels") - one panel for every canvas-overlay setting, rather
 *        than the legacy Studio's own split across a View menu, a Tools
 *        panel checkbox, and a separate Overlay dock.
 *
 * Purely presentational, the same division of responsibility as every
 * other dock panel (`ToolConfigurationPanel`, in particular): every
 * control change emits a signal carrying the panel's own new value -
 * `MainWindow` is what actually threads it into `CanvasWidget`.
 */
class GridPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief Builds the panel with both axis labels off, matching
    ///        `CanvasWidget`'s own defaults.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit GridPanel(QWidget* parent = nullptr);

    /// @brief The frequency (vertical) axis label mode the panel's own
    ///        combo box currently shows.
    /// @return The current mode.
    [[nodiscard]] VerticalAxisLabelMode verticalAxisLabelMode() const noexcept { return verticalAxisLabelMode_; }

    /// @brief The time (horizontal) axis label mode the panel's own
    ///        combo box currently shows.
    /// @return The current mode.
    [[nodiscard]] HorizontalAxisLabelMode horizontalAxisLabelMode() const noexcept {
        return horizontalAxisLabelMode_;
    }

signals:
    /// @brief The "Vertical axis" combo box changed.
    /// @param mode The newly selected mode.
    void verticalAxisLabelModeChanged(sound_mind::studio::VerticalAxisLabelMode mode);

    /// @brief The "Horizontal axis" combo box changed.
    /// @param mode The newly selected mode.
    void horizontalAxisLabelModeChanged(sound_mind::studio::HorizontalAxisLabelMode mode);

private:
    VerticalAxisLabelMode verticalAxisLabelMode_ = VerticalAxisLabelMode::Off;
    HorizontalAxisLabelMode horizontalAxisLabelMode_ = HorizontalAxisLabelMode::Off;
    QComboBox* verticalAxisCombo_ = nullptr;
    QComboBox* horizontalAxisCombo_ = nullptr;
};

}  // namespace sound_mind::studio
