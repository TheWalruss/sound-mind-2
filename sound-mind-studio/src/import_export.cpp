#include "sound_mind/studio/import_export.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <QCoreApplication>
#include <QImage>

#include "sound_mind/codec/audio_file.h"
#include "sound_mind/codec/audio_resample.h"
#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/codec/polar_projection.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/project_export.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/import_helpers.h"
#include "sound_mind/studio/qt_image_conversion.h"

namespace sound_mind::studio {

namespace {

/// @brief translate() shorthand matching MainWindow's own `tr()` calls -
///        these are free functions, so there's no QObject to hang a real
///        `tr()` off; QCoreApplication::translate() with this file's own
///        context name is the direct equivalent.
QString tr(const char* text) {
    return QCoreApplication::translate("ImportExport", text);
}

/// @brief Reads an audio file and converts it to `sampleRateHz` - the
///        project's own (`v0.1.49.3`), so the snippet split below, which
///        counts samples, is measured at the project's real duration.
sound_mind::codec::AudioBuffer readAudioFileAt(const std::filesystem::path& path, std::uint32_t sampleRateHz) {
    return sound_mind::codec::resample(sound_mind::codec::readAudioFile(path), sampleRateHz);
}

}  // namespace

std::vector<AudioSnippetPickerDialog::RowData> audioSnippetsForFile(const sound_mind::core::Project& project,
                                                                      const std::filesystem::path& path,
                                                                      QString* errorMessage, double offsetSeconds) {
    try {
        const auto config = sound_mind::core::streamCodecConfigFor(project.settings());
        const auto audio = readAudioFileAt(path, config.sampleRateHz);
        const auto loopLengthSamples =
            static_cast<std::size_t>(project.settings().canvasWidth) * static_cast<std::size_t>(config.hopLength);
        if (loopLengthSamples == 0 || audio.sampleRateHz == 0) {
            if (errorMessage != nullptr) {
                *errorMessage = tr("The project's own duration is zero - nothing to split against.");
            }
            return {};
        }

        const std::size_t totalSamples = audio.frameCount();
        // Finding #23: offsetSamples worth of audio is discarded outright
        // before the split grid is even considered - see this function's
        // own docs on why that's a real restart, not just a shorter
        // snippet 0.
        const auto offsetSamples =
            static_cast<std::size_t>(std::llround(offsetSeconds * static_cast<double>(audio.sampleRateHz)));
        const std::size_t effectiveTotalSamples = totalSamples > offsetSamples ? totalSamples - offsetSamples : 0;
        if (effectiveTotalSamples == 0) {
            if (errorMessage != nullptr) {
                *errorMessage = tr("The chosen offset leaves nothing left in the file to split.");
            }
            return {};
        }
        const std::size_t snippetCount =
            std::max<std::size_t>((effectiveTotalSamples + loopLengthSamples - 1) / loopLengthSamples, std::size_t{1});

        std::vector<AudioSnippetPickerDialog::RowData> result;
        result.reserve(snippetCount);
        for (std::size_t index = 0; index < snippetCount; ++index) {
            const std::size_t start = offsetSamples + index * loopLengthSamples;
            const std::size_t end = std::min(start + loopLengthSamples, totalSamples);
            AudioSnippetPickerDialog::RowData row;
            row.index = index;
            row.startSeconds = static_cast<double>(start) / static_cast<double>(audio.sampleRateHz);
            row.endSeconds = static_cast<double>(end) / static_cast<double>(audio.sampleRateHz);
            result.push_back(row);
        }
        return result;
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return {};
    }
}

std::vector<sound_mind::core::Layer> encodeAudioSnippets(const sound_mind::core::ProjectSettings& settings,
                                                           const std::filesystem::path& path,
                                                           const std::vector<std::size_t>& snippetIndices,
                                                           const std::function<bool()>& shouldCancel,
                                                           QString* errorMessage, double offsetSeconds) {
    try {
        const auto config = sound_mind::core::streamCodecConfigFor(settings);
        const auto audio = readAudioFileAt(path, config.sampleRateHz);
        const auto loopLengthSamples =
            static_cast<std::size_t>(settings.canvasWidth) * static_cast<std::size_t>(config.hopLength);
        const std::size_t totalSamples = audio.frameCount();
        // See audioSnippetsForFile()'s own docs - must agree with it exactly,
        // since snippetIndices was computed against that same offset.
        const auto offsetSamples =
            static_cast<std::size_t>(std::llround(offsetSeconds * static_cast<double>(audio.sampleRateHz)));
        const std::size_t effectiveTotalSamples = totalSamples > offsetSamples ? totalSamples - offsetSamples : 0;
        const std::size_t snippetCount =
            loopLengthSamples > 0 && effectiveTotalSamples > 0
                ? std::max<std::size_t>((effectiveTotalSamples + loopLengthSamples - 1) / loopLengthSamples,
                                         std::size_t{1})
                : (effectiveTotalSamples > 0 ? 1 : 0);

        // Sorted, de-duplicated so layers land in ascending snippet order
        // regardless of the order the caller listed indices in - a snippet
        // picker's checked order needn't match position order.
        std::vector<std::size_t> sortedIndices = snippetIndices;
        std::sort(sortedIndices.begin(), sortedIndices.end());
        sortedIndices.erase(std::unique(sortedIndices.begin(), sortedIndices.end()), sortedIndices.end());

        const std::string stem = path.stem().string();
        std::vector<sound_mind::core::Layer> layers;
        for (const std::size_t index : sortedIndices) {
            if (index >= snippetCount) {
                continue;  // silently skipped - see this function's own docs.
            }
            if (shouldCancel && shouldCancel()) {
                throw ImportCancelled{};
            }
            const std::size_t start = loopLengthSamples > 0 ? offsetSamples + index * loopLengthSamples : offsetSamples;
            const std::size_t end =
                loopLengthSamples > 0 ? std::min(start + loopLengthSamples, totalSamples) : totalSamples;
            if (start > end) {
                continue;
            }

            sound_mind::codec::AudioBuffer snippet;
            snippet.sampleRateHz = audio.sampleRateHz;
            snippet.left.assign(audio.left.begin() + static_cast<std::ptrdiff_t>(start),
                                 audio.left.begin() + static_cast<std::ptrdiff_t>(end));
            snippet.right.assign(audio.right.begin() + static_cast<std::ptrdiff_t>(start),
                                  audio.right.begin() + static_cast<std::ptrdiff_t>(end));

            const auto content = sound_mind::codec::encode(snippet, config);
            const std::string layerName =
                snippetCount > 1 ? stem + "_" + formatSnippetIndex(index) : path.filename().string();

            sound_mind::core::Layer layer(0, layerName, sound_mind::core::LayerType::Normal);
            // How to remake it from the file (v0.1.43.1) - see Layer::setBaseContent().
            layer.setBaseContent(content, nlohmann::json{{"kind", "importAudio"},
                                                         {"file", QString::fromStdWString(path.filename().wstring()).toStdString()},
                                                         {"offsetSeconds", offsetSeconds},
                                                         {"snippetIndex", index}});
            layers.push_back(std::move(layer));
        }

        if (layers.empty() && errorMessage != nullptr) {
            *errorMessage = tr("No snippets were imported.");
        }
        return layers;
    } catch (const ImportCancelled&) {
        throw;
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return {};
    }
}

int importAudioSnippetsInto(sound_mind::core::Project& project, const std::filesystem::path& path,
                             const std::vector<std::size_t>& snippetIndices, QString* errorMessage,
                             double offsetSeconds) {
    auto layers = encodeAudioSnippets(project.settings(), path, snippetIndices, nullptr, errorMessage, offsetSeconds);
    std::vector<sound_mind::core::Layer*> made;
    for (auto& layer : layers) {
        made.push_back(&layer);
    }
    attachImportSource(project, path, "audio", made);
    for (auto& layer : layers) {
        project.addLayer(std::move(layer));
    }
    return static_cast<int>(layers.size());
}

bool importImageFileInto(sound_mind::core::Project& project, const std::filesystem::path& path,
                          ImageScalePickerDialog::Mode mode, QString* errorMessage,
                          sound_mind::core::LayerId* outLayerId, std::optional<PolarImportParams> polarParams) {
    const auto content = encodeImageFile(project.settings(), path, mode, polarParams, errorMessage);
    if (!content.has_value()) {
        return false;
    }
    sound_mind::core::Layer layer(0, path.filename().string(), sound_mind::core::LayerType::Normal);
    {
        nlohmann::json recipe{{"kind", "importImage"},
                              {"file", QString::fromStdWString(path.filename().wstring()).toStdString()},
                              {"mode", static_cast<int>(mode)}};
        if (polarParams.has_value()) {
            recipe["polar"] = {{"originX", polarParams->originX},
                               {"originY", polarParams->originY},
                               {"radius", polarParams->radius},
                               {"arcStartRadians", polarParams->arcStartRadians},
                               {"arcEndRadians", polarParams->arcEndRadians},
                               {"outputWidth", polarParams->outputWidth}};
        }
        layer.setBaseContent(*content, std::move(recipe));
    }
    attachImportSource(project, path, "image", {&layer});
    const sound_mind::core::LayerId newId = project.addLayer(std::move(layer));
    if (outLayerId != nullptr) {
        *outLayerId = newId;
    }
    return true;
}

std::optional<sound_mind::codec::StreamImage> encodeImageFile(const sound_mind::core::ProjectSettings& settings,
                                                              const std::filesystem::path& path,
                                                              ImageScalePickerDialog::Mode mode,
                                                              const std::optional<PolarImportParams>& polarParams,
                                                              QString* errorMessage) {
    const QImage sourceImage(QString::fromStdWString(path.wstring()));
    if (sourceImage.isNull()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Could not load the image file.");
        }
        return std::nullopt;
    }
    if (mode == ImageScalePickerDialog::Mode::Polar && !polarParams.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No polar origin was set for this import.");
        }
        return std::nullopt;
    }
    try {
        sound_mind::codec::RgbImage rgbImage;
        if (mode == ImageScalePickerDialog::Mode::Polar) {
            // polarToRect()'s own per-output-pixel sampling already
            // targets the exact final size - see this function's own
            // docs - so no separate scaleImageForImport() pass follows,
            // unlike every other mode below.
            rgbImage = sound_mind::codec::polarToRect(toRgbImage(sourceImage), polarParams->originX,
                                                       polarParams->originY, polarParams->radius,
                                                       polarParams->arcStartRadians, polarParams->arcEndRadians,
                                                       polarParams->outputWidth, settings.canvasHeight);
        } else {
            const QImage scaledImage = scaleImageForImport(sourceImage, mode, static_cast<int>(settings.canvasWidth),
                                                             static_cast<int>(settings.canvasHeight));
            rgbImage = toRgbImage(scaledImage);
        }
        return sound_mind::codec::fromRgbImage(rgbImage, sound_mind::core::streamCodecConfigFor(settings));
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return std::nullopt;
    }
}

