#pragma once

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <QDockWidget>
#include <QString>

#include "sound_mind/core/blend_mode.h"
#include "sound_mind/core/tool_configuration.h"
#include "sound_mind/core/tool_preset.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QVBoxLayout;
class QWidget;

namespace sound_mind::core {
class Project;
}  // namespace sound_mind::core

namespace sound_mind::studio {

class GradientEditorWidget;
class HarmonicSeriesWidget;

/**
 * @brief A dockable panel for configuring the current painting tool - see
 *        `docs/sound-mind-design.md`'s "Tool Configuration".
 *
 * **Still partial**: the design doc describes both a Wizard button and a
 * "Tool Preset" drop-down at the top of this panel, loading/saving named
 * configurations to/from the project - `v0.Y.55.1`'s own second
 * prerequisite (see `refreshToolPresets()`'s own docs) built the Tool
 * Preset half (`toolPresetCombo_`/`savePresetButton_`/`deletePresetButton_`),
 * but no Wizard has been built yet, so it's still omitted entirely rather
 * than shown as a dead control, matching this codebase's own established
 * "don't build placeholder UI for a feature that doesn't work yet"
 * philosophy.
 *
 * **Eight real tool types as of `v0.Y.34.1` Installment B (Smudge/Order-
 * Chaos):** a `ToolType` selector (`toolTypeCombo_`) switches between
 * `Procedural`'s own group (tip shape), `Instrument`'s own (harmonic count/
 * strengths, inharmonicity), `MindShot`'s own (a picker over the project's
 * own `Project::mindShots()` library), `MindGrain`'s own (a picker over
 * `Project::mindGrains()`, with a red-highlight/tooltip warning - see
 * setActiveLayer()'s own docs), `Heal`/`Soften`/`Smudge` (none of the three
 * adds a group of its own at all - see each `ToolConfiguration` subtype's
 * own docs on why every parameter either needs is already shared), and
 * `OrderChaos` (its own `orderChaosGroup_`, a single "Amount" spin box -
 * the first new field this milestone's tools have needed) - the design
 * doc's own "only the parameters that apply to the current tool" dynamism,
 * one group shown at a time, the rest hidden, the same pattern
 * `MindWaveEditor`/`FilterConfigurationPanel` already establish for their
 * own per-type groups. `falloff()`/`size()`/`stampMode()`/`stampInterval()`/
 * color/opacity are shared by every tool type (`ToolConfiguration`'s own
 * base fields) and stay visible regardless of which is selected - though a
 * Mind Shot/Mind Grain stamp doesn't actually use falloff/size for anything
 * (both are a hard, native-size overwrite, see `MindShotConfiguration`'s/
 * `MindGrainConfiguration`'s own docs), and a Heal/Soften/Smudge/OrderChaos
 * stamp doesn't use the Color swatch's own intensity for anything (only
 * Opacity, as blend strength - see `HealConfiguration`'s own docs); they
 * stay visible anyway rather than hidden per-type, since nothing about this
 * panel's own "one group per type" mechanism needs to extend to the
 * *shared* controls too. `Clone` still has no real parameters of its own
 * (see `ToolConfiguration`'s own docs) and isn't offered in the selector
 * yet.
 *
 * **Instrument's own harmonic series gets a real visual editor too**
 * (`v0.Y.58.1`'s "Instrument harmonic-series visual editor" item, the
 * next installment after the Equalizer's own `EqualizerCurveWidget`
 * closed out the item before it) - `harmonicSeriesWidget_`
 * (`HarmonicSeriesWidget`) draws `harmonicStrengthSpinBoxes_`'s own
 * values as a draggable bar chart, bidirectionally synced with those
 * spin boxes exactly the way `EqualizerCurveWidget`/`GradientEditorWidget`
 * already sync for the Equalizer's own Cut gradient; dragging a bar sets
 * that one harmonic's strength directly (no separate "select, then edit
 * elsewhere" step - a harmonic series is a fixed bank of independent
 * scalars, not a variable-length stop list, so there's nothing to
 * select). The same `HarmonicSeriesWidget::renderThumbnail()` that widget
 * exposes also gives `refreshToolPresets()`'s own `toolPresetCombo_` a
 * small icon next to every saved `Instrument`-type preset's own name,
 * showing its harmonic content at a glance - `docs/sound-mind-
 * roadmap.md`'s own "preview image shown alongside each saved instrument
 * in its own selection menu."
 *
 * **Needs a live `Project*` for the Mind Shot/Mind Grain pickers**
 * (`setProject()`) - every other control here is purely presentational,
 * with no knowledge of `Project` at all; those two groups are the
 * exception, since "which entries exist to choose from" is real project
 * state. Call refreshMindShots()/refreshMindGrains() whenever that state
 * might have changed out from under this panel (a new capture, a project
 * switch) to repopulate the pickers. **Also needs to know the current
 * active layer** (`setActiveLayer()`) - the second piece of live state
 * `MindGrainConfiguration`'s own ordering rule needs to warn about (see
 * that method's own docs).
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
 * **A real, multi-stop gradient, not a flat color swatch** - real-world
 * testing pass, 2026-09-20, finding #17: `gradientEditor_`
 * (`GradientEditorWidget`) edits `config_->defaultGradient()` directly,
 * replacing the single flat `QColorDialog`-driven Color/Opacity pair an
 * earlier version of this panel exposed (that pair only ever wrote both
 * of the gradient's two endpoint stops identically - see this class's own
 * prior revision for the exact conversion it used). A fresh stroke still
 * starts from a real, fully-opaque two-stop gradient (the same default
 * this panel always seeded), but the editor now lets a stroke's own
 * gradient vary continuously along its own length, the same "one shared
 * gradient model, one shared widget" principle
 * `docs/sound-mind-design.md`'s own "Gradients" section establishes -
 * `FilterConfigurationPanel`'s Frequency-Axis Gradient/Equalizer Cut
 * sections embed the identical widget over `FilterConfiguration`'s own
 * gradient. `updateSharedControlVisibility()`'s own docs cover which tool
 * types show it, and in which of `GradientEditorWidget::setCutMode()`'s/
 * `setIntensityVisible()`'s own display modes.
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
     * @brief Repopulates the Mind Grain picker from `project`'s own current
     *        `mindGrains()` - call whenever that library might have changed
     *        out from under this panel (a fresh capture, in particular;
     *        `setProject()` already calls this itself for a project
     *        switch).
     *
     * Preserves the currently-selected entry, by id, if it still exists;
     * otherwise leaves nothing selected. Purely a display refresh, like
     * refreshMindShots() - never itself changes `config_`. Also refreshes
     * the red-highlight/tooltip validity display (see setActiveLayer()'s
     * own docs), since a fresh capture can change which entries even exist
     * to check.
     */
    void refreshMindGrains();

