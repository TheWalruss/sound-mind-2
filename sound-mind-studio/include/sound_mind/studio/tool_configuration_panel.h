#pragma once

#include <memory>
#include <vector>

#include <QColor>
#include <QDockWidget>

#include "sound_mind/core/tool_configuration.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;
class QVBoxLayout;
class QWidget;

namespace sound_mind::core {
class Project;
}  // namespace sound_mind::core

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
 * **Three real tool types as of `v0.Y.33.1` Installment A (Mind Shots):**
 * a `ToolType` selector (`toolTypeCombo_`) switches between `Procedural`'s
 * own group (tip shape), `Instrument`'s own (harmonic count/strengths,
 * inharmonicity), and `MindShot`'s own (a picker over the project's own
 * `Project::mindShots()` library) - the design doc's own "only the
 * parameters that apply to the current tool" dynamism, one group shown at
 * a time, the rest hidden, the same pattern `MindWaveEditor`/
 * `FilterConfigurationPanel` already establish for their own per-type
 * groups. `falloff()`/`size()`/`stampMode()`/`stampInterval()`/color/
 * opacity are shared by every tool type (`ToolConfiguration`'s own base
 * fields) and stay visible regardless of which is selected - though a Mind
 * Shot stamp doesn't actually use falloff/size for anything (it's a hard,
 * native-size overwrite, see `MindShotConfiguration`'s own docs); they
 * stay visible anyway rather than hidden per-type, since nothing about
 * this panel's own "one group per type" mechanism needs to extend to the
 * *shared* controls too. `MindGrain`/`Smudge`/`OrderChaos`/`Heal`/`Soften`/
 * `Clone` still have no real parameters of their own (see
 * `ToolConfiguration`'s own docs) and aren't offered in the selector yet.
 *
 * **Needs a live `Project*` for the Mind Shot picker alone** (`setProject()`)
 * - every other control here is purely presentational, with no knowledge
 * of `Project` at all; the Mind Shot group is the first exception, since
 * "which Mind Shots exist to choose from" is real project state. Call
 * refreshMindShots() whenever that state might have changed out from under
 * this panel (a new capture, a project switch) to repopulate the picker.
 *
 * Switching `toolTypeCombo_` constructs a fresh configuration of the
 * newly-selected concrete subtype, carrying over every shared base field
 * from the one just displayed (so falloff/size/stamp settings/color survive
 * a type switch) - only the outgoing type's own subtype-specific fields
 * (e.g. `tipShape()`) are lost, replaced by the incoming type's own
 * defaults.
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
        return *config_;
    }

    /**
     * @brief Loads a configuration into the panel's own controls -
     *        reopening Tool Configuration pre-filled with a Picked
     *        object's own settings (see `docs/sound-mind-design.md`'s
     *        "Pick") is exactly this.
     *
     * Deliberately does *not* emit toolConfigurationChanged(): loading an
     * existing configuration is a sync *from* some other source (a Picked
     * object), not a user edit - emitting here would make `MainWindow`'s
     * own wiring indistinguishable from a real change and re-apply the
     * unchanged configuration right back onto whatever was just picked,
     * spamming the operation log with no-op edits on every single click.
     * The very next real control interaction still emits normally.
     *
     * @param config The configuration to display.
     */
    void setToolConfiguration(const sound_mind::core::ToolConfiguration& config);

    /**
     * @brief Sets which project the Mind Shot picker draws its own entries
     *        from - the only state on this otherwise purely presentational
     *        panel that needs one (see the class's own docs).
     *
     * Repopulates the picker immediately (see refreshMindShots()'s own
     * docs); does not otherwise touch `config_`.
     *
     * @param project The project to read `mindShots()` from; may be
     *        `nullptr` (the picker shows nothing selectable until a real
     *        one is set again).
     */
    void setProject(sound_mind::core::Project* project);

    /**
     * @brief Repopulates the Mind Shot picker from `project`'s own current
     *        `mindShots()` - call whenever that library might have changed
     *        out from under this panel (a fresh capture, in particular;
     *        `setProject()` already calls this itself for a project
     *        switch).
     *
     * Preserves the currently-selected entry, by id, if it still exists;
     * otherwise leaves nothing selected. Purely a display refresh, like
     * setToolConfiguration() - never itself changes `config_`, so an
     * already-configured `MindShotConfiguration`'s own `clip()` is
     * unaffected either way (it's a snapshot, not a live reference into
     * this library - see that class's own docs).
     */
    void refreshMindShots();

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

    /// @brief Syncs stampIntervalSpinBox_'s own suffix/tooltip to
    ///        config_'s current `stampMode()`, and disables it entirely
    ///        while that mode is `Stroke` (where an interval is
    ///        meaningless - see `ToolConfiguration::stampInterval()`'s
    ///        own docs) - called after any change to config_'s stamp
    ///        mode, so the spin box never shows a stale/wrong unit.
    void updateStampIntervalAppearance();

    /// @brief `toolTypeCombo_`'s own `currentIndexChanged` handler:
    ///        constructs a fresh `ProceduralConfiguration`/
    ///        `InstrumentConfiguration`/`MindShotConfiguration` for the
    ///        newly-selected `ToolType`, carrying over every shared base
    ///        field from `config_`'s current value first (see the class's
    ///        own docs), replaces `config_` with it, refreshes every
    ///        control (including which per-type group is visible), and
    ///        emits toolConfigurationChanged(). Switching to `MindShot`
    ///        selects `mindShotCombo_`'s own current row, if any (an
    ///        unconfigured `MindShotConfiguration` - no clip - otherwise).
    /// @param type The newly-selected tool type.
    void changeToolType(sound_mind::core::ToolType type);

    /// @brief Shows exactly one of `proceduralGroup_`/`instrumentGroup_`/
    ///        `mindShotGroup_` - whichever matches `config_->type()` - and
    ///        hides the other two, the same "one group per type" pattern
    ///        `MindWaveEditor`/`FilterConfigurationPanel` already
    ///        establish for their own per-type groups.
    void updateVisibleToolTypeGroup();

    /// @brief `mindShotCombo_`'s own `currentIndexChanged` handler: if
    ///        `config_` is currently a `MindShotConfiguration`, sets its
    ///        clip from the newly-selected library entry and emits
    ///        toolConfigurationChanged() - a no-op (no config_ update) if
    ///        the selection is the placeholder "no entries" item, or
    ///        `project_` is `nullptr`.
    /// @param index The combo's own newly-selected row.
    void handleMindShotComboChanged(int index);

    /// @brief Rebuilds `harmonicStrengthSpinBoxes_` to match `count` rows -
    ///        called whenever `harmonicCountSpinBox_` changes, or a loaded
    ///        `InstrumentConfiguration` has a different harmonic count
    ///        than the panel currently shows. Preserves each already-
    ///        displayed row's own current value where a row at that index
    ///        already existed; a newly-added row starts at `1.0`.
    /// @param count The new number of harmonic strength rows to show.
    void rebuildHarmonicStrengthRows(std::size_t count);

    /// @brief Reads every one of `harmonicStrengthSpinBoxes_`'s own
    ///        current values, in order.
    /// @return The harmonic strengths currently displayed.
    [[nodiscard]] std::vector<double> currentHarmonicStrengths() const;

    std::unique_ptr<sound_mind::core::ToolConfiguration> config_;

    QComboBox* toolTypeCombo_ = nullptr;

    /// @brief `ProceduralConfiguration`'s own controls, shown only while
    ///        `config_->type() == ToolType::Procedural` - see
    ///        updateVisibleToolTypeGroup()'s own docs.
    QWidget* proceduralGroup_ = nullptr;
    QComboBox* tipShapeCombo_ = nullptr;

    /// @brief `InstrumentConfiguration`'s own controls, shown only while
    ///        `config_->type() == ToolType::Instrument` - see
    ///        updateVisibleToolTypeGroup()'s own docs.
    QWidget* instrumentGroup_ = nullptr;
    QSpinBox* harmonicCountSpinBox_ = nullptr;
    QVBoxLayout* harmonicStrengthsLayout_ = nullptr;
    std::vector<QDoubleSpinBox*> harmonicStrengthSpinBoxes_;
    QDoubleSpinBox* inharmonicitySpinBox_ = nullptr;

    /// @brief `MindShotConfiguration`'s own controls, shown only while
    ///        `config_->type() == ToolType::MindShot` - see
    ///        updateVisibleToolTypeGroup()'s own docs.
    QWidget* mindShotGroup_ = nullptr;
    QComboBox* mindShotCombo_ = nullptr;

    /// @brief Which project `mindShotCombo_`'s own entries are drawn from -
    ///        see setProject()'s own docs. Not owned; may be `nullptr`.
    sound_mind::core::Project* project_ = nullptr;

    QDoubleSpinBox* falloffSpinBox_ = nullptr;
    QDoubleSpinBox* sizeSpinBox_ = nullptr;
    QComboBox* stampModeCombo_ = nullptr;
    QDoubleSpinBox* stampIntervalSpinBox_ = nullptr;
    QPushButton* colorButton_ = nullptr;
    QDoubleSpinBox* opacitySpinBox_ = nullptr;
    QCheckBox* showBoundingBoxesCheckBox_ = nullptr;
    QCheckBox* showPathGeometryCheckBox_ = nullptr;
};

}  // namespace sound_mind::studio
