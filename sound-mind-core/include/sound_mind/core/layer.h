#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "sound_mind/codec/pool_codec.h"
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
     * @brief Whether this layer currently contributes to the project at
     *        all - see `docs/sound-mind-roadmap.md`'s Layers Panel
     *        milestone (`v0.Y.13.1`).
     *
     * A hidden layer is skipped by "topmost layer with content" logic
     * (`sound-mind-studio`'s `MainWindow::topmostLayerWithContent()`, in
     * particular) - real multi-layer compositing (which would need this
     * for blending, not just skipping) is still separately deferred.
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

    /// @brief Sets this layer's cached Stream-encoded content.
    /// @param content The new content, replacing anything previously cached.
    void setContent(sound_mind::codec::StreamImage content) { content_ = std::move(content); }

    /**
     * @brief This layer's cached Pool-encoded content, if it's been
     *        Pooled at least once (see `sound_mind::core::poolLayer()`).
     * @return The cached Pool content, or `std::nullopt` for a layer that
     *         has never been Pooled.
     *
     * @note In-memory only, same reasoning as content(): persisted as its
     *       own Pool file under the project's `pool/` folder, not inlined
     *       into the JSON project file.
     */
    [[nodiscard]] const std::optional<sound_mind::codec::PoolImage>& poolContent() const noexcept {
        return poolContent_;
    }

    /// @brief Sets this layer's cached Pool-encoded content.
    /// @param content The new content, replacing anything previously cached.
    void setPoolContent(sound_mind::codec::PoolImage content) { poolContent_ = std::move(content); }

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

    friend void to_json(nlohmann::json& json, const Layer& layer);
    friend void from_json(const nlohmann::json& json, Layer& layer);

private:
    LayerId id_ = 0;
    std::string name_;
    LayerType type_ = LayerType::Normal;
    float opacity_ = 1.0f;
    std::optional<MindWaveId> opacityMindWave_;
    bool visible_ = true;
    bool muted_ = false;
    std::int64_t translationColumns_ = 0;
    double rescaleFactor_ = 1.0;
    std::optional<sound_mind::codec::StreamImage> content_;
    std::optional<sound_mind::codec::PoolImage> poolContent_;
    FilterConfiguration filterConfiguration_;
    BlendMode blendMode_ = BlendMode::Normal;
};

/// @brief Serializes a Layer to its JSON representation.
void to_json(nlohmann::json& json, const Layer& layer);

/// @brief Parses a Layer from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, Layer& layer);

}  // namespace sound_mind::core