    /**
     * @brief Repopulates the Resonance picker from `project`'s
     *        own current `resonanceProfiles()` - `v0.Y.59.1` Installment D,
     *        the same "call whenever that library might have changed out
     *        from under this panel" role `refreshMindShots()`/
     *        `refreshMindGrains()` already establish (`setProject()`
     *        already calls this itself for a project switch; a fresh
     *        `Edit -> Create Resonance from Picked Path...`
     *        capture is the other trigger, from `MainWindow`).
     *
     * Preserves the currently-selected entry, by id, if it still exists;
     * otherwise leaves nothing selected. Purely a display refresh, like
     * refreshMindShots() - never itself changes `config_`, so an already-
     * configured `ResonanceConfiguration`'s own `spectrum()` is
     * unaffected either way (it's a snapshot, not a live reference into
     * this library - see that class's own docs).
     */
    void refreshResonanceProfiles();

    /**
     * @brief Repopulates the Tool Preset picker from `project`'s own
     *        current `toolPresets()` - `v0.Y.55.1`'s own second
     *        prerequisite, the same "call whenever that library might have
     *        changed out from under this panel" role `refreshMindShots()`/
     *        `refreshMindGrains()` already establish (`setProject()`
     *        already calls this itself for a project switch;
     *        `saveCurrentAsToolPresetNamed()`/`deleteCurrentToolPreset()`
     *        call it too, after changing the library themselves).
     *
     * Preserves the currently-selected entry, by id, if it still exists;
     * otherwise leaves nothing selected. Each entry whose own saved
     * `config` is an `InstrumentConfiguration`/`ResonanceConfiguration`
     * also gets a small icon (`HarmonicSeriesWidget::renderThumbnail()`)
     * showing its harmonic/spectral content at a glance - every other
     * preset type shows no icon, the same way it always has.
     */
    void refreshToolPresets();

