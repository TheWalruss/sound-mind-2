#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/blend_mode.h"
#include "sound_mind/core/filter_configuration.h"

namespace sound_mind::core {

/// @brief Opaque identifier for a Layer within a Project.
using LayerId = std::uint64_t;

/// @brief Opaque identifier for a `NamedMindWave` within a Project - a
/// deliberate, exact duplicate of `mind_wave.h`'s own `MindWaveId` alias
/// (matching `docs/sound-mind-architecture.md`'s own Decision #59
/// "duplicated, not shared" precedent for something this small), not a
/// `#include "sound_mind/core/mind_wave.h"` here - that header includes
/// `path.h`, which includes `operation.h`, which includes *this* header,
/// so including it here would be a real cycle. A type alias (unlike a
/// class) can be redeclared identically in multiple headers with no ODR
/// concern, since it never needs its own definition to be complete.
using MindWaveId = std::uint64_t;

/**
 * @brief What a Layer is for, per `docs/sound-mind-design.md`'s "Layer Types".
 */
enum class LayerType {
    Normal,      ///< Paintable directly.
    Filter,      ///< Composites the layers beneath it and applies a filter to the result.
    Background,  ///< The locked, always-visible floor of the layer stack.
    Equalizer,   ///< The locked, top-of-stack Filter layer of Equalizer type.
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(LayerType, {
    {LayerType::Normal, "normal"},
    {LayerType::Filter, "filter"},
    {LayerType::Background, "background"},
    {LayerType::Equalizer, "equalizer"},
})
// clang-format on

/// @brief Whether `type` is one of the two Filter layer kinds - see
/// `docs/sound-mind-design.md`'s "Special Layers" (the Equalizer is "a
/// Filter layer of Equalizer type", not a separate concept). Shared here
/// rather than reimplemented at each of its own call sites
/// (`compositor.cpp`, `sound-mind-studio`'s `MainWindow`) - Refactor &
/// Clean Up, `v0.Y.29.1`.
[[nodiscard]] constexpr bool isFilterLayerType(LayerType type) noexcept {
    return type == LayerType::Filter || type == LayerType::Equalizer;
}

/// @brief Whether `type` is one of the fixed-position layer types - no
/// drag handle, no delete button, no rename (see `LayersPanel`'s own
/// docs). Shared here rather than reimplemented at each of its own call
/// sites (`LayersPanel`, `MainWindow`) - Refactor & Clean Up, `v0.Y.29.1`.
[[nodiscard]] constexpr bool isLockedLayerType(LayerType type) noexcept {
    return type == LayerType::Background || type == LayerType::Equalizer;
}

/**
 * @brief A layer's base content and how it was made (`v0.1.43.1`) - what
 *        the project's `History` keeps to undo replacing it.
 *
 * Shared, immutable images: a base kept by the history is never copied
 * again.
 */
struct LayerBase {
    /// The base content; `nullptr` for a layer without content.
    std::shared_ptr<const sound_mind::codec::StreamImage> stream;
    /// How the base was made - see Layer::setBaseContent().
    nlohmann::json recipe;
    /// Its Layer::baseRevision().
    std::uint64_t revision = 0;

    /// @brief Roughly how much memory the image takes.
    /// @return Bytes.
    [[nodiscard]] std::size_t byteSize() const noexcept;
};

/**
 * @brief One entry in a Project's layer stack.
 *
 * See `docs/sound-mind-design.md`'s "Layers" and
 * `docs/sound-mind-architecture.md`'s Core Data Model.
 *
 * @note Deliberately minimal for now: the cached raster result is not yet
 *       represented here - it depends on a type (`RasterCache`) that hasn't
 *       been designed in code yet. Adding a placeholder member for it now
 *       would just mean redesigning this class again as soon as that type
 *       exists. As of `v0.Y.21.1` (Layer Time Alignment), a narrow slice of
 *       "transform" *is* represented - translationColumns() and
 *       rescaleFactor(), the horizontal-axis-only subset described there -
 *       not the legacy Studio's full affine transform (no vertical
 *       translation, scale, or rotation). As of `v0.Y.31.1` Installment C1,
 *       MindWave linkage *is* also represented - opacityMindWave(), a
 *       `std::optional<MindWaveId>` naming an entry in the owning
 *       `Project`'s own `mindWaves()` library, not an embedded `MindWave`
 *       value. As of `v0.Y.37.1` (Deferred Blend Modes), blendMode() *is*
 *       also represented - see its own docs.
 */
class Layer {
public:
    /// @brief Default-constructs a Layer with no identity yet.
    ///
    /// Exists so `from_json` (a free function, not a member) can populate
    /// an instance in place via the friend declaration below, matching
    /// nlohmann::json's default (de)serialization convention. Prefer the
    /// parameterized constructor for normal use.
    Layer() = default;

