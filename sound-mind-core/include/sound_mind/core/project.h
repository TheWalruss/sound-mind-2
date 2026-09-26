#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/convolution_kernel.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/mind_grain.h"
#include "sound_mind/core/mind_shot.h"
#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::core {

/**
 * @brief A Sound Mind Project: settings, an ordered layer stack, and the
 *        operation log, per `docs/sound-mind-architecture.md`'s Core Data
 *        Model and "Project File & Folder".
 *
 * @note Deliberately minimal for now: most resource libraries (Sound Mind
 *       Instruments) and sequences aren't represented yet, since none of
 *       those features exist. Their absence from a saved file is meant to
 *       be forward-compatible - added as fields once each feature lands,
 *       not designed in speculatively now. `MindWaves` (`v0.Y.31.1`
 *       Installment C1 - see `mindWaves()`'s own docs), Mind Shots
 *       (`v0.Y.33.1` Installment A - see `mindShots()`'s own docs), and
 *       Mind Grains (`v0.Y.33.1` Installment B - see `mindGrains()`'s own
 *       docs) are the exceptions so far.
 */
class Project {
public:
    /// @brief Default-constructs a Project with default settings and no
    ///        layers.
    ///
    /// Exists so `from_json` (a free function, not a member) can populate
    /// an instance in place via nlohmann::json's default (de)serialization
    /// convention - the same reasoning as `Layer`'s default constructor.
    /// Prefer createNew() for normal use.
    Project() = default;

    /**
     * @brief Copy-constructs a Project - **the copy starts with an empty
     *        `operationLog()`, not a duplicate of the original's own
     *        undo/redo history**.
     *
     * Hand-written (not defaulted) specifically because of this: a plain
     * member-wise copy is impossible anyway (`OperationLog` holds
     * `std::vector<std::unique_ptr<Operation>>`, which
     * `std::vector::vector(const std::vector&)` can't copy - `Operation`
     * is a polymorphic hierarchy with no `clone()`). Rather than adding
     * one (a materially larger change, and not needed by this copy
     * constructor's own actual motivating use - see below), this simply
     * defines "copying a Project" as copying everything *except* its own
     * edit history - settings, layers (including each layer's own
     * `filterConfiguration()`/`opacityMindWave()` bindings), MindWaves,
     * MindShots, MindGrains, and convolution kernels, all copied
     * verbatim, ids and all, since nothing here reassigns any id (unlike
     * `addLayer()`/`addMindWave()`/etc., which each assign a *fresh* one -
     * not usable to rebuild an equivalent copy, since every cross-
     * reference by id, e.g. a `MindGrain`'s own `sourceLayerId`, would
     * silently point at the wrong thing afterward).
     *
     * Added for `MainWindow::startPlayback()`'s own background-compositing
     * use (`docs/sound-mind-roadmap.md`'s finding #12, Installment H):
     * `sound_mind::core::compositeProject()` needs a snapshot it can read
     * from a background thread with no risk of the UI thread mutating the
     * same `Project` concurrently, and has no reason to touch
     * `operationLog()` at all (undo/redo is a UI-thread-only concern) - a
     * copy's own empty log is simply never read by anything that copy is
     * used for.
     *
     * @note Not intended for general "clone this project, undo history and
     *       all" use - there's no such use yet, and this constructor
     *       deliberately doesn't provide it. If one arises, giving
     *       `Operation` a real `clone()` (so `OperationLog` itself can
     *       become copyable) is the more honest fix, not layering more
     *       meaning onto this one.
     *
     * @param other The project to copy from; left unchanged.
     */
    Project(const Project& other);

    /// @brief Copy-assigns a Project - see the copy constructor's own docs
    ///        for why the result's `operationLog()` is empty, not a copy
    ///        of `other`'s.
    /// @param other The project to copy from; left unchanged.
    /// @return `*this`, for chaining - the usual copy-assignment contract.
    Project& operator=(const Project& other);

    /// @brief Defaulted move constructor - declaring the copy constructor
    ///        above suppresses the implicitly-declared one, so this
    ///        restores it explicitly. Unlike the copy constructor, a move
    ///        *does* carry `operationLog()` over untouched (there's only
    ///        ever one logical owner left after a move, so there's no
    ///        "which copy keeps the history" question to answer).
    Project(Project&&) = default;

