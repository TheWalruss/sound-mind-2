#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>

#include <nlohmann/json.hpp>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/playback_engine.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/resource_file.h"

namespace sound_mind::studio {

class ResourceBrowserPanel;

/**
 * @brief Owns the Resource Browser panel's actual `Project` reads/
 *        mutations - browsing a second project read-only, exporting an
 *        entry to a standalone portable resource file, importing one from
 *        a file or from the browsed project, and previewing an entry's own
 *        rendered raster and/or decoded audio. `docs/sound-mind-roadmap.md`'s
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
 * **Audio preview playback** owns a dedicated `PlaybackEngine` instance,
 * independent of the Studio's own main playback path - the same "its own
 * isolated engine, not the shared transport" precedent
 * `DeviceTestTonePlayer` already establishes for test-tone playback, so
 * previewing an entry here never interferes with (or is interfered with
 * by) actual project playback/Loop Mode. Mind Shot previews its own real
 * captured audio; Mind Wave, Tool Preset, and Resonance Profile each
 * instead preview a *synthesized* representative stroke (direct user
 * feedback: "practically wherever there is a visual preview of
 * something, give the user the ability to play an audio preview of
 * whatever it is") - see `renderToolConfigurationStreamImage()`'s own
 * docs in the `.cpp` for how.
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

    /**
     * @brief The non-prompting core behind "Add to Toolkit" - see
     *        `exportSelectedEntry()`'s own docs for why this split exists
     *        (here, so the auto-added-dependency notification is
     *        directly testable without a real, blocking `QMessageBox`).
     *
     * Adds the currently selected entry to the Toolkit draft, and
     * recursively does the same for every other resource it references
     * (`docs/sound-mind-roadmap.md`'s "check whether the resource requires
     * something else... add the dependencies, and notify the user which
     * additional resources were added" - confirmed with the user). A
     * dependency already present in the draft is a no-op for that one
     * entry, not re-added or re-reported.
     * @return The display name of every dependency actually added
     *         *besides* the primary entry itself, in order; empty if none
     *         were needed, nothing was selected, or the selected category
     *         has no portable form to add at all.
     */
    QStringList addSelectedEntryToToolkit();

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
    ///        a copy, not `project_` directly). Not `static` (despite
    ///        operating on `target`, not `project_`) - it still needs
    ///        `panel_` as the parent widget for the collision-resolution
    ///        dialog (`sound_mind::studio::resolveImportName()`) a
    ///        colliding entry's own name now shows.
    /// @param type Which portable resource type `resource` holds.
    /// @param resource That resource's own serialized JSON (`id` already
    ///        `0`, per `sound_mind::core::ImportedToolkit`'s own docs).
    /// @param target The project to add it to.
    /// @throws nlohmann::json::exception if `resource` doesn't actually
    ///         match `type`'s own expected shape.
    void applyToolkitEntry(sound_mind::core::PortableResourceType type, const nlohmann::json& resource,
                                   sound_mind::core::Project& target);

    /// @brief One resource `findDependencies()` found referenced, by type
    ///        and project-local id - `docs/sound-mind-roadmap.md`'s
    ///        "check whether the resource requires something else...
    ///        add the dependencies" (confirmed with the user).
    struct ResourceDependency {
        sound_mind::core::PortableResourceType type;
        std::uint64_t id;
    };

    /// @brief Looks up `id` within `type`'s own library in `project` and
    ///        builds its serialized JSON and display label - the shared
    ///        lookup `handleAddToToolkitRequested()`'s own former per-
    ///        category switch duplicated; factored out so
    ///        `addToolkitEntryWithDependencies()` can resolve a
    ///        dependency's own id the identical way.
    /// @param type Which library to look in.
    /// @param id The entry to find.
    /// @param project The project to resolve against.
    /// @param outResource Set to the entry's own serialized JSON on
    ///        success.
    /// @param outDisplayName Set to the entry's own display label (e.g.
    ///        "My Brush (Procedural)") on success.
    /// @return `true` if `id` resolved to a real entry; `false` otherwise
    ///         (neither output is touched).
    static bool resolveResourceForToolkit(sound_mind::core::PortableResourceType type, std::uint64_t id,
                                           const sound_mind::core::Project& project, nlohmann::json& outResource,
                                           QString& outDisplayName);