    /**
     * @param id Identity to give this layer within its project.
     * @param name Display name. The project (not this class) is
     *        responsible for keeping names unique within itself.
     * @param type Which of `docs/sound-mind-design.md`'s layer types
     *        this layer is.
     */
    Layer(LayerId id, std::string name, LayerType type)
        : id_(id), name_(std::move(name)), type_(type) {}

    /**
     * @brief This layer's own identity within its project.
     * @return The id this layer was constructed with.
     */
    [[nodiscard]] LayerId id() const noexcept { return id_; }

    /**
     * @brief This layer's current display name.
     * @return The name last set via the constructor or setName().
     */
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    /**
     * @brief Which of `docs/sound-mind-design.md`'s layer types this is.
     * @return The type this layer was constructed with; a layer's type
     *         does not change after construction.
     */
    [[nodiscard]] LayerType type() const noexcept { return type_; }

    /**
     * @brief How strongly this layer contributes to the composite.
     * @return A value in [0, 1]; not clamped or validated here.
     */
    [[nodiscard]] float opacity() const noexcept { return opacity_; }

    /**
     * @brief The MindWave (if any) this layer's own opacity is bound to -
     *        see `docs/sound-mind-design.md`'s "Layer opacity": "Linking a
     *        MindWave to a layer multiplies the layer's effective opacity
     *        by that field at every pixel during compositing."
     *
     * A reference (an id into the owning `Project`'s own `mindWaves()`
     * library), not an embedded `MindWave` value - confirmed with the
     * user, matching `docs/sound-mind-architecture.md`'s own `MindWaveRef`
     * sketch. An id that no longer resolves (its `NamedMindWave` was
     * removed from the project) is treated the same as `std::nullopt`
     * wherever this is resolved (`sound_mind::core::compositeProject()`,
     * in particular) - not a dangling-reference error.
     *
     * @return The bound MindWave's id, or `std::nullopt` if this layer's
     *         opacity is a plain scalar (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> opacityMindWave() const noexcept { return opacityMindWave_; }

    /**
     * @brief This layer's own stereo balance - "Layers Panel & Editing
     *        Enhancements v2" (`v0.Y.46.1` Installment C), alongside
     *        opacity(): opacity attenuates this layer's overall
     *        contribution; balance controls how that contribution splits
     *        between channels.
     *
     * A linear pan law where the panned-*toward* channel is always held
     * at its own original, full gain and only the *opposite* channel is
     * attenuated: `0.0` silences the right channel entirely (left
     * unchanged); `1.0` silences the left channel entirely (right
     * unchanged); `0.5` (the default) leaves both channels completely
     * unchanged - a true no-op. Deliberately **not**
     * `FilterConfiguration::channelBalance()`'s own energy-redistribution
     * formula (`total = left + right`, then re-split by balance) - that
     * formula is not, in general, a no-op at its own `0.5` default either
     * (only for an already-centered signal), which is fine for an
     * intentionally-added Filter effect but wrong for an always-present
     * per-layer property, which needs a genuinely inert default so every
     * existing project's own recorded stereo image survives unchanged.
     *
     * @return The current balance; `0.5` (the default) means centered/
     *         unchanged. Not clamped or validated here, same as opacity().
     */
    [[nodiscard]] float balance() const noexcept { return balance_; }

    /**
     * @brief Whether this layer currently contributes to the project at
     *        all - see `docs/sound-mind-roadmap.md`'s Layers Panel
     *        milestone (`v0.Y.13.1`).
     *
     * A hidden layer is left out of compositing, Playback and export.
     *
     * @return `true` unless explicitly hidden via setVisible(false).
     */
    [[nodiscard]] bool visible() const noexcept { return visible_; }

    /**
     * @brief Whether this layer is currently muted - real-world testing
     *        pass "Layers Panel & Editing Enhancements v2" (`v0.Y.46.1`
     *        Installment B), distinct from visible().
     *
     * A muted layer still contributes to the visual canvas exactly as any
     * other visible layer does (`visible()` alone still governs that) -
     * only whatever composite drives *audio* playback excludes it (see
     * `sound_mind::core::compositeProject()`'s own `respectMute`
     * parameter). Meaningless (never consulted) while `visible()` is
     * `false` - an invisible layer is already excluded from every
     * composite, audio included, regardless of this flag.
     *
     * @return `true` if muted; `false` (the default) otherwise.
     */
    [[nodiscard]] bool muted() const noexcept { return muted_; }