    /**
     * @brief Saves `config_`'s own current state as a new Tool Preset
     *        named `name`, in `project_`'s own library - the dialog-free
     *        half of the "Save..." button's own click handler (which
     *        gathers `name` from a `QInputDialog` first), the same split
     *        `MainWindow::renameLayerTo()`/`captureMindShotWithDetails()`
     *        already establish so a caller that already knows what it
     *        wants - in particular, this codebase's own test suite - never
     *        needs to trigger the real modal dialog itself.
     *
     * A no-op (returns `std::nullopt`) if `project_` is `nullptr` or `name`
     * is empty.
     *
     * @param name Display name for the new preset.
     * @return The new preset's own id, or `std::nullopt` if this was a
     *         no-op.
     */
    std::optional<sound_mind::core::ToolPresetId> saveCurrentAsToolPresetNamed(const QString& name);

    /**
     * @brief Repopulates `vibratoMindWaveCombo_`/`tremoloMindWaveCombo_` from
     *        the project's own current MindWave library - `v0.Y.39.1`
     *        Installment A's own `InstrumentConfiguration` vibrato/tremolo
     *        binding entry point, mirroring
     *        `FilterConfigurationPanel::setAvailableMindWaves()`'s exact
     *        role and shape (an id/name pair per library entry; this panel
     *        knows nothing about `Project`/`NamedMindWave` beyond that).
     *
     * Preserves each combo's own current selection, by id, if it's still
     * present among `mindWaves` - the same rule `rebuildMindWaveCombos()`'s
     * own docs establish; otherwise falls back to "None".
     *
     * @param mindWaves The project's current MindWave library, as id/name
     *        pairs.
     */
    void setAvailableMindWaves(const std::vector<std::pair<sound_mind::core::MindWaveId, QString>>& mindWaves);

    /**
     * @brief Sets which layer a freehand stroke started right now would
     *        paint into - the only other piece of live project state this
     *        otherwise purely presentational panel needs (see the class's
     *        own docs), specifically for the Mind Grain group's own red-
     *        highlight/tooltip warning (`docs/sound-mind-design.md`'s "Mind
     *        Grains" ordering rule: a Mind Grain can only paint onto a layer
     *        above its own source).
     *
     * A no-op on `config_` itself - purely a display refresh, like
     * setToolConfiguration()/refreshMindShots(). Recomputes the warning
     * immediately: if `config_` is currently a `MindGrainConfiguration` and
     * `layer` is not above its own `sourceLayerId()` (per
     * `sound_mind::core::isLayerAbove()`), `mindGrainGroup_` is shown with a
     * red background and a tooltip explaining why; otherwise the group
     * looks and behaves normally. Call whenever the active layer might have
     * changed (`LayersPanel`'s own selection changing) or whenever `config_`
     * itself changes (a type switch, a different Mind Grain picked, a fresh
     * `setToolConfiguration()` load).
     *
     * @param layer The layer a stroke would currently paint into.
     */
    void setActiveLayer(sound_mind::core::LayerId layer);

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