std::optional<sound_mind::core::SourceId> attachImportSource(sound_mind::core::Project& project,
                                                             const std::filesystem::path& path, const std::string& kind,
                                                             std::vector<sound_mind::core::Layer*> layers) {
    const auto folder = project.sourcesFolder();
    if (!folder.has_value()) {
        return std::nullopt;
    }
    try {
        const sound_mind::core::SourceId id = project.addSource(sound_mind::core::storeSourceFile(*folder, path, kind));
        for (sound_mind::core::Layer* layer : layers) {
            nlohmann::json recipe = layer->baseRecipe();
            if (recipe.is_object()) {
                recipe["source"] = id;
                layer->setBaseRecipe(std::move(recipe));
            }
        }
        return id;
    } catch (const std::exception&) {
        return std::nullopt;  // the import stands; it just can't be replayed from its original.
    }
}

int importImageFilesInto(sound_mind::core::Project& project, const std::vector<std::filesystem::path>& paths,
                          ImageScalePickerDialog::Mode mode, bool importAsSequence, QString* errorMessage,
                          std::optional<PolarImportParams> polarParams) {
    if (paths.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No files to import.");
        }
        return 0;
    }

    std::vector<std::filesystem::path> orderedPaths = paths;
    if (importAsSequence) {
        // Deterministic - the natural choice for numbered frame sequences
        // (frame001.png, frame002.png, ...) regardless of the file dialog's
        // own selection/return order, confirmed with the user before
        // implementing.
        std::sort(orderedPaths.begin(), orderedPaths.end());
    }

    const auto canvasWidth = static_cast<std::int64_t>(project.settings().canvasWidth);
    std::int64_t cumulativeTranslation = 0;
    int importedCount = 0;
    QString firstError;

    for (const auto& path : orderedPaths) {
        const auto fileMode = importAsSequence ? ImageScalePickerDialog::Mode::ScaleVerticalProportional : mode;
        const auto filePolarParams = importAsSequence ? std::nullopt : polarParams;
        QString thisError;
        sound_mind::core::LayerId newLayerId = 0;
        if (!importImageFileInto(project, path, fileMode, &thisError, &newLayerId, filePolarParams)) {
            if (firstError.isEmpty()) {
                firstError = thisError;
            }
            continue;
        }
        ++importedCount;

        if (importAsSequence) {
            // Wrap back to column 0 once the running total reaches
            // canvasWidth - matches the legacy Studio's own
            // cumulative-offset placement exactly (confirmed with the user
            // before implementing) rather than just letting later layers
            // keep extending past canvasWidth (which renderLayer() would
            // crop anyway, per its own Decision #25 padding/cropping).
            if (canvasWidth > 0 && cumulativeTranslation >= canvasWidth) {
                cumulativeTranslation = 0;
            }
            // Not project.layers().back() - that's the Equalizer layer,
            // if one exists (see importImageFileInto()'s own docs).
            sound_mind::core::Layer* justImportedPtr = project.layerById(newLayerId);
            if (justImportedPtr == nullptr) {
                continue;
            }
            sound_mind::core::Layer& justImported = *justImportedPtr;
            justImported.setTranslationColumns(cumulativeTranslation);
            const std::int64_t thisWidth =
                justImported.content().has_value() ? static_cast<std::int64_t>(justImported.content()->frameCount) : 0;
            cumulativeTranslation += thisWidth;
        }
    }

    if (importedCount == 0 && errorMessage != nullptr) {
        *errorMessage = firstError.isEmpty() ? tr("No files were imported.") : firstError;
    }
    return importedCount;
}

bool exportProjectAudioNow(const sound_mind::core::Project& project, const std::filesystem::path& path,
                           QString* errorMessage) {
    const auto format = audioFormatFromExtension(path);
    if (!format.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Unrecognized audio file extension - use .flac, .ogg, or .mp3.");
        }
        return false;
    }

    try {
        if (!sound_mind::core::exportProjectAudio(project, path, *format)) {
            if (errorMessage != nullptr) {
                *errorMessage = tr("Nothing to export - no visible, unmuted layer has content.");
            }
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
}

bool exportProjectVideoNow(const sound_mind::core::Project& project, const std::filesystem::path& path,
                           QString* errorMessage) {
    try {
        if (!sound_mind::core::exportProjectVideo(project, path)) {
            if (errorMessage != nullptr) {
                *errorMessage = tr("Nothing to export - no visible, unmuted layer has content.");
            }
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
}

}  // namespace sound_mind::studio