    /// @brief Defaulted move assignment - see the move constructor's own
    ///        docs.
    /// @return `*this`, for chaining - the usual move-assignment contract.
    Project& operator=(Project&&) = default;

    /**
     * @brief Creates a new project with the given settings, a Background
     *        layer, and an Equalizer layer, per `docs/sound-mind-design.md`'s
     *        "Special Layers".
     *
     * The Equalizer starts as a `FrequencyAxisGradient` filter configured
     * for "Cut": both gradient stops' own intensity is pinned to the
     * silence floor (`-96` dB, matching every other dB-ranged control's
     * own established floor), opacity left at its own default `0` (no
     * cut applied yet - the same "nothing happens by accident" default
     * every other fresh Filter layer's own gradient already establishes).
     *
     * @note Only a *new* project gets one - an existing project file
     *       saved before this milestone has no Equalizer layer at all,
     *       and `load()` doesn't retroactively add one (confirmed with
     *       the user: a project has whatever layers its own file says it
     *       has, the same as any other field).
     *
     * @param settings The settings the new project should carry.
     * @return The new project, with its Background and Equalizer layers
     *         and an empty operation log.
     */
    [[nodiscard]] static Project createNew(ProjectSettings settings);

    /**
     * @brief Loads a project from its `.smproj` JSON file on disk.
     *
     * Any layer whose media/pool file (see save()'s docs for the paths)
     * exists alongside the project is loaded back into that layer's
     * `Layer::content()`/`poolContent()`; a layer with no such file yet
     * (never rendered, or never Pooled) simply has no content there, same
     * as a freshly-created one.
     *
     * @param path Path to the project file.
     * @return The loaded project.
     * @throws std::ios_base::failure if the file can't be read.
     * @throws nlohmann::json::exception on malformed or missing required data.
     */
    [[nodiscard]] static Project load(const std::filesystem::path& path);

    /**
     * @brief Saves this project to a `.smproj` JSON file on disk.
     *
     * Per `docs/sound-mind-architecture.md`'s Project File & Folder layout,
     * any layer with cached content (see `Layer::content()`) also gets that
     * content written as its own Stream file, to `<path's folder>/<path's
     * stem>/media/layer_<id>.smstream`; any layer with Pool content (see
     * `Layer::poolContent()`) likewise gets `.../pool/layer_<id>.smpool` -
     * both created if they don't exist yet.
     *
     * @param path Destination path.
     * @throws std::ios_base::failure if the file (or a layer's media/pool
     *         file) can't be written.
     */
    void save(const std::filesystem::path& path) const;

    /**
     * @brief Appends a new layer to the top of the layer stack - or, if
     *        an Equalizer layer already occupies the very top, just
     *        beneath it instead.
     *
     * The Equalizer stays locked at the top of the stack, per
     * `docs/sound-mind-design.md`'s "Special Layers" - `createNew()`
     * creates at most one, and nothing else in this codebase ever
     * creates a second, but this check costs nothing even if that ever
     * changes, and keeps this method correct regardless of *how* an
     * Equalizer layer came to exist rather than relying on every caller
     * to know not to add layers after it.
     *
     * @param layer The layer to add. Its own id is ignored - a fresh,
     *        unique id is assigned to the appended copy instead, since a
     *        caller building a layer to import has no way to know what ids
     *        are already taken. Its own name is passed through
     *        uniqueLayerName() too (`v0.Y.44.1`, Layers Panel Redesign) -
     *        a caller never needs to check for a name collision itself.
     * @return The id actually assigned to the appended layer.
     */
    LayerId addLayer(Layer layer);