    /// @brief Syncs stampIntervalSpinBox_'s own suffix/tooltip to
    ///        config_'s current `stampMode()`, and disables it entirely
    ///        while that mode is `Stroke` (where an interval is
    ///        meaningless - see `ToolConfiguration::stampInterval()`'s
    ///        own docs) - called after any change to config_'s stamp
    ///        mode, so the spin box never shows a stale/wrong unit. Also
    ///        shows/hides stampIntervalPatternLineEdit_/
    ///        stampIntervalPatternErrorLabel_, visible only for
    ///        `StampMode::AlongCurve` - the only mode
    ///        `ToolConfiguration::stampIntervalPatternText()` actually
    ///        affects (`v0.Y.54.1` Paint Tool Enhancements Installment A;
    ///        see that method's own docs for why `TimeAxis`/`FrequencyAxis`
    ///        are deliberately excluded).
    void updateStampIntervalAppearance();

    /// @brief `stampIntervalPatternLineEdit_`'s own `textChanged` handler:
    ///        writes the raw text into `config_` unconditionally (so an
    ///        in-progress edit is never lost or silently reverted), then
    ///        attempts `sound_mind::core::parseStampIntervalPattern()`
    ///        purely to surface a validation error in
    ///        `stampIntervalPatternErrorLabel_` - the same "always store the
    ///        raw text, validate only to inform the user" pattern
    ///        `ChordGeneratorPanel::emitNotationChanged()` already
    ///        establishes for `parseSequenceNotation()`. A pattern that
    ///        fails to parse is never rejected here: painting itself already
    ///        falls back to the plain `stampInterval()` scalar for an
    ///        unparseable pattern (see `stampIntervalPatternText()`'s own
    ///        docs), so this handler's error label is purely informational.
    void updateStampIntervalPattern();

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
    ///        `mindShotGroup_`/`mindGrainGroup_`/`orderChaosGroup_` -
    ///        whichever matches `config_->type()` - and hides the rest, the
    ///        same "one group per type" pattern `MindWaveEditor`/
    ///        `FilterConfigurationPanel` already establish for their own
    ///        per-type groups. `Heal`/`Soften`/`Smudge` match none of the
    ///        five - all three leave every group hidden, since none of them
    ///        needs a group of its own (see `HealConfiguration`'s own
    ///        docs).
    void updateVisibleToolTypeGroup();

    /**
     * @brief Hides/shows Falloff/Brush Size/Stamp Mode/Stamp Interval/
     *        `gradientEditor_`'s own individual rows in
     *        `sharedControlsForm_` (via `QFormLayout::setRowVisible()`)
     *        according to which of them `config_`'s own current type
     *        actually consults - a review pass confirmed with the user
     *        (`v0.Y.34.1` Installment C): a control the active tool type
     *        never reads is hidden entirely, the same "don't build
     *        placeholder UI for something that doesn't do anything"
     *        philosophy this panel's own class docs already establish for
     *        the Wizard/Tool Preset drop-down, rather than left visible
     *        but silently inert. Where `gradientEditor_` itself stays
     *        visible, `GradientEditorWidget::setIntensityVisible()` further
     *        controls whether its own Intensity fields specifically are
     *        shown - see the bullets below.
     *
     * - **`MindShotConfiguration`/`MindGrainConfiguration`**: hides
     *   Falloff/Brush Size/`gradientEditor_` entirely - both stamp a
     *   captured/live `Clip` (`blitClipCentered()`), never reading
     *   `size()`/`falloff()`, and never touching the stroke's own gradient
     *   at all (opacity/intensity meaningless - see each class's own
     *   docs). Stamp Mode/Interval stay visible - still a real, meaningful
     *   placement choice for a repeated stamp. As of `v0.Y.37.1` (Deferred
     *   Blend Modes), `blendModeCombo_` is shown *only* for these two - the
     *   exact inverse of Falloff/Size/`gradientEditor_`'s own visibility
     *   here - since it's the one control these two types uniquely have
     *   that no other type does.
     * - **`FixedStampPlacementConfiguration`'s own four subtypes** (`Heal`/
     *   `Soften`/`Smudge`/`OrderChaos`): keeps `gradientEditor_` visible
     *   but with `setIntensityVisible(false)` - only the stroke's own
     *   gradient stop *opacity*, not intensity, feeds into their blend
     *   (see each one's own docs) - and hides Stamp Mode/Stamp Interval
     *   (forced to `AlongCurve`/`66%` of `size()` - see
     *   `FixedStampPlacementConfiguration`'s own docs on why showing a
     *   control the value can no longer actually change would be
     *   misleading, not just inert). Falloff/Brush Size/Opacity stay
     *   visible - all three are real, load-bearing parameters for every
     *   one of the four.
     * - **`ProceduralConfiguration`/`InstrumentConfiguration`**: every
     *   shared control stays visible, `gradientEditor_` with
     *   `setIntensityVisible(true)` - both genuinely use all of Falloff/
     *   Size/Stamp Mode/Interval/Intensity/Opacity.
     *
     * **`opacityMindWaveCombo_`/`sizeMindWaveCombo_`/`colorMindWaveCombo_`
     * (`v0.Y.54.1` Installment B) follow the same visibility as Falloff/
     * Size** - hidden for `MindShotConfiguration`/`MindGrainConfiguration`
     * (meaningless, per each binding's own docs on `ToolConfiguration`),
     * visible for every other type, including `FixedStampPlacementConfiguration`'s
     * own four subtypes (where `colorMindWaveCombo_` still narrows to
     * affecting only which stop's own *opacity* is read, the same "still
     * meaningful, just narrower" shape `gradientEditor_`'s own
     * `setIntensityVisible(false)` already establishes for those four).
     *
     * Called wherever `updateVisibleToolTypeGroup()` already is, right
     * alongside it - the same "config_'s type just changed" trigger.
     */
    void updateSharedControlVisibility();

