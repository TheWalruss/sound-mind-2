#pragma once

#include <QLabel>
#include <QString>

namespace sound_mind::studio {

/**
 * @brief A one-line status label showing which named preset a panel's
 *        settings came from, and whether they have been changed since.
 *
 * Shared by every panel with a preset drop-down (Tool, Filter, Grid) so the
 * active preset reads the same everywhere: "Preset: Name" while the settings
 * still match what was loaded or saved, "Preset: Name * (modified)" in an
 * accent color once any setting is edited afterwards, and a muted "No preset
 * active" otherwise. Purely presentational - it holds no preset data and
 * never touches a Project.
 *
 * @note UI thread only.
 */
class PresetIndicatorLabel : public QLabel {
    Q_OBJECT

public:
    /// @brief Constructs the label in its "No preset active" state.
    /// @param parent Optional parent widget.
    explicit PresetIndicatorLabel(QWidget* parent = nullptr);

    /// @brief Marks `name` as the active, unmodified preset.
    /// @param name The preset's display name.
    void setActivePreset(const QString& name);

    /// @brief Returns to the "No preset active" state.
    void clearActivePreset();

    /// @brief Flags the active preset as modified (asterisk + accent color).
    ///        A no-op while no preset is active.
    void markModified();

    /// @brief Whether a preset is currently active.
    /// @return `true` while a preset name is shown.
    [[nodiscard]] bool hasActivePreset() const noexcept { return hasActive_; }

    /// @brief Whether the active preset has been modified since it was
    ///        loaded or saved. Always `false` while none is active.
    /// @return `true` once markModified() has run since the last setActivePreset().
    [[nodiscard]] bool isModified() const noexcept { return hasActive_ && modified_; }

    /// @brief The active preset's name; empty while none is active.
    /// @return The name last passed to setActivePreset().
    [[nodiscard]] const QString& activePresetName() const noexcept { return name_; }

private:
    void refresh();

    bool hasActive_ = false;
    bool modified_ = false;
    QString name_;
};

}  // namespace sound_mind::studio