    /**
     * @brief A name guaranteed not to collide with any current layer's own
     *        name (except `excludingId`'s, if given) - `docs/sound-mind-
     *        design.md`'s "Layer panel styling" ("Layer names shall be
     *        unique - this can be enforced with serial-number suffixes"),
     *        `v0.Y.44.1` (Layers Panel Redesign).
     *
     * Appends `" (2)"`, `" (3)"`, ... (trying each in turn) until a free
     * name is found; returns `desiredName` unchanged if it's already
     * unique. `addLayer()` calls this itself for every newly added layer,
     * so every caller that creates one - `sound-mind-studio`'s "+ Add
     * Layer"/"+ Add Filter Layer" buttons, audio import, anything else -
     * gets a unique name automatically, with no extra wiring of its own
     * needed; `sound-mind-studio`'s own rename flow calls this directly
     * (with `excludingId` set to the layer being renamed) before actually
     * applying an edited name.
     *
     * @param desiredName The name to make unique.
     * @param excludingId A layer id whose own current name is ignored by
     *        the collision check - e.g. renaming a layer to the exact name
     *        it already has shouldn't get needlessly suffixed.
     *        `std::nullopt` (the default) excludes nothing, matching
     *        `addLayer()`'s own use for a layer that doesn't exist in this
     *        project yet.
     * @return A name that doesn't collide with any other current layer's.
     */
    [[nodiscard]] std::string uniqueLayerName(const std::string& desiredName,
                                               std::optional<LayerId> excludingId = std::nullopt) const;

    /**
     * @brief Removes the layer with the given id, if one exists.
     *
     * No restriction here on removing a `Background`/`Equalizer` layer -
     * that's a UI-level rule (`sound-mind-studio`'s `LayersPanel` doesn't
     * even show a delete button for them), not a `Project`-level
     * invariant, the same division `Layer::setVisible()`'s docs draw for
     * the Background-stays-visible rule.
     *
     * @param id The layer to remove.
     * @return `true` if a layer with this id was found and removed;
     *         `false` (no change) if none was.
     */
    bool removeLayer(LayerId id);

    /**
     * @brief Reorders the layer stack.
     *
     * @param newOrderBottomToTop Every current layer's id, exactly once
     *        each, in the desired new bottom-to-top order.
     * @return `true` and applies the reorder if `newOrderBottomToTop` is
     *         a valid permutation of the current layers' ids (same size,
     *         same set, no duplicates); `false` (no change) otherwise -
     *         e.g. a missing id, an unknown id, or a duplicate.
     */
    bool reorderLayers(const std::vector<LayerId>& newOrderBottomToTop);

    /// @brief This project's settings (sample rate, canvas size, etc).
    /// @return The settings this project currently holds.
    [[nodiscard]] const ProjectSettings& settings() const noexcept { return settings_; }

    /// @brief Sets which Principal Mode (`v0.Y.47.1`) new geometry is
    ///        authored under - see `PrincipalMode`'s own docs.
    ///
    /// The first, and so far only, post-construction mutator for any
    /// `ProjectSettings` field - every other field is fixed at Create
    /// Project Wizard time and never changes again, so this doesn't
    /// generalize into a broader `mutableSettings()`; it's scoped
    /// narrowly to the one field a user can actually toggle mid-project.
    ///
    /// @param mode The mode to switch to.
    void setPrincipalMode(PrincipalMode mode) noexcept { settings_.principalMode = mode; }

    /// @brief This project's layer stack, in bottom-to-top order.
    /// @return The layers this project currently holds.
    [[nodiscard]] const std::vector<Layer>& layers() const noexcept { return layers_; }

    /// @brief This project's layer stack, in bottom-to-top order - mutable
    ///        access, for in-place changes (renaming, opacity, Pooling,
    ///        and eventually painting) that don't change the stack's
    ///        membership or order (addLayer() is still how a new layer
    ///        gets appended).
    /// @return The layers this project currently holds.
    [[nodiscard]] std::vector<Layer>& layers() noexcept { return layers_; }

    /**
     * @brief Finds the layer with the given id, if one exists.
     *
     * A small, project-level lookup shared by every caller that needs
     * "the layer this operation/selection/clipboard targets" from just an
     * id - `sound-mind-studio`'s `MainWindow`, `PaintController`, and
     * `SelectionController` each needed the identical few-line linear
     * search independently before this existed here instead.
     *
     * @param id The layer to find.
     * @return A pointer to that layer, or `nullptr` if no layer with this
     *         id exists.
     */
    [[nodiscard]] const Layer* layerById(LayerId id) const noexcept;

    /// @brief Mutable overload of layerById() - for in-place changes
    ///        (setting content, renaming, opacity) that don't change the
    ///        stack's own membership or order.
    /// @param id The layer to find.
    /// @return A mutable pointer to that layer, or `nullptr` if no layer
    ///         with this id exists.
    [[nodiscard]] Layer* layerById(LayerId id) noexcept;