    /**
     * @brief This layer's horizontal time-axis shift, in spectrogram
     *        columns - see `docs/sound-mind-roadmap.md`'s Layer Time
     *        Alignment milestone (`v0.Y.21.1`).
     *
     * Applied by `sound_mind::core::renderLayer()`, not baked into
     * content(): a positive value shifts the layer's rendered content
     * later in time (right), a negative value shifts it earlier (left).
     * Columns, not seconds - a raw spectrogram-column count needs no
     * sample-rate/hop-length conversion at render time, unlike a
     * seconds-based value would.
     *
     * @return The current shift; `0` (the default) means untranslated.
     */
    [[nodiscard]] std::int64_t translationColumns() const noexcept { return translationColumns_; }

    /**
     * @brief This layer's horizontal timeline stretch/compress ratio - see
     *        translationColumns()'s docs for the milestone this belongs to.
     *
     * Applied by `sound_mind::core::renderLayer()` before translation:
     * `> 1.0` stretches (slows) the layer's own timeline, `< 1.0`
     * compresses (speeds it up). Not clamped or validated here, same as
     * opacity().
     *
     * @return The current ratio; `1.0` (the default) means unrescaled.
     */
    [[nodiscard]] double rescaleFactor() const noexcept { return rescaleFactor_; }

    /**
     * @brief Reassigns the layer's id.
     *
     * Not needed for normal use - a Layer's id is meant to be set once at
     * construction. Exists specifically for `Project::addLayer()`, which
     * has to assign a fresh, project-unique id to whatever id a caller's
     * about-to-be-imported Layer happened to be constructed with.
     *
     * @param id The new id.
     */
    void setId(LayerId id) noexcept { id_ = id; }

    /**
     * @brief Renames the layer.
     * @param name The new display name. Uniqueness within the project is
     *        the project's responsibility, not enforced here.
     */
    void setName(std::string name) { name_ = std::move(name); }

    /**
     * @brief Sets how strongly this layer contributes to the composite.
     * @param opacity Intended to be in [0, 1]; not clamped or validated here.
     */
    void setOpacity(float opacity) noexcept { opacity_ = opacity; }

    /**
     * @brief Sets (or clears) which MindWave this layer's opacity is bound to.
     * @param mindWaveId The new binding, or `std::nullopt` to unbind (a
     *        plain scalar `opacity()` again). Not validated against the
     *        owning `Project`'s own library here - see `opacityMindWave()`'s
     *        own docs on a non-resolving id being harmless.
     */
    void setOpacityMindWave(std::optional<MindWaveId> mindWaveId) noexcept { opacityMindWave_ = mindWaveId; }

    /// @brief Sets this layer's own stereo balance.
    /// @param balance The new balance, intended to be in `[0, 1]` - see
    ///        balance()'s own docs.
    void setBalance(float balance) noexcept { balance_ = balance; }

    /// @brief Whether this layer was made by Recording - what the Record
    ///        panel lists as a target to record over.
    /// @return `true` for a recorded layer; `false` by default.
    [[nodiscard]] bool recorded() const noexcept { return recorded_; }

    /// @brief Marks (or unmarks) this layer as made by Recording.
    /// @param recorded The new mark.
    void setRecorded(bool recorded) noexcept { recorded_ = recorded; }

    /**
     * @brief Sets whether this layer currently contributes to the project.
     * @param visible The new visibility. Not enforced here that a
     *        `Background` layer stays visible - that's a UI-level rule
     *        (`sound-mind-studio`'s `LayersPanel` disables the toggle for
     *        it), not a `Layer`-level invariant.
     */
    void setVisible(bool visible) noexcept { visible_ = visible; }

    /// @brief Sets whether this layer is currently muted - see muted()'s
    ///        own docs.
    /// @param muted The new muted state.
    void setMuted(bool muted) noexcept { muted_ = muted; }

    /// @brief Sets this layer's horizontal time-axis shift.
    /// @param columns The new shift, in spectrogram columns - see
    ///        translationColumns()'s docs.
    void setTranslationColumns(std::int64_t columns) noexcept { translationColumns_ = columns; }