    /// @brief Scans a resource's own serialized JSON for every reference
    ///        to another library resource - any field ending in
    ///        `"MindWaveId"` (every `opacityMindWave()`/`blurSigmaMindWave()`/
    ///        etc.-style binding across `ToolConfiguration`/
    ///        `FilterConfiguration` already shares this one naming
    ///        convention), plus `"sourceMindShotId"`/
    ///        `"sourceResonanceProfileId"` (a `MindShotConfiguration`'s/
    ///        `ResonanceConfiguration`'s own UI-metadata source id).
    ///        `"sourceMindGrainId"` is deliberately not matched - a Mind
    ///        Grain has no portable form to bundle in the first place.
    /// @param resourceJson The already-serialized resource to scan.
    /// @return Every dependency found, in encounter order; duplicates are
    ///         possible (the caller dedups against the draft already).
    [[nodiscard]] static std::vector<ResourceDependency> findDependencies(const nlohmann::json& resourceJson);

    /// @brief Adds `type`/`id` to `toolkitDraft_` (if not already present,
    ///        by `type`+`sourceId`) and recursively does the same for
    ///        every dependency `findDependencies()` finds in its own
    ///        serialized JSON, appending each dependency's own display
    ///        name to `autoAddedNames` (never the originally-requested
    ///        entry's own name, even on a repeat call that's already a
    ///        no-op).
    /// @param type Which library `id` is in.
    /// @param id The entry to add.
    /// @param project The project to resolve `id` (and any dependency ids
    ///        it references) against.
    /// @param autoAddedNames Appended with each dependency's own display
    ///        name, for `handleAddToToolkitRequested()`'s own "Also
    ///        added..." notification.
    /// @param isDependency `true` for a recursive dependency call (so its
    ///        own name is reported); `false` for the originally-requested
    ///        entry.
    /// @return `true` if `id` resolved and was (or already is) in the
    ///         draft; `false` if it didn't resolve at all.
    bool addToolkitEntryWithDependencies(sound_mind::core::PortableResourceType type, std::uint64_t id,
                                         const sound_mind::core::Project& project, QStringList& autoAddedNames,
                                         bool isDependency = false);

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

    /// @brief The currently selected entry's own encoded `StreamImage`
    ///        (built once, in `refreshInspector()`, to render the raster
    ///        and/or ready an audio preview) - kept here so
    ///        `handlePlayRequested()` can decode it lazily, on the first
    ///        actual Play click, rather than `refreshInspector()`
    ///        decoding eagerly on every selection whether or not it's
    ///        ever played. `decode()` (an inverse STFT over however many
    ///        frames/bins the image holds) was the real cost behind "the
    ///        Mind Shot preview is very slow" - confirmed by profiling
    ///        which of the two steps `refreshInspector()` already did
    ///        (render the raster, decode the audio) actually dominated;
    ///        every other previewable category reuses the same lazy-
    ///        decode shape for the same reason. A Mind Shot's own real
    ///        captured image for that category; a freshly synthesized
    ///        one (`renderToolConfigurationStreamImage()`) for Tool
    ///        Preset/MindWave/Resonance Profile. `std::nullopt` for any
    ///        other category, or no selection.
    std::optional<sound_mind::codec::StreamImage> currentPreviewImage_;

    /// @brief The currently previewed entry's own decoded audio - empty
    ///        until `handlePlayRequested()` first decodes
    ///        `currentPreviewImage_` (see its own docs), then kept here
    ///        so a second Play on the same selection doesn't re-decode.
    ///        Reset to empty whenever `currentPreviewImage_` changes.
    sound_mind::codec::AudioBuffer currentPreviewAudio_;

    /// @brief Dedicated to this panel's own audio preview playback - see
    ///        the class's own docs on why this is a separate engine
    ///        instance.
    sound_mind::core::PlaybackEngine previewEngine_{sound_mind::core::AudioDeviceMode::Real};

    /// @brief One entry added to the in-progress Toolkit draft via "Add to
    ///        Toolkit" - `docs/sound-mind-roadmap.md`'s `v0.Y.57.1`
    ///        Installment D.
    struct ToolkitDraftEntry {
        /// @brief Which portable resource type this entry is.
        sound_mind::core::PortableResourceType type;

        /// @brief The entry's own id within whichever project it was
        ///        added from - kept only for this session's own in-memory
        ///        dedup check (`addToolkitEntryWithDependencies()`'s own
        ///        "already in the draft?" lookup), never written to the
        ///        exported `.smtoolkit` file (`exportToolkitAt()` only
        ///        ever reads `type`/`resource` back out) - the same
        ///        "never meaningful outside the project that assigned it"
        ///        reasoning every exported resource's own `id` field
        ///        already follows.
        std::uint64_t sourceId = 0;

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