    /// @brief `blendModeCombo_`'s own `currentIndexChanged` handler: if
    ///        `config_` is currently a `MindShotConfiguration` or
    ///        `MindGrainConfiguration`, sets its blend mode and emits
    ///        toolConfigurationChanged() - `v0.Y.37.1` (Deferred Blend
    ///        Modes). A no-op for every other type (the combo is hidden
    ///        for them anyway - see `updateSharedControlVisibility()`'s
    ///        own docs).
    /// @param index The combo's own newly-selected row.
    void handleBlendModeComboChanged(int index);

    /// @brief `mindShotCombo_`'s own `currentIndexChanged` handler: if
    ///        `config_` is currently a `MindShotConfiguration`, sets its
    ///        clip from the newly-selected library entry and emits
    ///        toolConfigurationChanged() - a no-op (no config_ update) if
    ///        the selection is the placeholder "no entries" item, or
    ///        `project_` is `nullptr`.
    /// @param index The combo's own newly-selected row.
    void handleMindShotComboChanged(int index);

    /// @brief `resonanceProfileCombo_`'s own `currentIndexChanged` handler:
    ///        if `config_` is currently a `ResonanceConfiguration`,
    ///        sets its spectrum from the newly-selected library entry and
    ///        emits toolConfigurationChanged() - a no-op (no config_
    ///        update) if the selection is the placeholder "no entries"
    ///        item, or `project_` is `nullptr`.
    /// @param index The combo's own newly-selected row.
    void handleResonanceProfileComboChanged(int index);

    /// @brief `mindGrainCombo_`'s own `currentIndexChanged` handler: if
    ///        `config_` is currently a `MindGrainConfiguration`, sets its
    ///        reference from the newly-selected library entry and emits
    ///        toolConfigurationChanged() - a no-op (no config_ update) if
    ///        the selection is the placeholder "no entries" item, or
    ///        `project_` is `nullptr`. Also refreshes the red-highlight
    ///        validity display (see setActiveLayer()'s own docs) - a newly
    ///        picked Mind Grain can have a different source layer than the
    ///        one just displayed.
    /// @param index The combo's own newly-selected row.
    void handleMindGrainComboChanged(int index);