    /// @brief Sets this layer's horizontal timeline stretch/compress ratio.
    /// @param factor The new ratio - see rescaleFactor()'s docs. Intended
    ///        to be positive; not clamped or validated here.
    void setRescaleFactor(double factor) noexcept { rescaleFactor_ = factor; }

    /**
     * @brief This layer's cached Stream-encoded content, if it's been
     *        rendered at least once (by importing media into it, or -
     *        once painting exists - by painting on it).
     * @return The cached content, or `std::nullopt` for a layer with
     *         nothing rendered yet (e.g. a fresh Background layer).
     *
     * @note In-memory only - deliberately not part of `to_json`/`from_json`.
     *       Per `docs/sound-mind-architecture.md`'s Project File & Folder
     *       layout, a layer's cached render is persisted as its own Stream
     *       file under the project's `media/` folder, not inlined into the
     *       JSON project file; `Project::save()`/`load()` are what read and
     *       write that file, deriving its name from this layer's id.
     */
    [[nodiscard]] const std::optional<sound_mind::codec::StreamImage>& content() const noexcept { return content_; }

    /**
     * @brief Sets this layer's *rendered* content - its base content with
     *        its operations replayed on top (`PaintController`, `History`).
     *
     * Splits the base off first if content() still doubles as it (see
     * baseContent()), so the base survives. Media that *replaces* what the
     * layer is built on - an import, a recording, a generated layer, phase
     * cleanup - goes through setBaseContent() instead.
     *
     * @param content The new rendered content.
     */
    void setContent(sound_mind::codec::StreamImage content) {
        splitBase();
        content_ = std::move(content);
    }

    /**
     * @brief The content this layer's operations are replayed on top of
     *        (`v0.1.43.1`) - what an import, recording or generator
     *        produced, or what phase cleanup baked.
     *
     * Kept apart from content() only once something has been rendered on
     * top of it; until then content() *is* the base, so an unpainted layer
     * holds one copy, not two.
     *
     * @return The base content, or `nullptr` for a layer with none.
     */
    [[nodiscard]] const sound_mind::codec::StreamImage* baseContent() const noexcept {
        return base_ ? base_.get() : (content_ ? &*content_ : nullptr);
    }

    /**
     * @brief Replaces what this layer is built on (`v0.1.43.1`): its base
     *        and rendered content both become `content`.
     *
     * Gives the layer a new baseRevision(), which is how the project's
     * `History` tells a new base apart from an unchanged one, and keeps the
     * replaced base for it (takeDisplacedBase()). Operations
     * already logged on the layer stay as they are - re-render them with
     * setContent() if any are active.
     *
     * @param content The new base content.
     * @param recipe How it was made, for replaying it from the project's
     *        sources: a JSON object with at least a `"kind"` (see
     *        `docs/sound-mind-architecture.md`'s *History, Sources and
     *        Macros*), or `nullptr` if it can't be remade.
     */
    void setBaseContent(sound_mind::codec::StreamImage content, nlohmann::json recipe = nullptr);

    /**
     * @brief Takes this layer's base out, leaving it with no content -
     *        `History` keeping the base it's about to replace, and
     *        `Project::removeLayer()` keeping a removed layer's (`v0.1.43.1`).
     *
     * Moves rather than copies: the image ends up shared by whoever keeps
     * the returned value.
     *
     * @return The base, its recipe and revision; an empty
     *         `stream` for a layer that had no content.
     */
    [[nodiscard]] LayerBase releaseBase();

    /**
     * @brief Makes `base` this layer's base and rendered content -
     *        `History` undoing or redoing an entry that changed it.
     *
     * Restores the revision too, so the history sees exactly the state it
     * recorded - or, for a base remade by replay (revision `0`), gives it a
     * fresh one. The rendered content starts out as the base; re-render it
     * if operations target the layer.
     *
     * @param base The base to restore; an empty `stream` leaves the layer
     *        without content.
     */
    void restoreBase(const LayerBase& base);

    /**
     * @brief The base this layer had before the first setBaseContent()
     *        since the last takeDisplacedBase() - how the history gets hold
     *        of a replaced base without copying it.
     * @return That base, or `std::nullopt` if the base hasn't been replaced.
     */
    [[nodiscard]] std::optional<LayerBase> takeDisplacedBase() {
        std::optional<LayerBase> displaced = std::move(displaced_);
        displaced_.reset();
        return displaced;
    }

    /// @brief Identifies the current base content: a fresh value on every
    ///        setBaseContent(), carried by restoreBase(); `0` for a layer
    ///        whose base was never set. In-memory only.
    /// @return The revision.
    [[nodiscard]] std::uint64_t baseRevision() const noexcept { return baseRevision_; }

