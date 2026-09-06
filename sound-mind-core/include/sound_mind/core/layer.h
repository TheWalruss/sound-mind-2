#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

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
 * @note Deliberately minimal for now: blend mode, MindWave linkage,
 *       transform, and the cached raster result are not yet represented
 *       here - each depends on a type (`BlendMode`, `MindWave`,
 *       `RasterCache`) that hasn't been designed in code yet. Adding
 *       placeholder members for them now would just mean redesigning
 *       this class again as soon as those types exist.
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

    friend void to_json(nlohmann::json& json, const Layer& layer);
    friend void from_json(const nlohmann::json& json, Layer& layer);

private:
    LayerId id_ = 0;
    std::string name_;
    LayerType type_ = LayerType::Normal;
    float opacity_ = 1.0f;
};

/// @brief Serializes a Layer to its JSON representation.
void to_json(nlohmann::json& json, const Layer& layer);

/// @brief Parses a Layer from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, Layer& layer);

}  // namespace sound_mind::core
