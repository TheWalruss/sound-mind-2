#pragma once

#include <optional>
#include <utility>
#include <vector>

#include <QDockWidget>
#include <QString>

#include "sound_mind/core/layer.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace sound_mind::studio {

/**
 * @brief A dockable panel exposing Recording's transport - see
 *        `docs/sound-mind-roadmap.md`'s Transport Panels milestone
 *        (`v0.Y.16.1`).
 *
 * Adapted from the legacy Studio's `RecordPanel`, narrowed to what this
 * codebase's Recording actually has scope for today: a Start/Stop button.
 *
 * **Input device picker removed (real-world testing pass, 2026-09-20,
 * finding #7)**: it duplicated Configure Devices' own input device combo
 * exactly, always kept in sync with it by `MainWindow` - pure redundancy,
 * not a distinct control. Configure Devices is now the single place to
 * change it; its own combo is disabled while Recording is active
 * (`ConfigureDevicesPanel::setInputDeviceSelectionEnabled()`), preserving
 * this panel's own prior "locked while recording" behavior.
 *
 * Purely presentational, the same division of responsibility as
 * `LayersPanel`/`LoopPanel`: every user action is a signal `MainWindow`
 * connects to its own handlers. Wrapped in a `QScrollArea` so its content
 * is never clipped, and never forces the dock wider than the window.
 *
 * **"Configure Devices" button (`v0.Y.58.1`, "Reduce top-level buttons"):**
 * a quick-access shortcut into `ConfigureDevicesPanel`, shared identically
 * by `PlaybackPanel`/`LoopPanel` - see `configureDevicesRequested()`'s own
 * docs. Replaces `ConfigureDevicesPanel`'s own now-removed standalone
 * toolbar toggle, folded into the new "Input/Output" dropdown instead.
 */
class RecordPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief Builds the panel.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit RecordPanel(QWidget* parent = nullptr);

    /// @brief Updates the Start/Stop button's label/checked state.
    /// @param recording Whether Recording is currently active.
    void setRecording(bool recording);

    /**
     * @brief Lists the project's recorded layers as targets to record over,
     *        after "New recording layer" (always first). Keeps the current
     *        choice if that layer is still listed; otherwise falls back to
     *        a new layer.
     * @param layers Each recorded layer's id and name, in display order.
     */
    void setRecordedLayers(const std::vector<std::pair<sound_mind::core::LayerId, QString>>& layers);

    /// @return The recorded layer to overwrite, or `std::nullopt` to record
    ///         into a new layer.
    [[nodiscard]] std::optional<sound_mind::core::LayerId> targetLayerId() const;

    /// @return The new layer's name, trimmed - "Recording" if left blank.
    [[nodiscard]] QString newLayerName() const;

signals:
    /// @brief The Start/Stop button was clicked.
    void toggleRequested();

    /// @brief The "Configure Devices" button was clicked - `MainWindow`
    ///        responds by showing/raising `configureDevicesPanel_` (the
    ///        same "panel emits, MainWindow shows/raises a different
    ///        dock" pattern `LayersPanel::editFilterRequested()` already
    ///        established).
    void configureDevicesRequested();

    /// @brief The "Live-encoding view" checkbox was toggled.
    /// @param enabled Whether to show the take being recorded live.
    void liveViewToggled(bool enabled);

private:
    QCheckBox* liveViewCheckBox_ = nullptr;
    QPushButton* toggleButton_ = nullptr;
    QComboBox* targetCombo_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QLabel* nameLabel_ = nullptr;
};

}  // namespace sound_mind::studio