    /// @brief How the current base content was made - see setBaseContent().
    /// @return The recipe JSON, or `null` if none was given.
    [[nodiscard]] const nlohmann::json& baseRecipe() const noexcept { return baseRecipe_; }

    /// @brief Completes the current base's recipe without making it a new
    ///        base (the revision stays) - an import naming its source once
    ///        the file has been copied into the project (`v0.1.43.2`).
    /// @param recipe The recipe.
    void setBaseRecipe(nlohmann::json recipe) { baseRecipe_ = std::move(recipe); }

    /**
     * @brief This layer's own filter type and parameters - see
     *        `docs/sound-mind-design.md`'s "Filter Layer" and
     *        `FilterConfiguration`'s own docs.
     *
     * Unconditionally present on every layer, the same as opacity()/
     * translationColumns() - meaningless unless type() is `Filter` or
     * `Equalizer` (a Normal/Background layer is never filtered), the same
     * "always there, sometimes not applicable" shape those two already
     * establish rather than wrapping this one field alone in
     * `std::optional`.
     *
     * @return The current filter configuration.
     */
    [[nodiscard]] const FilterConfiguration& filterConfiguration() const noexcept { return filterConfiguration_; }

    /// @brief Mutable access to this layer's own filter configuration,
    ///        for in-place edits.
    /// @return The current filter configuration.
    [[nodiscard]] FilterConfiguration& filterConfiguration() noexcept { return filterConfiguration_; }

    /// @brief Sets this layer's own filter configuration wholesale.
    /// @param config The new configuration - see filterConfiguration()'s own docs.
    void setFilterConfiguration(FilterConfiguration config) { filterConfiguration_ = std::move(config); }

    /**
     * @brief How this layer's own contribution combines with what's
     *        already composited beneath it - `docs/sound-mind-design.md`'s
     *        "Compositing", `v0.Y.37.1` (Deferred Blend Modes).
     *
     * Dispatched through `sound_mind::core::applyBlendedCell()` by
     * `compositor.cpp`'s own `mixLayerInto()` for every layer.
     *
     * @return The current blend mode; `BlendMode::Normal` (this codebase's
     *         own pre-`v0.Y.37.1` audio-style summing behavior) by default -
     *         a layer saved before this milestone existed always gets this
     *         value, reproducing its own prior, only-ever-Normal behavior
     *         exactly.
     */
    [[nodiscard]] BlendMode blendMode() const noexcept { return blendMode_; }

    /// @brief Sets this layer's own blend mode.
    /// @param mode The new mode - see blendMode()'s own docs.
    void setBlendMode(BlendMode mode) noexcept { blendMode_ = mode; }

    /**
     * @brief Copies every property of `other` - everything its JSON holds -
     *        except its base and content (`History` restoring a layer's
     *        properties without touching its images).
     * @param other The layer to copy from; its id is copied too.
     */
    void assignProperties(const Layer& other);

    friend void to_json(nlohmann::json& json, const Layer& layer);
    friend void from_json(const nlohmann::json& json, Layer& layer);

private:
    LayerId id_ = 0;
    std::string name_;
    LayerType type_ = LayerType::Normal;
    float opacity_ = 1.0f;
    std::optional<MindWaveId> opacityMindWave_;
    float balance_ = 0.5f;
    bool recorded_ = false;
    bool visible_ = true;
    bool muted_ = false;
    std::int64_t translationColumns_ = 0;
    double rescaleFactor_ = 1.0;
    std::optional<sound_mind::codec::StreamImage> content_;
    /// The base, once something is rendered on top of it - see baseContent().
    std::shared_ptr<const sound_mind::codec::StreamImage> base_;
    std::uint64_t baseRevision_ = 0;
    nlohmann::json baseRecipe_;
    /// The base the first setBaseContent() since takeDisplacedBase() replaced.
    std::optional<LayerBase> displaced_;
    FilterConfiguration filterConfiguration_;
    BlendMode blendMode_ = BlendMode::Normal;

    /// @brief Keeps the base apart from content() before content() is
    ///        overwritten with a render - see baseContent().
    void splitBase() {
        if (!base_ && content_) {
            base_ = std::make_shared<const sound_mind::codec::StreamImage>(*content_);
        }
    }
};

/// @brief Serializes a Layer to its JSON representation.
void to_json(nlohmann::json& json, const Layer& layer);

/// @brief Parses a Layer from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, Layer& layer);

}  // namespace sound_mind::core