    /// @brief This project's single, project-wide operation log.
    /// @return The operation log this project currently holds.
    [[nodiscard]] const OperationLog& operationLog() const noexcept { return operationLog_; }

    /// @brief Mutable access to this project's operation log, for
    ///        appending new operations (painting and, eventually, every
    ///        other loggable action) and undo()/redo().
    /// @return The operation log this project currently holds.
    [[nodiscard]] OperationLog& operationLog() noexcept { return operationLog_; }

    /**
     * @brief This project's MindWave library - `v0.Y.31.1` Installment
     *        C1's own answer to "identity/storage arrives once something
     *        actually binds to a MindWave by reference" (a `Layer` binding
     *        its own opacity to one, via `Layer::opacityMindWave()`, is
     *        that first real binder). The same "peer resource library"
     *        shape `docs/sound-mind-architecture.md`'s own Core Data Model
     *        sketch already gives `SoundMindInstrument`/`ToolConfiguration`,
     *        landed here first since this is the first of those with a
     *        real consumer.
     * @return This project's current MindWave library.
     */
    [[nodiscard]] const std::vector<NamedMindWave>& mindWaves() const noexcept { return mindWaves_; }

    /// @brief This project's MindWave library - mutable access, for
    ///        in-place edits (renaming, tuning a wave's own parameters)
    ///        that don't change the library's own membership (addMindWave()
    ///        is still how a new entry gets appended).
    /// @return This project's current MindWave library.
    [[nodiscard]] std::vector<NamedMindWave>& mindWaves() noexcept { return mindWaves_; }

    /**
     * @brief Adds a new, named MindWave to this project's library.
     * @param name Display name - see `NamedMindWave::name`'s own docs on
     *        uniqueness being this project's own responsibility, not
     *        enforced here.
     * @param wave The MindWave itself.
     * @return The id assigned to the new entry - see `addLayer()`'s own
     *         docs for the identical "fresh, project-unique id" pattern.
     */
    MindWaveId addMindWave(std::string name, MindWave wave);

    /**
     * @brief Removes the MindWave with the given id, if one exists.
     *
     * Does **not** clear any `Layer::opacityMindWave()` (or, later, any
     * other binding) that still references this id - a dangling reference
     * is treated the same as "never bound" wherever a MindWave is resolved
     * by id (see `docs/sound-mind-architecture.md`'s own Decision on this),
     * not cascaded into a project-wide cleanup pass here.
     *
     * @param id The MindWave to remove.
     * @return `true` if a MindWave with this id was found and removed;
     *         `false` (no change) if none was.
     */
    bool removeMindWave(MindWaveId id);

    /**
     * @brief Finds the MindWave library entry with the given id, if one
     *        exists - the same "small, project-level lookup" `layerById()`
     *        already provides for layers.
     * @param id The entry to find.
     * @return A pointer to that entry, or `nullptr` if no MindWave with
     *         this id exists in this project's library.
     */
    [[nodiscard]] const NamedMindWave* mindWaveById(MindWaveId id) const noexcept;

    /// @brief Mutable overload of mindWaveById() - for in-place edits
    ///        (renaming, tuning a wave's own parameters).
    /// @param id The entry to find.
    /// @return A mutable pointer to that entry, or `nullptr` if no
    ///         MindWave with this id exists in this project's library.
    [[nodiscard]] NamedMindWave* mindWaveById(MindWaveId id) noexcept;

    /**
     * @brief This project's Mind Shot library - `v0.Y.33.1` Installment
     *        A's own permanent, named store of captured selections (see
     *        `NamedMindShot`'s own docs), the same "peer resource library"
     *        shape `mindWaves()` already established for MindWaves.
     * @return This project's current Mind Shot library.
     */
    [[nodiscard]] const std::vector<NamedMindShot>& mindShots() const noexcept { return mindShots_; }

    /// @brief This project's Mind Shot library - mutable access, for
    ///        in-place edits (renaming) that don't change the library's
    ///        own membership (addMindShot() is still how a new entry gets
    ///        appended).
    /// @return This project's current Mind Shot library.
    [[nodiscard]] std::vector<NamedMindShot>& mindShots() noexcept { return mindShots_; }

