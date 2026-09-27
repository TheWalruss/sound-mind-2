#pragma once

#include <optional>

#include <QDockWidget>

#include "sound_mind/core/midi_program_mapping.h"
#include "sound_mind/core/project.h"

class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QTableWidget;

namespace sound_mind::studio {

/**
 * @brief A dockable panel mapping General MIDI programs to saved Tool
 *        Presets, with per-program duration/pitch modifiers -
 *        `v0.Y.55.1`'s own MIDI Configuration panel installment (the
 *        milestone's own last remaining piece), see
 *        `docs/sound-mind-design.md`'s "Import" ("A new 'MIDI
 *        configuration' panel allows the user to map MIDI programs to
 *        saved/named Paint Tool configurations... duration, pitch,
 *        strength, etc can modify the paint op in different ways").
 *
 * **Keyed by program number, not by a specific MIDI file's own channels** -
 * see `sound_mind::core::MidiProgramMapping`'s own docs for why: a mapping
 * saved here is project-wide, persisted state (`Project::
 * midiProgramMappings()`), automatically picked up by *any* future MIDI
 * import whose own tracks use the same GM program, not scoped to one
 * file's own import session the way `MidiImportDialog`'s own channel
 * checklist is.
 *
 * **Needs a live `Project*`** (`setProject()`) - like `ToolConfigurationPanel`'s
 * own Tool Preset combo, this panel's whole reason for existing is live
 * project state (the current mapping set, and the Tool Preset library each
 * row's own combo lists), so it mutates `project_` directly rather than
 * only emitting signals for some separate controller to apply - the same
 * "this one legitimately needs it" exception `ToolConfigurationPanel`'s
 * own class docs already establish, not the more decoupled
 * emit-only shape most other panels use.
 *
 * **No "strength"/velocity modifier** - deliberately, matching
 * `MidiProgramMapping`'s own docs: velocity has nothing in this codebase's
 * own paint model to attach to yet.
 *
 * Every edit (adding/deleting a mapping, changing its Tool Preset,
 * duration scale, or pitch offset) writes straight through to
 * `project_->setMidiProgramMapping()`/`removeMidiProgramMapping()`
 * immediately - no separate "Save" step, the same "auto-applies, like a
 * layer's own opacity slider" immediacy every scalar-valued control in
 * this codebase already uses when there's no name to type first (unlike
 * `ToolConfigurationPanel`'s own Tool Preset "Save..." button, which needs
 * one).
 */
class MidiConfigurationPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief Builds the panel with an empty table and no project set.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit MidiConfigurationPanel(QWidget* parent = nullptr);

    /**
     * @brief Sets which project this panel reads/writes its own MIDI
     *        Configuration from.
     *
     * Repopulates the table immediately from `project`'s own current
     * `midiProgramMappings()` (see refreshMappings()'s own docs).
     *
     * @param project The project to read/write; may be `nullptr` (the
     *        table shows nothing, and every control is disabled, until a
     *        real one is set again).
     */
    void setProject(sound_mind::core::Project* project);

    /**
     * @brief Repopulates the table from `project_`'s own current
     *        `midiProgramMappings()` - call whenever that collection might
     *        have changed out from under this panel (`setProject()`
     *        already calls this itself for a project switch).
     */
    void refreshMappings();

    /**
     * @brief Repopulates every row's own Tool Preset combo from
     *        `project_`'s own current `toolPresets()` - call whenever that
     *        library might have changed out from under this panel (a
     *        preset saved/deleted via `ToolConfigurationPanel`, in
     *        particular; `setProject()`/`refreshMappings()` already call
     *        this themselves).
     *
     * Preserves each row's own currently-selected preset, by id, if it
     * still exists; falls back to "None" (no mapping) otherwise -
     * matching `ToolConfigurationPanel::populateMindWaveCombo()`'s own
     * "preserve by id, fall back to none" precedent.
     */
    void refreshToolPresets();

private:
    /// @brief `addMappingButton_`'s own `clicked` slot: adds a fresh,
    ///        default `MidiProgramMapping` for whichever program
    ///        `addProgramCombo_` currently shows, writes it straight into
    ///        `project_`, adds its own new table row, and removes that
    ///        program from `addProgramCombo_` (a program can only be
    ///        mapped once - see this class's own docs).
    void addMapping();

    /// @brief Rebuilds `addProgramCombo_` to list every General MIDI
    ///        program (0-127) that doesn't already have a mapping in
    ///        `project_`.
    void rebuildAddProgramCombo();

    /// @brief One row's own Tool Preset combo `currentIndexChanged` slot -
    ///        updates `project_`'s own mapping for `programNumber`.
    /// @param programNumber Which program's own mapping to update - bound
    ///        at connect time, not read from the table's own current row
    ///        (stable across later row deletions elsewhere in the table).
    /// @param toolPresetId The newly-selected preset's own id, or
    ///        `std::nullopt` for "None".
    void handleToolPresetChanged(int programNumber, std::optional<sound_mind::core::ToolPresetId> toolPresetId);

    /// @brief One row's own Duration Scale spin box `valueChanged` slot.
    /// @param programNumber Which program's own mapping to update.
    /// @param value The spin box's own new value.
    void handleDurationScaleChanged(int programNumber, double value);

    /// @brief One row's own Pitch Offset spin box `valueChanged` slot.
    /// @param programNumber Which program's own mapping to update.
    /// @param value The spin box's own new value.
    void handlePitchOffsetChanged(int programNumber, double value);

    /// @brief One row's own Delete button `clicked` slot - removes
    ///        `programNumber`'s own mapping from `project_`, removes its
    ///        table row, and adds it back to `addProgramCombo_`.
    /// @param programNumber Which program's own mapping to delete.
    void deleteMapping(int programNumber);

    /// @brief Appends one new table row for `mapping`, wiring every
    ///        cell's own widget to the handlers above - the shared
    ///        row-building logic refreshMappings()/addMapping() both need.
    /// @param mapping The mapping to display.
    void appendRow(const sound_mind::core::MidiProgramMapping& mapping);

    /// @brief Finds the table row whose own stored program number matches
    ///        `programNumber`, if any - the "row index shifts after a
    ///        deletion, program number doesn't" lookup every per-row
    ///        handler above needs before touching `table_` directly.
    /// @param programNumber The program to find.
    /// @return The matching row index, or `-1` if none.
    [[nodiscard]] int rowForProgram(int programNumber) const;

    QComboBox* addProgramCombo_ = nullptr;
    QPushButton* addMappingButton_ = nullptr;
    QTableWidget* table_ = nullptr;
    sound_mind::core::Project* project_ = nullptr;
};

}  // namespace sound_mind::studio