    /// @brief `toolPresetCombo_`'s own `currentIndexChanged` handler: loads
    ///        the newly-selected preset's own configuration wholesale via
    ///        `setToolConfiguration()` (so every shared/per-type control
    ///        refreshes together, the same as loading a Picked object's own
    ///        configuration) - a no-op if the selection is the placeholder
    ///        "no presets saved" item, or `project_` is `nullptr`.
    /// @param index The combo's own newly-selected row.
    void handleToolPresetComboChanged(int index);

    /// @brief `savePresetButton_`'s own `clicked` handler: prompts for a
    ///        name via `QInputDialog::getText()` (pre-filled with `config_`'s
    ///        own current `name()`, if any), then calls
    ///        `saveCurrentAsToolPresetNamed()` - see that method's own docs
    ///        for why the two are split (this one is never called from a
    ///        test, which calls the dialog-free half directly instead). A
    ///        no-op if the dialog is cancelled or the entered name is
    ///        empty.
    void saveCurrentAsToolPreset();

    /// @brief `deletePresetButton_`'s own `clicked` handler: removes the
    ///        currently-selected Tool Preset from `project_`'s own library
    ///        - a no-op if nothing is selected (the placeholder item) or
    ///        `project_` is `nullptr`. No confirmation dialog - matching
    ///        `NamedMindShot`'s own docs that removing a library entry
    ///        never affects anything that already loaded it, so this is a
    ///        low-stakes action, the same way removing a saved MindWave or
    ///        convolution kernel already is elsewhere in this codebase.
    void deleteCurrentToolPreset();

    /// @brief Recomputes `mindGrainGroup_`'s own red-highlight/tooltip -
    ///        see setActiveLayer()'s own docs. Called after anything that
    ///        could change either input to that check: `activeLayer_`
    ///        itself changing, `config_` changing (type switch, a different
    ///        Mind Grain picked, a fresh setToolConfiguration() load), or
    ///        the Mind Grain library being refreshed.
    void updateMindGrainValidity();

    /// @brief Rebuilds `harmonicStrengthSpinBoxes_` to match `count` rows -
    ///        called whenever `harmonicCountSpinBox_` changes, or a loaded
    ///        `InstrumentConfiguration` has a different harmonic count
    ///        than the panel currently shows. Preserves each already-
    ///        displayed row's own current value where a row at that index
    ///        already existed; a newly-added row starts at `1.0`. Also
    ///        resyncs `harmonicSeriesWidget_`'s own bar count to match,
    ///        via the same non-emitting `setHarmonicStrengths()` every
    ///        other sync in this class uses.
    /// @param count The new number of harmonic strength rows to show.
    void rebuildHarmonicStrengthRows(std::size_t count);

    /// @brief Reads every one of `harmonicStrengthSpinBoxes_`'s own
    ///        current values, in order.
    /// @return The harmonic strengths currently displayed.
    [[nodiscard]] std::vector<double> currentHarmonicStrengths() const;

    /// @brief Rebuilds `vibratoMindWaveCombo_`/`tremoloMindWaveCombo_`'s own
    ///        items from `availableMindWaves_` - see
    ///        `setAvailableMindWaves()`'s own docs. Called from there, and
    ///        from `setToolConfiguration()` so a freshly-loaded
    ///        `InstrumentConfiguration`'s own bindings are reflected
    ///        immediately even if the library itself hasn't changed. Also
    ///        rebuilds `opacityMindWaveCombo_`/`sizeMindWaveCombo_`/
    ///        `colorMindWaveCombo_` (`v0.Y.54.1` Installment B) - unlike the
    ///        vibrato/tremolo pair, these read straight from `config_`
    ///        itself (a base-class field every type shares), not gated by
    ///        a `dynamic_cast`.
    void rebuildMindWaveCombos();

    /// @brief Repopulates one MindWave-binding combo from `availableMindWaves_`,
    ///        selecting `boundId`'s own entry if it names one currently in
    ///        the list - the shared body every `rebuildMindWaveCombos()`
    ///        call and `changeToolType()`'s own narrower Opacity/Size/Color-
    ///        only refresh (see its own docs) both build on.
    /// @param combo The combo to repopulate.
    /// @param boundId The id to preselect, or `std::nullopt` for "None".
    void populateMindWaveCombo(QComboBox* combo, std::optional<sound_mind::core::MindWaveId> boundId);