    /**
     * @brief Adds a new, named Mind Shot to this project's library.
     * @param name Display name - see `NamedMindShot::name`'s own docs on
     *        uniqueness being this project's own responsibility, not
     *        enforced here.
     * @param clip The captured content itself.
     * @return The id assigned to the new entry - see `addLayer()`'s own
     *         docs for the identical "fresh, project-unique id" pattern.
     */
    MindShotId addMindShot(std::string name, Clip clip);

    /**
     * @brief Removes the Mind Shot with the given id, if one exists.
     *
     * Does **not** clear any `MindShotConfiguration` (or, later, any
     * other reference) that still embeds this Mind Shot's own captured
     * content - a `MindShotConfiguration` snapshots the `Clip` itself at
     * configuration time (see its own docs), not a live reference back
     * into this library, so an already-painted stroke keeps rendering
     * correctly even after its source entry is removed here.
     *
     * @param id The Mind Shot to remove.
     * @return `true` if a Mind Shot with this id was found and removed;
     *         `false` (no change) if none was.
     */
    bool removeMindShot(MindShotId id);

    /**
     * @brief Finds the Mind Shot library entry with the given id, if one
     *        exists - the same "small, project-level lookup" `layerById()`/
     *        `mindWaveById()` already provide.
     * @param id The entry to find.
     * @return A pointer to that entry, or `nullptr` if no Mind Shot with
     *         this id exists in this project's library.
     */
    [[nodiscard]] const NamedMindShot* mindShotById(MindShotId id) const noexcept;

    /// @brief Mutable overload of mindShotById() - for in-place edits
    ///        (renaming).
    /// @param id The entry to find.
    /// @return A mutable pointer to that entry, or `nullptr` if no Mind
    ///         Shot with this id exists in this project's library.
    [[nodiscard]] NamedMindShot* mindShotById(MindShotId id) noexcept;

    /**
     * @brief This project's Mind Grain library - `v0.Y.33.1` Installment
     *        B's own permanent, named store of *live references* to a
     *        region on a layer (see `NamedMindGrain`'s own docs), the same
     *        "peer resource library" shape `mindShots()`/`mindWaves()`
     *        already establish.
     * @return This project's current Mind Grain library.
     */
    [[nodiscard]] const std::vector<NamedMindGrain>& mindGrains() const noexcept { return mindGrains_; }

    /// @brief This project's Mind Grain library - mutable access, for
    ///        in-place edits (renaming) that don't change the library's
    ///        own membership (addMindGrain() is still how a new entry gets
    ///        appended).
    /// @return This project's current Mind Grain library.
    [[nodiscard]] std::vector<NamedMindGrain>& mindGrains() noexcept { return mindGrains_; }

    /**
     * @brief Adds a new, named Mind Grain to this project's library.
     * @param name Display name - see `NamedMindGrain::name`'s own docs on
     *        uniqueness being this project's own responsibility, not
     *        enforced here.
     * @param sourceLayerId The layer this grain reads its live content
     *        from.
     * @param bounds The region within `sourceLayerId` this grain reads.
     * @return The id assigned to the new entry - see `addLayer()`'s own
     *         docs for the identical "fresh, project-unique id" pattern.
     */
    MindGrainId addMindGrain(std::string name, LayerId sourceLayerId, TimeFrequencyRect bounds);

    /**
     * @brief Removes the Mind Grain with the given id, if one exists.
     *
     * Does **not** clear any `MindGrainConfiguration` that still
     * references this entry - see `removeMindShot()`'s own docs for the
     * identical "dangling reference is treated as never-bound" reasoning,
     * here applying to `sourceMindGrainId()` (UI-only metadata) rather
     * than the actual painted content, which a `MindGrainConfiguration`
     * resolves independently via its own `sourceLayerId()`/`bounds()`.
     *
     * @param id The Mind Grain to remove.
     * @return `true` if a Mind Grain with this id was found and removed;
     *         `false` (no change) if none was.
     */
    bool removeMindGrain(MindGrainId id);

    /**
     * @brief Finds the Mind Grain library entry with the given id, if one
     *        exists - the same "small, project-level lookup" `layerById()`/
     *        `mindShotById()` already provide.
     * @param id The entry to find.
     * @return A pointer to that entry, or `nullptr` if no Mind Grain with
     *         this id exists in this project's library.
     */
    [[nodiscard]] const NamedMindGrain* mindGrainById(MindGrainId id) const noexcept;

