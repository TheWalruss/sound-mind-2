#pragma once

#include <functional>
#include <string>

#include <QString>

class QWidget;

namespace sound_mind::studio {

/// @brief Which choice the user ended up making in promptSaveName() - see
/// that function's own docs.
enum class SaveNameOutcome {
    /// @brief `name`/`replacingExisting` in the same `SaveNameResult` are
    ///        meaningful; proceed with the save.
    Saved,
    /// @brief The user cancelled - do not save anything.
    Cancelled,
};

/// @brief promptSaveName()'s own result - see that function's own docs.
struct SaveNameResult {
    /// @brief Whether to actually save, and whether `name` is a fresh
    ///        entry or an existing one to overwrite - see `Saved`'s own
    ///        docs.
    SaveNameOutcome outcome = SaveNameOutcome::Cancelled;

    /// @brief The final, collision-free (or deliberately-overwriting)
    ///        name to save under - only meaningful while `outcome` is
    ///        `Saved`.
    std::string name;

    /// @brief `true` if the user chose Replace on a collision - the
    ///        caller should overwrite the *existing* entry already using
    ///        `name` (same id, new payload), not append a second entry
    ///        under the same name. `false` for every other case
    ///        (including a `name` that was never actually taken).
    bool replacingExisting = false;
};

/**
 * @brief Drives the universal "save under a name, resolving a collision"
 *        flow every named-resource library now shares - direct user
 *        feedback: "when a user specifies an existing name, the user gets
 *        the option to replace the existing resource, to give a new name
 *        (to resolve the conflict), or to cancel saving. Check for new
 *        name collisions when the user specifies a new name, of course."
 *
 * Prompts for a name via `QInputDialog::getText()`, seeded with
 * `initialName` (matching every existing per-resource "Save..." handler's
 * own prior behavior exactly - this function replaces that prompt, not
 * just what happens after it). If the typed name doesn't collide
 * (`nameExists()` returns `false`), returns immediately with
 * `{Saved, name, false}` - the common case is unchanged from before this
 * flow existed. On a collision, asks Replace / Rename / Cancel: Replace
 * returns `{Saved, name, true}` (caller overwrites the existing entry's
 * own payload in place); Rename re-prompts for a new name and checks it
 * again, looping for as many collisions as the user keeps hitting;
 * Cancel (or dismissing either dialog) returns `{Cancelled, {}, false}`.
 *
 * @param parent The dialog's own parent widget, per Qt's normal
 *        parent-ownership convention; may be `nullptr`.
 * @param dialogTitle The name-entry dialog's own window title (e.g. "Save
 *        Tool Preset") - matches each resource's own prior
 *        `QInputDialog::getText()` call exactly, so this is a drop-in
 *        replacement.
 * @param promptLabel The name-entry dialog's own prompt text (e.g.
 *        "Name:").
 * @param initialName The name field's own starting text.
 * @param nameExists Checks whether a given name already exists in the
 *        caller's own resource library - typically a one-line lambda
 *        wrapping e.g. `project_->toolPresetNameExists()`.
 * @return The user's final choice - see `SaveNameResult`'s own docs.
 */
[[nodiscard]] SaveNameResult promptSaveName(QWidget* parent, const QString& dialogTitle, const QString& promptLabel,
                                             const QString& initialName,
                                             const std::function<bool(const std::string&)>& nameExists);

/**
 * @brief The collision-resolution half of promptSaveName() alone, for a
 *        caller that already obtained a candidate name from its own
 *        dialog instead of a plain `QInputDialog` - `MindCaptureDialog`
 *        in particular, which prompts for a name alongside its own
 *        fundamental-frequency/start-offset fields together, so
 *        `promptSaveName()`'s own separate name prompt would be a
 *        redundant second dialog. Skips straight to the Replace / Rename
 *        / Cancel flow `promptSaveName()`'s own docs describe, starting
 *        from `candidateName` instead of asking for it first - `
 *        promptSaveName()` itself is just this, called after its own
 *        initial `askForName()`.
 * @param parent The dialog's own parent widget; may be `nullptr`.
 * @param dialogTitle The Rename re-prompt's own window title.
 * @param promptLabel The Rename re-prompt's own prompt text.
 * @param candidateName The name to check first - already chosen by the
 *        caller's own dialog, not asked for here.
 * @param nameExists Checks whether a given name already exists in the
 *        caller's own resource library.
 * @return The user's final choice - see `SaveNameResult`'s own docs.
 */
[[nodiscard]] SaveNameResult resolveNameCollision(QWidget* parent, const QString& dialogTitle,
                                                   const QString& promptLabel, const QString& candidateName,
                                                   const std::function<bool(const std::string&)>& nameExists);

/// @brief Which choice the user ended up making in resolveImportName() -
/// see that function's own docs.
enum class ImportNameAction {
    /// @brief Add the import as a new entry under `ImportNameResolution::name`
    ///        (either the original, collision-free name, or an
    ///        auto-renamed one after Keep Both).
    AddNew,
    /// @brief Overwrite the existing entry already using
    ///        `ImportNameResolution::name` with the imported payload.
    OverwriteExisting,
    /// @brief Skip this import entirely - keep the existing entry
    ///        untouched, discard the imported payload.
    Skip,
};

/// @brief resolveImportName()'s own result - see that function's own docs.
struct ImportNameResolution {
    /// @brief What the caller should actually do - see `ImportNameAction`'s
    ///        own docs.
    ImportNameAction action = ImportNameAction::AddNew;

    /// @brief The name to use - `desiredName` unchanged for `AddNew`
    ///        (no collision) or `OverwriteExisting`; a freshly generated,
    ///        collision-free name for `AddNew` after Keep Both; the
    ///        original colliding name (unused) for `Skip`.
    std::string name;
};

/**
 * @brief Drives the universal import-name-collision flow every named-
 *        resource library's import path now shares - direct user
 *        feedback: "When importing resources, if there's a name
 *        collision, give the user the option to use the import version
 *        (overwrite), keep the existing version (ignore), or keep both
 *        (rename import version)."
 *
 * If `desiredName` doesn't collide (`nameExists()` returns `false`),
 * returns immediately with `{AddNew, desiredName}` - no dialog, matching
 * every import path's own prior behavior for the overwhelmingly common
 * case of importing something with no naming conflict. On a collision,
 * asks Overwrite / Keep Existing / Keep Both: Overwrite returns
 * `{OverwriteExisting, desiredName}`; Keep Existing returns
 * `{Skip, desiredName}`; Keep Both appends " (2)", " (3)", ... to
 * `desiredName` until `nameExists()` finds a free one, and returns
 * `{AddNew, <that free name>}` - the same auto-suffix shape
 * `Project::uniqueLayerName()` already establishes for Layers, applied
 * here as a user-chosen option instead of an unconditional default.
 *
 * @param parent The dialog's own parent widget; may be `nullptr`.
 * @param resourceTypeLabel A human-readable label for the resource type
 *        being imported (e.g. "Tool Preset"), used in the dialog's own
 *        message text.
 * @param desiredName The imported entry's own name, before any collision
 *        resolution.
 * @param nameExists Checks whether a given name already exists in the
 *        caller's own resource library.
 * @return The user's final choice - see `ImportNameResolution`'s own
 *         docs.
 */
[[nodiscard]] ImportNameResolution resolveImportName(QWidget* parent, const QString& resourceTypeLabel,
                                                      const std::string& desiredName,
                                                      const std::function<bool(const std::string&)>& nameExists);

}  // namespace sound_mind::studio