    std::unique_ptr<sound_mind::core::ToolConfiguration> config_;

    /// @brief `v0.Y.55.1`'s own second prerequisite - see
    ///        `refreshToolPresets()`'s own docs. Reads from `project_`,
    ///        the same field `mindShotCombo_`/`mindGrainCombo_` already
    ///        draw their own entries from.
    QComboBox* toolPresetCombo_ = nullptr;
    QPushButton* savePresetButton_ = nullptr;
    QPushButton* deletePresetButton_ = nullptr;

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
    /// @brief The draggable bar-chart visual complement to
    ///        `harmonicStrengthSpinBoxes_` above - see
    ///        `HarmonicSeriesWidget`'s own docs. Kept in sync with the
    ///        spin boxes bidirectionally, the same
    ///        `EqualizerCurveWidget`/`GradientEditorWidget` pattern
    ///        `FilterConfigurationPanel`'s own Equalizer section already
    ///        established.
    HarmonicSeriesWidget* harmonicSeriesWidget_ = nullptr;
    QDoubleSpinBox* inharmonicitySpinBox_ = nullptr;

    /// @brief Vibrato/tremolo binding controls - `v0.Y.39.1` Installment A.
    ///        See `setAvailableMindWaves()`'s own docs.
    QComboBox* vibratoMindWaveCombo_ = nullptr;
    QDoubleSpinBox* vibratoDepthSpinBox_ = nullptr;
    QComboBox* tremoloMindWaveCombo_ = nullptr;
    QDoubleSpinBox* tremoloDepthSpinBox_ = nullptr;

    /// @brief The project's current MindWave library, as id/name pairs -
    ///        see `setAvailableMindWaves()`'s own docs.
    std::vector<std::pair<sound_mind::core::MindWaveId, QString>> availableMindWaves_;

    /// @brief `ResonanceConfiguration`'s own controls - `v0.Y.59.1`
    ///        Installment D - shown only while `config_->type() ==
    ///        ToolType::Resonance`, see
    ///        updateVisibleToolTypeGroup()'s own docs.
    QWidget* resonanceGroup_ = nullptr;
    QComboBox* resonanceProfileCombo_ = nullptr;
    QDoubleSpinBox* decayRateSpinBox_ = nullptr;
    QDoubleSpinBox* frequencyScaleSpinBox_ = nullptr;
    QDoubleSpinBox* timeSpanSpinBox_ = nullptr;

    /// @brief `MindShotConfiguration`'s own controls, shown only while
    ///        `config_->type() == ToolType::MindShot` - see
    ///        updateVisibleToolTypeGroup()'s own docs.
    QWidget* mindShotGroup_ = nullptr;
    QComboBox* mindShotCombo_ = nullptr;

    /// @brief `MindGrainConfiguration`'s own controls, shown only while
    ///        `config_->type() == ToolType::MindGrain` - see
    ///        updateVisibleToolTypeGroup()'s own docs. `mindGrainGroup_`
    ///        itself carries the red-highlight/tooltip warning - see
    ///        setActiveLayer()'s own docs.
    QWidget* mindGrainGroup_ = nullptr;
    QComboBox* mindGrainCombo_ = nullptr;

    /// @brief `OrderChaosConfiguration`'s own controls, shown only while
    ///        `config_->type() == ToolType::OrderChaos` - see
    ///        updateVisibleToolTypeGroup()'s own docs. The first per-type
    ///        group added since Mind Grain's own - `Heal`/`Soften`/`Smudge`
    ///        (`v0.Y.34.1` Installment A/B) all needed none at all.
    QWidget* orderChaosGroup_ = nullptr;
    QDoubleSpinBox* amountSpinBox_ = nullptr;