    /// @brief Mutable overload of mindGrainById() - for in-place edits
    ///        (renaming).
    /// @param id The entry to find.
    /// @return A mutable pointer to that entry, or `nullptr` if no Mind
    ///         Grain with this id exists in this project's library.
    [[nodiscard]] NamedMindGrain* mindGrainById(MindGrainId id) noexcept;

    /**
     * @brief This project's convolution kernel library - `v0.Y.36.1`
     *        (Deferred Filters) Installment B's own permanent, named store
     *        of saved kernels (see `NamedConvolutionKernel`'s own docs),
     *        the same "peer resource library" shape `mindWaves()`/
     *        `mindShots()`/`mindGrains()` already establish.
     * @return This project's current convolution kernel library.
     */
    [[nodiscard]] const std::vector<NamedConvolutionKernel>& convolutionKernels() const noexcept {
        return convolutionKernels_;
    }

    /// @brief This project's convolution kernel library - mutable access,
    ///        for in-place edits (renaming) that don't change the
    ///        library's own membership (addConvolutionKernel() is still
    ///        how a new entry gets appended).
    /// @return This project's current convolution kernel library.
    [[nodiscard]] std::vector<NamedConvolutionKernel>& convolutionKernels() noexcept { return convolutionKernels_; }

    /**
     * @brief Adds a new, named convolution kernel to this project's
     *        library.
     * @param name Display name - see `NamedConvolutionKernel::name`'s own
     *        docs on uniqueness being this project's own responsibility,
     *        not enforced here.
     * @param size The kernel's own side length - see
     *        `NamedConvolutionKernel::size`'s own docs.
     * @param coefficients The kernel's own coefficients, row-major.
     * @param normalize See `NamedConvolutionKernel::normalize`'s own docs.
     * @return The id assigned to the new entry - see `addLayer()`'s own
     *         docs for the identical "fresh, project-unique id" pattern.
     */
    ConvolutionKernelId addConvolutionKernel(std::string name, int size, std::vector<float> coefficients,
                                              bool normalize);

    /**
     * @brief Removes the convolution kernel with the given id, if one
     *        exists.
     *
     * Does **not** affect any `FilterConfiguration` that already loaded
     * this kernel's own coefficients - see `removeMindShot()`'s own docs
     * for the identical "a loaded copy is independent, not a live
     * reference" reasoning, here applying to `FilterConfiguration::
     * convolveKernel()`'s own one-time-copy-on-load contract.
     *
     * @param id The kernel to remove.
     * @return `true` if a kernel with this id was found and removed;
     *         `false` (no change) if none was.
     */
    bool removeConvolutionKernel(ConvolutionKernelId id);

    /**
     * @brief Finds the convolution kernel library entry with the given id,
     *        if one exists - the same "small, project-level lookup"
     *        `layerById()`/`mindWaveById()` already provide.
     * @param id The entry to find.
     * @return A pointer to that entry, or `nullptr` if no kernel with this
     *         id exists in this project's library.
     */
    [[nodiscard]] const NamedConvolutionKernel* convolutionKernelById(ConvolutionKernelId id) const noexcept;

    /// @brief Mutable overload of convolutionKernelById() - for in-place
    ///        edits (renaming).
    /// @param id The entry to find.
    /// @return A mutable pointer to that entry, or `nullptr` if no kernel
    ///         with this id exists in this project's library.
    [[nodiscard]] NamedConvolutionKernel* convolutionKernelById(ConvolutionKernelId id) noexcept;

    friend void to_json(nlohmann::json& json, const Project& project);
    friend void from_json(const nlohmann::json& json, Project& project);

private:
    ProjectSettings settings_;
    std::vector<Layer> layers_;
    OperationLog operationLog_;
    std::vector<NamedMindWave> mindWaves_;
    std::vector<NamedMindShot> mindShots_;
    std::vector<NamedMindGrain> mindGrains_;
    std::vector<NamedConvolutionKernel> convolutionKernels_;
};

/// @brief Serializes a Project to its JSON representation.
void to_json(nlohmann::json& json, const Project& project);

/// @brief Parses a Project from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, Project& project);

}  // namespace sound_mind::core
