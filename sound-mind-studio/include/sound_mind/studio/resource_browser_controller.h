#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include <QObject>
#include <QString>

#include <nlohmann/json.hpp>

#include "sound_mind/core/playback_engine.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/resource_file.h"

namespace sound_mind::studio {

class ResourceBrowserPanel;

/**
 * @brief Owns the Resource Browser panel's actual `Project` reads/
 *        mutations - browsing a second project read-only, exporting an
 *        entry to a standalone portable resource file, importing one from
 *        a file or from the browsed project, and previewing a Mind Shot's
 *        own rendered raster/decoded audio. `docs/sound-mind-roadmap.md`'s
 *        `v0.Y.57.1` Installment C, the same "own presentation" split
 *        `MindWaveController` already establishes for `MindWavesPanel`.
 *
 * **Which project is "active" for browsing** is `browsedProject()` while
 * set, `project_` otherwise - every category/entry/inspector read goes
 * through this; every *mutation* (importing something) always targets
 * `project_` specifically, never the browsed one, which this controller
 * only ever reads (`sound_mind::core::Project::load()`'s own result, kept
 * entirely separate from, and never written back over, whatever file it
 * came from).
 *
 * **Mind Shot preview playback** owns a dedicated `PlaybackEngine`
 * instance, independent of the Studio's own main playback path - the same
 * "its own isolated engine, not the shared transport" precedent
 * `DeviceTestTonePlayer` already establishes for test-tone playback, so
 * previewing a Mind Shot here never interferes with (or is interfered
 * with by) actual project playback/Loop Mode.
 */
class ResourceBrowserController : public QObject {
    Q_OBJECT

public:
    /**
     * @param panel Non-owning; refreshed after any mutation, and the
     *        source of every user action this controller handles. Must
     *        outlive this controller.
     * @param parent The owning object, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    explicit ResourceBrowserController(ResourceBrowserPanel* panel, QObject* parent = nullptr);

    /// @brief Sets which project this controller browses/mutates by
    ///        default, and stops browsing any other project (its own
    ///        selection means nothing for a different project). Does
    ///        *not* itself refresh the panel - `MainWindow`'s own
    ///        `setProject()` calls refreshPanel() separately, matching
    ///        `MindWaveController::setProject()`'s own precedent.
    /// @param project The project to target; may be `nullptr`.
    void setProject(sound_mind::core::Project* project);

    /// @brief Rebuilds the panel's own category/entry list and inspector
    ///        from whichever project is currently active for browsing.
    void refreshPanel();

    /**
     * @brief The non-prompting core behind the inspector's "Export..."
     *        button - the same `exportTopmostLayerAudioNow()`/
     *        `openProjectAt()` split `MainWindow` already establishes
     *        between "show a file dialog" and "do the actual work at a
     *        given path," so this is directly testable without a real
     *        `QFileDialog`.
     * @param path Destination path.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable reason.
     * @return `true` on success; `false` if no entry is selected, the
     *         selected category has no portable file format, or the
     *         export itself throws.
     */
    bool exportSelectedEntry(const std::filesystem::path& path, QString* errorMessage = nullptr);

    /// @brief The non-prompting core behind "Import from File..." - see
    ///        exportSelectedEntry()'s own docs for why this split exists.
    /// @param path The portable resource file to import.
    /// @param errorMessage If non-null and this returns `false`, set to a
    ///        human-readable reason.
    /// @return `true` on success (and refreshes the panel/emits
    ///         resourcesChanged()); `false` if no project is set, the
    ///         selected category has no portable file format, or the
    ///         import itself throws.
    bool importFromFile(const std::filesystem::path& path, QString* errorMessage = nullptr);

    /// @brief The non-prompting core behind "Browse Other Project..." -
    ///        see exportSelectedEntry()'s own docs for why this split
    ///        exists.
    /// @param path The second project's own `.smproj` file - loaded
    ///        read-only via `sound_mind::core::Project::load()`, never
    ///        written back to.
    /// @param errorMessage If non-null and this returns `false`, set to a
    ///        human-readable reason.
    /// @return `true` on success (and refreshes the panel); `false` if
    ///         loading it throws.
    bool browseOtherProjectAt(const std::filesystem::path& path, QString* errorMessage = nullptr);

    /// @brief The non-prompting core behind "Export Toolkit..." - see
    ///        exportSelectedEntry()'s own docs for why this split exists.
    /// @param name The toolkit's own display name.
    /// @param path Destination path.
    /// @param errorMessage If non-null and this returns `false`, set to a
    ///        human-readable reason.
    /// @return `true` on success; `false` if the draft is empty or the
    ///         export itself throws.
    bool exportToolkitAt(const QString& name, const std::filesystem::path& path, QString* errorMessage = nullptr);

