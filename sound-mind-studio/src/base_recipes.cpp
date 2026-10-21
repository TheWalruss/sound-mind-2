#include "sound_mind/studio/base_recipes.h"

#include <memory>

#include <QCoreApplication>

#include "sound_mind/codec/audio_file.h"
#include "sound_mind/codec/stream_file.h"
#include "sound_mind/core/generator.h"
#include "sound_mind/core/phase_cleanup.h"
#include "sound_mind/core/pooling.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/import_export.h"

namespace sound_mind::studio {

namespace {

QString tr(const char* text) { return QCoreApplication::translate("BaseRecipes", text); }

std::optional<sound_mind::core::LayerBase> fail(QString* errorMessage, const QString& reason) {
    if (errorMessage != nullptr) {
        *errorMessage = reason;
    }
    return std::nullopt;
}

sound_mind::core::LayerBase baseOf(sound_mind::codec::StreamImage image, const nlohmann::json& recipe) {
    return sound_mind::core::LayerBase{std::make_shared<const sound_mind::codec::StreamImage>(std::move(image)),
                                       nullptr, recipe, 0};
}

bool sameCodec(const sound_mind::codec::StreamCodecConfig& a, const sound_mind::codec::StreamCodecConfig& b) {
    return a.sampleRateHz == b.sampleRateHz && a.hopLength == b.hopLength && a.binCount == b.binCount &&
           a.minFrequencyHz == b.minFrequencyHz && a.maxFrequencyHz == b.maxFrequencyHz;
}

}  // namespace

std::optional<sound_mind::core::LayerBase> runBaseRecipe(const nlohmann::json& recipe, sound_mind::core::LayerId layer,
                                                        const sound_mind::core::Project& project,
                                                        const SourcePathLookup& sourcePath,
                                                        const RenderedContentLookup& renderedContent,
                                                        QString* errorMessage) {
    if (!recipe.is_object() || !recipe.contains("kind")) {
        return fail(errorMessage, tr("This layer's content has no record of how it was made."));
    }
    const std::string kind = recipe.at("kind").get<std::string>();
    const sound_mind::core::ProjectSettings& settings = project.settings();
    const auto config = sound_mind::core::streamCodecConfigFor(settings);

    const auto source = [&]() -> std::optional<std::filesystem::path> {
        if (!recipe.contains("source") || !sourcePath) {
            return std::nullopt;
        }
        auto path = sourcePath(recipe.at("source").get<sound_mind::core::SourceId>());
        if (path.has_value() && !std::filesystem::exists(*path)) {
            return std::nullopt;
        }
        return path;
    };
    const auto rendered = [&](sound_mind::core::LayerId id) -> const sound_mind::codec::StreamImage* {
        return renderedContent ? renderedContent(id) : nullptr;
    };

    try {
        if (kind == "empty") {
            return baseOf(sound_mind::core::silentContentFor(settings), recipe);
        }
        if (kind == "importAudio") {
            const auto path = source();
            if (!path) {
                return fail(errorMessage, tr("The imported audio file is missing from the project's sources."));
            }
            QString error;
            auto layers = encodeAudioSnippets(settings, *path, {recipe.value("snippetIndex", std::size_t{0})}, nullptr,
                                              &error, recipe.value("offsetSeconds", 0.0));
            if (layers.empty() || layers.front().baseContent() == nullptr) {
                return fail(errorMessage, error.isEmpty() ? tr("The audio file couldn't be imported again.") : error);
            }
            return baseOf(*layers.front().baseContent(), recipe);
        }
        if (kind == "importImage") {
            const auto path = source();
            if (!path) {
                return fail(errorMessage, tr("The imported image file is missing from the project's sources."));
            }
            std::optional<PolarImportParams> polar;
            if (recipe.contains("polar")) {
                const auto& p = recipe.at("polar");
                polar = PolarImportParams{p.value("originX", 0.0),         p.value("originY", 0.0),
                                          p.value("radius", 0.0),          p.value("arcStartRadians", 0.0),
                                          p.value("arcEndRadians", 0.0),   p.value("outputWidth", std::uint32_t{0})};
            }
            QString error;
            auto image = encodeImageFile(settings, *path,
                                         static_cast<ImageScalePickerDialog::Mode>(recipe.value("mode", 0)), polar,
                                         &error);
            if (!image) {
                return fail(errorMessage, error);
            }
            return baseOf(std::move(*image), recipe);
        }
        if (kind == "generate") {
            const auto generator = recipe.value("configuration", nlohmann::json::object())
                                       .get<sound_mind::core::GeneratorConfiguration>();
            return baseOf(sound_mind::core::generateContent(
                              generator, settings,
                              [&project](sound_mind::core::MindWaveId id) -> const sound_mind::core::MindWave* {
                                  const auto* entry = project.mindWaveById(id);
                                  return entry != nullptr ? &entry->wave : nullptr;
                              }),
                          recipe);
        }
        if (kind == "record" || kind == "loopTake") {
            const auto path = source();
            if (!path) {
                return fail(errorMessage, tr("The recorded audio is missing from the project's sources."));
            }
            return baseOf(sound_mind::codec::encode(sound_mind::codec::readAudioFile(*path), config), recipe);
        }
        if (kind == "layerImage") {
            const auto path = source();
            if (!path) {
                return fail(errorMessage, tr("The imported layer is missing from the project's sources."));
            }
            sound_mind::codec::StreamImage image = sound_mind::codec::readStreamFile(*path);
            if (!sameCodec(image.config, config)) {
                image = sound_mind::codec::encode(sound_mind::codec::decode(image), config);
            }
            return baseOf(std::move(image), recipe);
        }
        if (kind == "duplicate") {
            const auto* from = rendered(recipe.value("sourceLayer", sound_mind::core::LayerId{0}));
            if (from == nullptr) {
                return fail(errorMessage, tr("The duplicated layer's original is missing."));
            }
            return baseOf(*from, recipe);
        }
        if (kind == "pool") {
            const auto* from = rendered(layer);
            if (from == nullptr) {
                return fail(errorMessage, tr("The pooled layer had no content."));
            }
            auto pooled = sound_mind::core::computePooledContent(*from);
            sound_mind::core::LayerBase base = baseOf(std::move(pooled.streamContent), recipe);
            base.pool = std::make_shared<const sound_mind::codec::PoolImage>(std::move(pooled.poolImage));
            return base;
        }
        if (kind == "phaseCleanup") {
            const auto* from = rendered(layer);
            if (from == nullptr) {
                return fail(errorMessage, tr("The cleaned-up layer had no content."));
            }
            sound_mind::codec::StreamImage image = *from;
            sound_mind::core::applyPhaseCleanup(image);
            return baseOf(std::move(image), recipe);
        }
    } catch (const std::exception& e) {
        return fail(errorMessage, QString::fromStdString(e.what()));
    }
    return fail(errorMessage, tr("This layer's content was made in a way that can't be repeated."));
}

}  // namespace sound_mind::studio
