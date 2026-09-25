#pragma once

#include <QWidget>

#include "sound_mind/core/gradient.h"

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace sound_mind::studio {

class GradientBarWidget;

/**
 * @brief A complete, reusable gradient editor - `GradientBarWidget`'s own
 *        draggable bar, plus the currently-selected stop's four value
 *        spin boxes, a Link Channels checkbox, and a Delete Stop button -
 *        real-world testing pass, 2026-09-20, finding #17.
 *
 * This is the one shared widget every gradient-editing surface in the
 * app now embeds - `ToolConfigurationPanel` (a paint stroke's own
 * gradient), `FilterConfigurationPanel`'s Frequency-Axis Gradient section
 * and its Equalizer Cut section, and Fill's own gradient picker - the
 * same "one shared model, one shared widget" principle
 * `docs/sound-mind-design.md`'s "Gradients" section already establishes
 * for the underlying data model. Replaces each of those surfaces' own
 * previous two-endpoint-only spin-box pairs (or, for
 * `ToolConfigurationPanel`, a single `QColorDialog` button) - see this
 * class's own embedding call sites for what each one used to show
 * instead.
 *
 * Purely presentational, the same division of responsibility every other
 * configuration control in this codebase already draws: every edit (a
 * drag/insert/remove on the bar, or a spin-box/checkbox change here)
 * emits gradientChanged() with this widget's own new, complete gradient;
 * nothing here writes into a `FilterConfiguration`/`ToolConfiguration`
 * directly.
 *
 * **Link Channels**: when checked, editing any one of Left
 * Intensity/Opacity mirrors the same value onto Right Intensity/Opacity
 * (and vice versa) - `Gradient::linkChannels()`'s own UI-editing
 * convenience flag, with no effect on `evaluate()` itself (see its own
 * docs). Toggling it on doesn't retroactively unify values that already
 * differ - it only applies to edits made *after* it's checked, matching
 * `Gradient::setLinkChannels()`'s own docs precisely.
 */
class GradientEditorWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief Builds the editor with the default fully-transparent
    ///        two-stop gradient (`sound_mind::core::Gradient{}`).
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit GradientEditorWidget(QWidget* parent = nullptr);

    /// @brief This editor's own current gradient.
    /// @return The current gradient.
    [[nodiscard]] const sound_mind::core::Gradient& gradient() const noexcept;

    /**
     * @brief Loads a gradient into the editor - the actual work behind
     *        switching to a different Filter/Paint tool configuration.
     *
     * Deliberately does *not* emit gradientChanged() - the same
     * "loading is a sync from some other source, not a user edit"
     * reasoning `GradientBarWidget::setGradient()`'s own docs already
     * give.
     *
     * @param gradient The new gradient.
     */
    void setGradient(sound_mind::core::Gradient gradient);

    /**
     * @brief Switches between the normal editor (both Intensity and
     *        Opacity fields, for every filter that shows a real gradient)
     *        and the Equalizer's own "Cut" mode - real-world testing
     *        pass, 2026-09-20, finding #17, preserving Decision #61's own
     *        already-confirmed Cut semantics exactly (`FilterConfigurationPanel`'s
     *        own docs): both Intensity spin boxes hide, the Opacity ones
     *        relabel to "Cut", and every edit forces both channels'
     *        `leftIntensity`/`rightIntensity` to the silence floor
     *        (`-96` dB) regardless of whatever they were previously -
     *        "Cut" is opacity alone, always writing silence underneath,
     *        never a real color/loudness.
     *
     * A pure display-mode switch, the same as `setGradient()`'s own
     * "loading, not editing" contract - doesn't itself emit
     * gradientChanged(), even though the very next edit in Cut mode will
     * force intensity to the silence floor (a real, if usually invisible,
     * value change) the moment the user actually edits something.
     *
     * @param cutMode `true` for Cut mode; `false` for the normal editor.
     */
    void setCutMode(bool cutMode);

    /**
     * @brief Hides (or reshows) both Intensity fields without Cut mode's
     *        own Equalizer-specific relabeling or silence-floor forcing -
     *        `ToolConfigurationPanel`'s own Heal/Soften/Smudge/OrderChaos
     *        tool types, whose blend never reads intensity at all (only
     *        opacity, as blend strength - see each `ToolConfiguration`
     *        subtype's own docs), but which still call the remaining
     *        Opacity fields "Opacity", not "Cut" - unlike the Equalizer,
     *        nothing here is being attenuated toward silence.
     *
     * Independent of `setCutMode()` - both simply gate whether the
     * Intensity fields are shown (`cutMode_ || !intensityVisible_` hides
     * them); neither call forces the other's own state. Doesn't itself
     * touch the currently-held intensity values - a hidden field's own
     * value is simply left as whatever it already was, since nothing
     * reads it while hidden either way.
     *
     * @param visible `false` to hide both Intensity fields (and their own
     *        labels); `true` to show them.
     */
    void setIntensityVisible(bool visible);

signals:
    /// @brief Emitted whenever any control here changes the gradient.
    /// @param gradient This widget's own new, complete gradient.
    void gradientChanged(const sound_mind::core::Gradient& gradient);

private:
    /// @brief Syncs the four value spin boxes (and the Delete Stop
    ///        button's own enabled state) to `bar_`'s newly selected
    ///        stop.
    void handleSelectionChanged(std::size_t index);

    /// @brief Applies an edited spin-box value onto `bar_`'s own
    ///        currently selected stop, mirroring it onto the opposite
    ///        channel first if `linkChannelsCheckBox_` is checked - see
    ///        this class's own docs.
    void applyEditedStopValues();

    /// @brief Applies `cutMode_`/`intensityVisible_` to both Intensity
    ///        fields' own (and their labels') visibility - see
    ///        setCutMode()'s/setIntensityVisible()'s own docs.
    void updateIntensityFieldVisibility();

    GradientBarWidget* bar_;
    QDoubleSpinBox* leftIntensitySpinBox_;
    QDoubleSpinBox* leftOpacitySpinBox_;
    QDoubleSpinBox* rightIntensitySpinBox_;
    QDoubleSpinBox* rightOpacitySpinBox_;
    QLabel* leftIntensityLabel_ = nullptr;
    QLabel* rightIntensityLabel_ = nullptr;
    QLabel* leftOpacityLabel_ = nullptr;
    QLabel* rightOpacityLabel_ = nullptr;
    QCheckBox* linkChannelsCheckBox_;
    QPushButton* deleteStopButton_;
    bool cutMode_ = false;
    bool intensityVisible_ = true;
};

}  // namespace sound_mind::studio
