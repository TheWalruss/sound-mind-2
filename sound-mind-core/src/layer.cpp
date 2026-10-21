#include "sound_mind/core/layer.h"

#include <atomic>

namespace sound_mind::core {

void to_json(nlohmann::json& json, const Layer& layer) {
    json = nlohmann::json{
        {"id", layer.id_},
        {"name", layer.name_},
        {"type", layer.type_},
        {"opacity", layer.opacity_},
        {"balance", layer.balance_},
        {"recorded", layer.recorded_},
        {"visible", layer.visible_},
        {"muted", layer.muted_},
        {"translationColumns", layer.translationColumns_},
        {"rescaleFactor", layer.rescaleFactor_},
        {"filterConfiguration", layer.filterConfiguration_},
        {"blendMode", layer.blendMode_},
    };
    // Same "only write if present" convention path.cpp's own
    // handleIn/handleOut already establish for an std::optional field.
    if (layer.opacityMindWave_.has_value()) {
        json["opacityMindWaveId"] = *layer.opacityMindWave_;
    }
    if (!layer.baseRecipe_.is_null()) {
        json["baseRecipe"] = layer.baseRecipe_;
    }
}

void from_json(const nlohmann::json& json, Layer& layer) {
    json.at("id").get_to(layer.id_);
    json.at("name").get_to(layer.name_);
    json.at("type").get_to(layer.type_);
    json.at("opacity").get_to(layer.opacity_);
    // Lenient (defaults to centered/unchanged if absent), same reasoning
    // as visible_ below - didn't exist before v0.Y.46.1 Installment C
    // (Per-layer balance); a layer saved before this milestone was
    // implicitly always centered anyway.
    layer.balance_ = json.value("balance", 0.5f);
    layer.recorded_ = json.value("recorded", false);
    // Lenient (defaults to true if absent) - didn't exist before v0.Y.13.1
    // (Layers Panel); requiring it here would make it a breaking change
    // to an already-established format, per the same reasoning
    // ProjectSettings' own new fields used in v0.Y.11.1 - a layer saved
    // before this milestone was implicitly always visible anyway.
    layer.visible_ = json.value("visible", true);
    // Lenient (defaults to unmuted if absent), same reasoning as visible_
    // above - didn't exist before v0.Y.46.1 Installment B (Mute); a layer
    // saved before this milestone was implicitly always unmuted anyway.
    layer.muted_ = json.value("muted", false);
    // Lenient (defaults to untranslated/unrescaled if absent), same
    // reasoning as visible_ above - didn't exist before v0.Y.21.1 (Layer
    // Time Alignment); a layer saved before this milestone was implicitly
    // untranslated and unrescaled anyway.
    layer.translationColumns_ = json.value("translationColumns", std::int64_t{0});
    layer.rescaleFactor_ = json.value("rescaleFactor", 1.0);
    // Lenient (defaults to a fresh FilterConfiguration if absent), same
    // reasoning as visible_/translationColumns_/rescaleFactor_ above -
    // didn't exist before v0.Y.28.1 (Filter Layers); a layer saved before
    // this milestone was never a Filter-type layer anyway, so its own
    // (meaningless, for that layer) default filter configuration is a
    // harmless fallback.
    layer.filterConfiguration_ = json.value("filterConfiguration", FilterConfiguration{});
    // Lenient (defaults to unbound if absent), same reasoning as the
    // fields above - didn't exist before v0.Y.31.1 Installment C1; a
    // layer saved before this milestone was never MindWave-bound anyway.
    layer.opacityMindWave_ = json.contains("opacityMindWaveId")
                                  ? std::optional(json.at("opacityMindWaveId").get<MindWaveId>())
                                  : std::nullopt;
    // Lenient (defaults to Normal if absent), same reasoning as the fields
    // above - didn't exist before v0.Y.37.1 (Deferred Blend Modes); a layer
    // saved before this milestone was implicitly always Normal anyway.
    layer.blendMode_ = json.value("blendMode", BlendMode::Normal);
    layer.baseRecipe_ = json.contains("baseRecipe") ? json.at("baseRecipe") : nlohmann::json();
}

namespace {

/// Hands out a fresh base revision - see Layer::baseRevision(). Atomic, so
/// layers built on a background thread (an import, a replay) never share one.
std::uint64_t nextBaseRevision() {
    static std::atomic<std::uint64_t> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace

void Layer::setBaseContent(sound_mind::codec::StreamImage content, nlohmann::json recipe,
                           std::optional<sound_mind::codec::PoolImage> pool) {
    if (!displaced_.has_value()) {
        displaced_ = releaseBase();
    }
    content_ = std::move(content);
    base_.reset();
    poolContent_ = std::move(pool);
    baseRecipe_ = std::move(recipe);
    baseRevision_ = nextBaseRevision();
}

LayerBase Layer::releaseBase() {
    LayerBase released;
    if (base_) {
        released.stream = std::move(base_);
    } else if (content_) {
        released.stream = std::make_shared<const sound_mind::codec::StreamImage>(std::move(*content_));
    }
    if (poolContent_) {
        released.pool = std::make_shared<const sound_mind::codec::PoolImage>(std::move(*poolContent_));
    }
    released.recipe = std::move(baseRecipe_);
    released.revision = baseRevision_;
    base_.reset();
    content_.reset();
    poolContent_.reset();
    baseRecipe_ = nlohmann::json();
    return released;
}

void Layer::restoreBase(const LayerBase& base) {
    base_ = base.stream;
    if (base.stream) {
        content_ = *base.stream;
    } else {
        content_.reset();
    }
    if (base.pool) {
        poolContent_ = *base.pool;
    } else {
        poolContent_.reset();
    }
    baseRecipe_ = base.recipe;
    // A base remade by replaying the history comes without a revision of
    // its own: it's a new one as far as anything caching on it knows.
    baseRevision_ = base.revision != 0 || !base.stream ? base.revision : nextBaseRevision();
}

void Layer::assignProperties(const Layer& other) {
    id_ = other.id_;
    name_ = other.name_;
    type_ = other.type_;
    opacity_ = other.opacity_;
    opacityMindWave_ = other.opacityMindWave_;
    balance_ = other.balance_;
    recorded_ = other.recorded_;
    visible_ = other.visible_;
    muted_ = other.muted_;
    translationColumns_ = other.translationColumns_;
    rescaleFactor_ = other.rescaleFactor_;
    filterConfiguration_ = other.filterConfiguration_;
    blendMode_ = other.blendMode_;
}

std::size_t LayerBase::byteSize() const noexcept {
    std::size_t bytes = 0;
    if (stream) {
        bytes += (stream->leftMagnitudeDb.size() + stream->rightMagnitudeDb.size() +
                  stream->sharedPhaseRadians.size()) *
                 sizeof(float);
    }
    if (pool) {
        bytes += (pool->leftMagnitudeDb.size() + pool->rightMagnitudeDb.size()) * sizeof(float) * 2;
    }
    return bytes;
}

}  // namespace sound_mind::core