    /// @brief Which project `mindShotCombo_`'s/`mindGrainCombo_`'s own
    ///        entries are drawn from - see setProject()'s own docs. Not
    ///        owned; may be `nullptr`.
    sound_mind::core::Project* project_ = nullptr;

    /// @brief See setActiveLayer()'s own docs. `0` (never a real `LayerId`
    ///        - see `Layer`'s own docs on why ids start at `1`) until the
    ///        first real setActiveLayer() call.
    sound_mind::core::LayerId activeLayer_ = 0;

    /// @brief The layout holding every *shared* control below - stored (not
    ///        a local constructor variable) so updateSharedControlVisibility()
    ///        can hide/show individual rows via `QFormLayout::setRowVisible()`
    ///        per the current tool type - see that method's own docs.
    QFormLayout* sharedControlsForm_ = nullptr;

    QDoubleSpinBox* falloffSpinBox_ = nullptr;
    QDoubleSpinBox* sizeSpinBox_ = nullptr;
    QComboBox* stampModeCombo_ = nullptr;
    QDoubleSpinBox* stampIntervalSpinBox_ = nullptr;
    QLineEdit* stampIntervalPatternLineEdit_ = nullptr;
    QLabel* stampIntervalPatternErrorLabel_ = nullptr;

    /// @brief Canvas-space Opacity/Size/Color MindWave binding combos -
    ///        `v0.Y.54.1` Paint Tool Enhancements Installment B. Live in
    ///        `sharedControlsForm_` alongside Falloff/Size (not per-type
    ///        groups) since `ToolConfiguration::opacityMindWave()`/
    ///        `sizeMindWave()`/`colorMindWave()` are base-class fields every
    ///        gradient-blended type shares - see
    ///        `updateSharedControlVisibility()`'s own docs for exactly which
    ///        types show them (the same set that shows Falloff/Size).
    ///        Unlike `vibratoMindWaveCombo_`/`tremoloMindWaveCombo_`, no
    ///        paired depth spin box - each binding's own field value
    ///        directly multiplies/scales/replaces the parameter it targets
    ///        (see each one's own docs on `ToolConfiguration`), with no
    ///        separate depth dial. Repopulated by `rebuildMindWaveCombos()`,
    ///        the same as the vibrato/tremolo pair.
    QComboBox* opacityMindWaveCombo_ = nullptr;
    QComboBox* sizeMindWaveCombo_ = nullptr;
    QComboBox* colorMindWaveCombo_ = nullptr;

    /// @brief Which coordinate frame the three combos above sample in -
    ///        `sound_mind::core::MindWaveBindingFrame`, `v0.Y.54.1`
    ///        Installment C. A single shared choice, not one per binding -
    ///        see `MindWaveBindingFrame`'s own docs on `ToolConfiguration`
    ///        for why. Lives in `sharedControlsForm_` right alongside the
    ///        three combos, same visibility gate.
    QComboBox* mindWaveBindingFrameCombo_ = nullptr;

    /// @brief The stroke's own gradient editor - see the class's own docs
    ///        and updateSharedControlVisibility()'s own docs for which
    ///        tool types show it, and in which display mode.
    GradientEditorWidget* gradientEditor_ = nullptr;

    /// @brief Mind Shot's/Mind Grain's own blend mode - `v0.Y.37.1`
    ///        (Deferred Blend Modes). Lives in `sharedControlsForm_`
    ///        alongside Falloff/Size/`gradientEditor_` (shown only for
    ///        those two types, the exact inverse of those three) rather
    ///        than duplicated once per type-specific group, even though
    ///        it's stored as two separately-declared fields
    ///        (`MindShotConfiguration::blendMode()`/
    ///        `MindGrainConfiguration::blendMode()`, not a shared
    ///        `ToolConfiguration` base member) - see
    ///        `updateSharedControlVisibility()`'s own docs.
    QComboBox* blendModeCombo_ = nullptr;
    QCheckBox* showBoundingBoxesCheckBox_ = nullptr;
    QCheckBox* showPathGeometryCheckBox_ = nullptr;
};

}  // namespace sound_mind::studio
