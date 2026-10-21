#include "sound_mind/studio/remaster.h"

#include <QCoreApplication>

#include "sound_mind/core/history_replay.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paste_operation.h"
#include "sound_mind/core/remaster.h"
#include "sound_mind/core/text_paste.h"
#include "sound_mind/studio/base_recipes.h"
#include "sound_mind/studio/text_controller.h"

namespace sound_mind::studio {

namespace {

QString tr(const char* text) { return QCoreApplication::translate("Remaster", text); }

std::optional<sound_mind::core::Project> fail(QString* errorMessage, const QString& reason) {
    if (errorMessage != nullptr) {
        *errorMessage = reason;
    }
    return std::nullopt;
}

void remapSource(nlohmann::json& layerJson, const std::map<sound_mind::core::SourceId, sound_mind::core::SourceId>& ids) {
    if (!layerJson.is_object() || !layerJson.contains("baseRecipe")) {
        return;
    }
    nlohmann::json& recipe = layerJson["baseRecipe"];
    if (recipe.is_object() && recipe.contains("source")) {
        const auto it = ids.find(recipe["source"].get<sound_mind::core::SourceId>());
        if (it != ids.end()) {
            recipe["source"] = it->second;
        }
    }
}

/// Re-renders a text paste's clip from its text at the new resolution - sharper
/// than resampling the old raster.
void rerenderText(nlohmann::json& operation, const sound_mind::codec::StreamCodecConfig& config) {
    if (operation.value("kind", "") != "paste" || !operation.contains("textSource")) {
        return;
    }
    const auto parsed = sound_mind::core::OperationLog::operationFromJson(operation);
    const auto* paste = dynamic_cast<const sound_mind::core::PasteOperation*>(parsed.get());
    if (paste == nullptr || !paste->textSource().has_value()) {
        return;
    }
    const sound_mind::core::TextSource& source = *paste->textSource();
    const sound_mind::core::TextPasteBox box = sound_mind::core::textPasteBox(*paste, config);
    const auto raster =
        TextController::rasterize(TextController::settingsFor(source, paste->blendMode()), box.width, box.height);
    auto remade = sound_mind::core::makeTextPaste(paste->id(), *paste->targetLayer(), raster, box.frameLow,
                                                  box.binLow, source.leftIntensityDb, source.rightIntensityDb,
                                                  source.opacity, paste->blendMode(), config, source,
                                                  paste->supersedes());
    if (remade != nullptr) {
        nlohmann::json json = sound_mind::core::OperationLog::operationToJson(*remade);
        if (operation.contains("boundary")) {
            json["boundary"] = operation["boundary"];
        }
        operation = std::move(json);
    }
}

}  // namespace

std::optional<RemasterInput> remasterInputFrom(const sound_mind::core::Project& project) {
    const auto folder = project.sourcesFolder();
    if (!project.history().startsAtCreation() || !folder.has_value()) {
        return std::nullopt;
    }
    RemasterInput input;
    input.originalSettings = project.settings();
    for (std::size_t i = 0; i < project.history().position(); ++i) {
        input.entries.push_back(project.history().entry(i));
    }
    input.sources = project.sources();
    input.sourcesFolder = *folder;
    return input;
}

std::optional<sound_mind::core::Project> remasterProject(const RemasterInput& input, const RemasterRequest& request,
                                                        const std::function<bool()>& shouldCancel,
                                                        const std::function<void(std::size_t, std::size_t)>& progress,
                                                        QString* errorMessage) {
    const sound_mind::core::ProjectSettings settings =
        sound_mind::core::remasterSettings(input.originalSettings, request.settings);
    const auto config = sound_mind::core::streamCodecConfigFor(settings);

    sound_mind::core::ReplayOptions options;
    options.useKeptBases = false;
    sound_mind::core::Project* replayed = nullptr;
    options.runRecipe = replayRecipeRunner([&replayed](sound_mind::core::SourceId id) {
        return replayed != nullptr ? replayed->sourcePath(id) : std::nullopt;
    });
    sound_mind::core::HistoryReplayer replayer(settings, std::move(options));
    replayed = &replayer.project();

    // 1. The sources, in the new project's folder.
    std::map<sound_mind::core::SourceId, sound_mind::core::SourceId> replacedIds;
    try {
        replayed->setFolderForProjectFile(request.outputPath);
        const std::filesystem::path sourcesFolder = *replayed->sourcesFolder();
        std::filesystem::create_directories(sourcesFolder);
        for (const sound_mind::core::ProjectSource& source : input.sources) {
            const std::filesystem::path name(std::u8string(source.fileName.begin(), source.fileName.end()));
            if (std::filesystem::exists(input.sourcesFolder / name) && !std::filesystem::exists(sourcesFolder / name)) {
                std::filesystem::copy_file(input.sourcesFolder / name, sourcesFolder / name);
            }
            // Registered in the same order, so ids normally match; map any
            // that don't so recipes still find their file.
            const sound_mind::core::SourceId id = replayed->addSource(source);
            if (id != source.id) {
                replacedIds[source.id] = id;
            }
        }
        for (const auto& [id, path] : request.replacements) {
            const sound_mind::core::ProjectSource* original = replayed->sourceById(id);
            const std::string kind = original != nullptr ? original->kind : "audio";
            replacedIds[id] = replayed->addSource(sound_mind::core::storeSourceFile(sourcesFolder, path, kind));
        }
    } catch (const std::exception& e) {
        return fail(errorMessage, tr("Couldn't copy the sources: %1").arg(QString::fromStdString(e.what())));
    }

    // 2-3. Adapt every entry, then replay it.
    std::vector<sound_mind::core::HistoryEntry> adapted;
    adapted.reserve(input.entries.size());
    for (std::size_t i = 0; i < input.entries.size(); ++i) {
        if (shouldCancel && shouldCancel()) {
            return fail(errorMessage, tr("Cancelled."));
        }
        sound_mind::core::HistoryEntry entry = input.entries[i];
        sound_mind::core::adaptEntryForRemaster(entry, input.originalSettings, settings);
        for (sound_mind::core::HistoryLayerChange& change : entry.layers) {
            remapSource(change.before, replacedIds);
            remapSource(change.after, replacedIds);
        }
        for (nlohmann::json& operation : entry.appendedOperations) {
            rerenderText(operation, config);
        }
        std::string failure;
        if (!replayer.apply(entry, &failure)) {
            return fail(errorMessage, tr("\"%1\" couldn't be rebuilt: %2")
                                          .arg(QString::fromStdString(entry.description), QString::fromStdString(failure)));
        }
        adapted.push_back(std::move(entry));
        if (progress) {
            progress(i + 1, input.entries.size());
        }
    }

    // 4. Its own, complete history; saved.
    sound_mind::core::Project result = replayer.takeProject();
    result.history().assignEntries(result, std::move(adapted), /*startsAtCreation=*/true);
    try {
        result.save(request.outputPath);
    } catch (const std::exception& e) {
        return fail(errorMessage, tr("Couldn't save the remastered project: %1").arg(QString::fromStdString(e.what())));
    }
    return result;
}

}  // namespace sound_mind::studio
