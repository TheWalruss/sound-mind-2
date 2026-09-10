#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "sound_mind/core/gradient.h"

namespace sound_mind::core {

/**
 * @brief Which kind of painting tool a `ToolConfiguration` configures -
 *        see `docs/sound-mind-design.md`'s "Tool Configuration".
 *
 * @note Only `Procedural` exists as a real, paintable tool so far (the
 *       `v0.Y.24.1` "Basic Painting" milestone's own scope, per
 *       `docs/sound-mind-roadmap.md`) - `Instrument`/`MindShot`/
 *       `MindGrain`/`Smudge`/`OrderChaos`/`Heal`/`Soften`/`Clone` are
 *       future roadmap milestones (`v0.Y.30.1`, `v0.Y.31.1`, and others
 *       not yet scheduled). Adding a value here ahead of its own tool
 *       actually working is deliberate groundwork for the Tool
 *       Configuration Panel/Wizard's dynamic-per-type UI (this session's
 *       scope, confirmed with the user), not a claim that tool is usable.
 */
enum class ToolType {
    Procedural,
    Instrument,
    MindShot,
    MindGrain,
    Smudge,
    OrderChaos,
    Heal,
    Soften,
    Clone,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(ToolType, {
    {ToolType::Procedural, "procedural"},
    {ToolType::Instrument, "instrument"},
    {ToolType::MindShot, "mindShot"},
    {ToolType::MindGrain, "mindGrain"},
    {ToolType::Smudge, "smudge"},
    {ToolType::OrderChaos, "orderChaos"},
    {ToolType::Heal, "heal"},
    {ToolType::Soften, "soften"},
    {ToolType::Clone, "clone"},
})
// clang-format on

/**
 * @brief A brush tip's geometric footprint - see `docs/sound-mind-design.md`'s
 *        "Procedural Brushes".
 */
enum class BrushTipShape {
    Circle,
    Square,
    Diamond,
    Triangle,
    SingleStroke,
    Cross,
    Star,
    Corner,
    Arc,
    DotSpatter,
    Dapple,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(BrushTipShape, {
    {BrushTipShape::Circle, "circle"},
    {BrushTipShape::Square, "square"},
    {BrushTipShape::Diamond, "diamond"},
    {BrushTipShape::Triangle, "triangle"},
    {BrushTipShape::SingleStroke, "singleStroke"},
    {BrushTipShape::Cross, "cross"},
    {BrushTipShape::Star, "star"},
    {BrushTipShape::Corner, "corner"},
    {BrushTipShape::Arc, "arc"},
    {BrushTipShape::DotSpatter, "dotSpatter"},
    {BrushTipShape::Dapple, "dapple"},
})
// clang-format on

/**
 * @brief A named, savable/shareable painting-tool setup - see
 *        `docs/sound-mind-design.md`'s "Tool Configuration".
 *
 * Every `PaintOperation` carries its own `ToolConfiguration` (a snapshot
 * of whatever was configured at the moment it was painted, not a
 * reference into a shared list) - a separate, project-owned collection of
 * named presets (the Tool Configuration Panel's own "Tool Preset"
 * drop-down draws from) is a later installment of this same milestone.
 *
 * @note Only `Procedural`'s own parameters exist as real fields so far,
 *       matching `ToolType`'s own note - deliberately *not* modeled as a
 *       generic key/value `ParamSet` (see `docs/sound-mind-architecture.md`'s
 *       Decision #34): with a single real tool type, plain typed fields
 *       are simpler and safer than an opaque bag would be, at the cost of
 *       needing real rework (a tagged union, or per-type subclassing) once
 *       a second tool type's parameters actually need to coexist with
 *       these - deferred until that's a real, not speculative, need.
 */
class ToolConfiguration {
public:
    /// @brief Constructs a Procedural configuration with a plain, medium
    ///        circular tip and a fresh, fully transparent default gradient.
    ToolConfiguration() = default;

    /// @brief Which kind of tool this configures.
    /// @return The tool type this configuration was constructed with.
    [[nodiscard]] ToolType type() const noexcept { return type_; }

    /// @brief This configuration's own saved name.
    /// @return The name it was last saved under, or an empty string for
    ///         one that's never been saved as a named preset.
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    /// @brief Names (or renames) this configuration.
    /// @param name The new name; uniqueness among a project's saved
    ///        presets is the project's responsibility, not enforced here -
    ///        the same division `Layer::setName()`'s own docs draw.
    void setName(std::string name) { name_ = std::move(name); }

    /// @brief The Procedural brush tip's geometric footprint.
    /// @return The currently configured tip shape.
    [[nodiscard]] BrushTipShape tipShape() const noexcept { return tipShape_; }

    /// @brief Sets the Procedural brush tip's geometric footprint.
    /// @param shape The new tip shape.
    void setTipShape(BrushTipShape shape) noexcept { tipShape_ = shape; }

    /**
     * @brief The Procedural brush tip's edge softness.
     * @return A value in `[0, 1]` - `0` is a hard edge, `1` the softest
     *         falloff; not clamped or validated here.
     */
    [[nodiscard]] float falloff() const noexcept { return falloff_; }

    /// @brief Sets the Procedural brush tip's edge softness.
    /// @param falloff Intended to be in `[0, 1]`; not clamped or validated here.
    void setFalloff(float falloff) noexcept { falloff_ = falloff; }

    /**
     * @brief The brush tip's own size.
     *
     * In the same seconds-equivalent normalized space `fitPathToPoints()`'s
     * own `frequencyToTimeScale` parameter establishes (see `path.h`) -
     * resolution/project-agnostic, converted to real canvas pixels only at
     * the point of actually stamping or displaying it.
     *
     * @return The tip's radius, in seconds-equivalent units; not clamped
     *         or validated here.
     */
    [[nodiscard]] double size() const noexcept { return size_; }

    /// @brief Sets the brush tip's own size.
    /// @param size The new radius, in seconds-equivalent units (see
    ///        size()'s own docs); intended to be positive, not clamped or
    ///        validated here.
    void setSize(double size) noexcept { size_ = size; }

    /// @brief This tool's own default gradient - seeds a new Path's own
    ///        gradient (see `path.h`) whenever painting starts with this
    ///        configuration; the artist can then further customize that
    ///        copy per-stroke without affecting this configuration's own.
    /// @return The default gradient painting with this tool starts from.
    [[nodiscard]] const Gradient& defaultGradient() const noexcept { return defaultGradient_; }

    /// @brief Mutable access to this tool's own default gradient, for
    ///        in-place edits.
    /// @return The default gradient painting with this tool starts from.
    [[nodiscard]] Gradient& defaultGradient() noexcept { return defaultGradient_; }

    friend void to_json(nlohmann::json& json, const ToolConfiguration& config);
    friend void from_json(const nlohmann::json& json, ToolConfiguration& config);

private:
    ToolType type_ = ToolType::Procedural;
    std::string name_;
    BrushTipShape tipShape_ = BrushTipShape::Circle;
    float falloff_ = 0.5f;
    double size_ = 1.0;
    Gradient defaultGradient_;
};

/// @brief Serializes a tool configuration to its JSON representation.
void to_json(nlohmann::json& json, const ToolConfiguration& config);

/// @brief Parses a tool configuration from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, ToolConfiguration& config);

}  // namespace sound_mind::core