    /// @brief The non-prompting core behind "Import Toolkit..." - see
    ///        exportSelectedEntry()'s own docs for why this split exists.
    ///        Every bundled entry is added to `project_` - a partial
    ///        import never happens; if any entry fails to apply, this
    ///        returns `false` with nothing yet added.
    /// @param path The `.smtoolkit` file to import.
    /// @param errorMessage If non-null and this returns `false`, set to a
    ///        human-readable reason.
    /// @return `true` on success (and refreshes the panel/emits
    ///         resourcesChanged()); `false` if no project is set or the
    ///         import itself throws.
    bool importToolkitFrom(const std::filesystem::path& path, QString* errorMessage = nullptr);

signals:
    /// @brief An import actually added something to `project_` - the
    ///        Studio's own cue to mark `hasUnsavedChanges()`, the same
    ///        role `MindWaveController::mindWavesChanged()` plays.
    void resourcesChanged();

private:
    /// @brief Rebuilds the entries list for whichever category is
    ///        currently selected, from the active project.
    void refreshEntries();

    /// @brief Rebuilds the inspector for the currently selected entry, if
    ///        any - clears it otherwise.
    void refreshInspector();

    void handleCategoryChanged();
    void handleEntrySelected(std::optional<std::uint64_t> id);
    void handleExportRequested();
    void handleImportFromFileRequested();
    void handleImportEntryRequested();
    void handleBrowseOtherProjectRequested();
    void handleReturnToCurrentProjectRequested();
    void handlePlayRequested();
    void handleStopRequested();
    void handleAddToToolkitRequested();
    void handleRemoveFromToolkitRequested(int index);
    void handleExportToolkitRequested();
    void handleImportToolkitRequested();

    /// @brief Rebuilds the panel's own Toolkit draft list from
    ///        `toolkitDraft_`.
    void refreshToolkitEntries();

    /// @brief Applies one imported Toolkit entry onto `target` - shared by
    ///        `importToolkitFrom()`'s own all-or-nothing trial-copy
    ///        mechanism (see that method's own docs on why it operates on
    ///        a copy, not `project_` directly).
    /// @param type Which portable resource type `resource` holds.
    /// @param resource That resource's own serialized JSON (`id` already
    ///        `0`, per `sound_mind::core::ImportedToolkit`'s own docs).
    /// @param target The project to add it to.
    /// @throws nlohmann::json::exception if `resource` doesn't actually
    ///         match `type`'s own expected shape.
    static void applyToolkitEntry(sound_mind::core::PortableResourceType type, const nlohmann::json& resource,
                                   sound_mind::core::Project& target);

    /// @brief `browsedProject_ ? &*browsedProject_ : project_` - see the
    ///        class's own docs.
    [[nodiscard]] const sound_mind::core::Project* activeProject() const;

    ResourceBrowserPanel* panel_;
    sound_mind::core::Project* project_ = nullptr;

    /// @brief A second project, loaded read-only via
    ///        `sound_mind::core::Project::load()`, while "Browse Other
    ///        Project..." is active - `std::nullopt` while browsing
    ///        `project_` itself.
    std::optional<sound_mind::core::Project> browsedProject_;

    /// @brief `browsedProject_`'s own path, for the panel's "Browsing:"
    ///        label - `std::nullopt` alongside `browsedProject_`.
    std::optional<QString> browsedProjectPath_;

    /// @brief The currently previewed Mind Shot's own decoded audio, kept
    ///        here (rather than re-decoding on every Play) so repeated
    ///        Play/Stop on the same selection is cheap. Cleared whenever
    ///        the selection or category changes.
    sound_mind::codec::AudioBuffer currentPreviewAudio_;

    /// @brief Dedicated to Mind Shot preview playback - see the class's
    ///        own docs on why this is a separate engine instance.
    sound_mind::core::PlaybackEngine previewEngine_{sound_mind::core::AudioDeviceMode::Real};

    /// @brief One entry added to the in-progress Toolkit draft via "Add to
    ///        Toolkit" - `docs/sound-mind-roadmap.md`'s `v0.Y.57.1`
    ///        Installment D.
    struct ToolkitDraftEntry {
        /// @brief Which portable resource type this entry is.
        sound_mind::core::PortableResourceType type;

        /// @brief That resource's own serialized JSON, captured from
        ///        `activeProject()` at the moment "Add to Toolkit" was
        ///        clicked - a permanent snapshot, not a live reference
        ///        (the same "capture now, independent of later changes"
        ///        shape every other captured resource in this codebase
        ///        already follows), so switching categories/selections -
        ///        or even browsing a different project - afterward never
        ///        changes what an already-added draft entry holds.
        nlohmann::json resource;

        /// @brief The display label `ResourceBrowserPanel::
        ///        setToolkitEntries()` shows for this entry (e.g. "My
        ///        Brush (Procedural)").
        QString displayName;
    };

    /// @brief The Toolkit currently being assembled - cleared only by a
    /// successful Export (the whole point is to let someone keep adding
    /// across several category/selection changes, so nothing *else* ever
    /// clears it automatically).
    std::vector<ToolkitDraftEntry> toolkitDraft_;
};

}  // namespace sound_mind::studio
