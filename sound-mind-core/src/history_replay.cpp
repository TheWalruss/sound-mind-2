#include "sound_mind/core/history_replay.h"

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

HistoryReplayer::HistoryReplayer(const ProjectSettings& settings, ReplayOptions options)
    : project_(Project::createNew(settings)), options_(std::move(options)) {}

bool HistoryReplayer::apply(const HistoryEntry& original, std::string* error) {
    HistoryEntry entry = original;
    if (!options_.useKeptBases) {
        for (HistoryLayerChange& change : entry.layers) {
            change.baseBefore.reset();
            change.baseAfter.reset();
        }
    }
    if (options_.transformEntry) {
        options_.transformEntry(entry);
    }

    std::string failure;
    const auto supplyBase = [&](LayerId layer) -> std::optional<LayerBase> {
        for (const HistoryLayerChange& change : entry.layers) {
            if (change.id != layer) {
                continue;
            }
            const nlohmann::json recipe = change.after.value("baseRecipe", nlohmann::json());
            if (!options_.runRecipe) {
                failure = "No way to remake layer content from its recipe.";
                return std::nullopt;
            }
            auto base = options_.runRecipe(
                recipe, layer, project_, [this](LayerId id) { return rendered(id); }, &failure);
            if (base.has_value()) {
                base->recipe = recipe;
            }
            return base;
        }
        return std::nullopt;
    };
    const auto change = History::applyEntry(project_, entry, supplyBase);
    if (!change.has_value()) {
        if (error != nullptr) {
            *error = failure.empty() ? "A layer's content couldn't be remade." : failure;
        }
        return false;
    }
    stale_.insert(change->contentLayers.begin(), change->contentLayers.end());
    std::erase_if(stale_, [this](LayerId id) { return project_.layerById(id) == nullptr; });
    return true;
}

bool HistoryReplayer::applyAll(const History& history, std::size_t count, std::string* error) {
    count = std::min(count, history.size());
    for (std::size_t i = 0; i < count; ++i) {
        if (options_.shouldCancel && options_.shouldCancel()) {
            if (error != nullptr) {
                *error = "Cancelled.";
            }
            return false;
        }
        std::string failure;
        if (!apply(history.entry(i), &failure)) {
            if (error != nullptr) {
                *error = "\"" + history.entry(i).description + "\": " + failure;
            }
            return false;
        }
        if (options_.progress) {
            options_.progress(i + 1, count);
        }
    }
    return true;
}

const sound_mind::codec::StreamImage* HistoryReplayer::rendered(LayerId layer) {
    std::set<LayerId> visiting;
    render(layer, visiting);
    const Layer* found = project_.layerById(layer);
    return found != nullptr && found->content().has_value() ? &*found->content() : nullptr;
}

void HistoryReplayer::render(LayerId layer, std::set<LayerId>& visiting) {
    if (!stale_.contains(layer) || visiting.contains(layer)) {
        return;
    }
    visiting.insert(layer);
    Layer* target = project_.layerById(layer);
    if (target == nullptr) {
        stale_.erase(layer);
        return;
    }
    const auto operations = project_.operationLog().activeOperationsTargeting(layer);
    if (!operations.empty()) {
        const ProjectSettings settings = project_.settings();
        const sound_mind::codec::StreamImage silent = silentContentFor(settings);
        const sound_mind::codec::StreamImage& base =
            target->baseContent() != nullptr ? *target->baseContent() : silent;
        // A Mind Grain stroke reads another layer's content: render it first.
        const auto resolveLayer = [this, &visiting](LayerId id) -> const sound_mind::codec::StreamImage* {
            render(id, visiting);
            const Layer* source = project_.layerById(id);
            return source != nullptr && source->content().has_value() ? &*source->content() : nullptr;
        };
        const auto resolveMindWave = [this](MindWaveId id) -> const MindWave* {
            const NamedMindWave* named = project_.mindWaveById(id);
            return named != nullptr ? &named->wave : nullptr;
        };
        sound_mind::codec::StreamImage result = rebuildPaintedContent(
            base, operations, frequencyToTimeScaleFor(settings), resolveLayer, resolveMindWave, &settings);
        project_.layerById(layer)->setContent(std::move(result));
    } else if (target->baseContent() != nullptr && target->content().has_value() &&
               target->baseContent() != &*target->content()) {
        // Every operation was undone or baked: the content is the base again.
        target->setContent(*target->baseContent());
    }
    stale_.erase(layer);
}

void HistoryReplayer::renderAll() {
    while (!stale_.empty()) {
        (void)rendered(*stale_.begin());
    }
}

Project HistoryReplayer::takeProject() {
    renderAll();
    project_.history().setBaseline(project_);
    return std::move(project_);
}

}  // namespace sound_mind::core
