#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "sound_mind/codec/pool_codec.h"
#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::core {

/// @brief Opaque identifier for a Layer within a Project.
using LayerId = std::uint64_t;

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

/**
 * @brief One entry in a Project's layer stack.
 *
 * See `docs/sound-mind-design.md`'s "Layers" and
 * `docs/sound-mind-architecture.md`'s Core Data Model.
 *
 * @note Deliberately minimal for now: blend mode, MindWave linkage, and
 *       the cached raster result are not yet represented here - each
 *       depends on a type (`BlendMode`, `MindWave`, `RasterCache`) that
 *       hasn't been designed in code yet. Adding placeholder members for
 *       them now would just mean redesigning this class again as soon as
 *       those types exist. As of `v0.Y.21.1` (Layer Time Alignment), a
 *       narrow slice of "transform" *is* represented - translationColumns()
 *       and rescaleFactor(), the horizontal-axis-only subset described
 *       there - not the legacy Studio's full affine transform (no
 *       vertical translation, scale, or rotation).
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
     * @brief Sets whether this layer currently contributes to the project.
     * @param visible The new visibility. Not enforced here that a
     *        `Background` layer stays visible - that's a UI-level rule
     *        (`sound-mind-studio`'s `LayersPanel` disables the toggle for
     *        it), not a `Layer`-level invariant.
     */
    void setVisible(bool visible) noexcept { visible_ = visible; }

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

    friend void to_json(nlohmann::json& json, const Layer& layer);
    friend void from_json(const nlohmann::json& json, Layer& layer);

private:
    LayerId id_ = 0;
    std::string name_;
    LayerType type_ = LayerType::Normal;
    float opacity_ = 1.0f;
    bool visible_ = true;
    std::int64_t translationColumns_ = 0;
    double rescaleFactor_ = 1.0;
    std::optional<sound_mind::codec::StreamImage> content_;
    std::optional<sound_mind::codec::PoolImage> poolContent_;
};

/// @brief Serializes a Layer to its JSON representation.
void to_json(nlohmann::json& json, const Layer& layer);

/// @brief Parses a Layer from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, Layer& layer);

}  // namespace sound_mind::core
