#include "sound_mind/studio/import_export.h"

#include <algorithm>
#include <cstddef>

#include <QCoreApplication>
#include <QImage>

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/wav_file.h"
#include "sound_mind/core/layer_export.h"
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

}  // namespace

std::vector<AudioSnippetPickerDialog::RowData> audioSnippetsForFile(const sound_mind::core::Project& project,
                                                                      const std::filesystem::path& path,
                                                                      QString* errorMessage) {
    try {
        const auto audio = sound_mind::codec::readWavFile(path);
        const auto config = sound_mind::core::streamCodecConfigFor(project.settings());
        const auto loopLengthSamples =
            static_cast<std::size_t>(project.settings().canvasWidth) * static_cast<std::size_t>(config.hopLength);
        if (loopLengthSamples == 0 || audio.sampleRateHz == 0) {
            if (errorMessage != nullptr) {
                *errorMessage = tr("The project's own duration is zero - nothing to split against.");
            }
            return {};
        }

        const std::size_t totalSamples = audio.frameCount();
        const std::size_t snippetCount =
            std::max<std::size_t>((totalSamples + loopLengthSamples - 1) / loopLengthSamples, std::size_t{1});

        std::vector<AudioSnippetPickerDialog::RowData> result;
        result.reserve(snippetCount);
        for (std::size_t index = 0; index < snippetCount; ++index) {
            const std::size_t start = index * loopLengthSamples;
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

int importAudioSnippetsInto(sound_mind::core::Project& project, const std::filesystem::path& path,
                             const std::vector<std::size_t>& snippetIndices, QString* errorMessage) {
    try {
        const auto audio = sound_mind::codec::readWavFile(path);
        const auto config = sound_mind::core::streamCodecConfigFor(project.settings());
        const auto loopLengthSamples =
            static_cast<std::size_t>(project.settings().canvasWidth) * static_cast<std::size_t>(config.hopLength);
        const std::size_t totalSamples = audio.frameCount();
        const std::size_t snippetCount =
            loopLengthSamples > 0
                ? std::max<std::size_t>((totalSamples + loopLengthSamples - 1) / loopLengthSamples, std::size_t{1})
                : 1;

        // Sorted, de-duplicated so layers land in the project in ascending
        // snippet order regardless of the order the caller listed indices
        // in - a snippet picker's checked order needn't match position
        // order.
        std::vector<std::size_t> sortedIndices = snippetIndices;
        std::sort(sortedIndices.begin(), sortedIndices.end());
        sortedIndices.erase(std::unique(sortedIndices.begin(), sortedIndices.end()), sortedIndices.end());

        const std::string stem = path.stem().string();
        int importedCount = 0;
        for (const std::size_t index : sortedIndices) {
            if (index >= snippetCount) {
                continue;  // silently skipped - see this function's own docs.
            }
            const std::size_t start = loopLengthSamples > 0 ? index * loopLengthSamples : 0;
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
            layer.setContent(content);
            project.addLayer(std::move(layer));
            ++importedCount;
        }

        if (importedCount == 0 && errorMessage != nullptr) {
            *errorMessage = tr("No snippets were imported.");
        }
        return importedCount;
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return 0;
    }
}

bool importImageFileInto(sound_mind::core::Project& project, const std::filesystem::path& path,
                          ImageScalePickerDialog::Mode mode, QString* errorMessage) {
    const QImage sourceImage(QString::fromStdString(path.string()));
    if (sourceImage.isNull()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Could not load the image file.");
        }
        return false;
    }

    try {
        const auto& settings = project.settings();
        const QImage scaledImage = scaleImageForImport(sourceImage, mode, static_cast<int>(settings.canvasWidth),
                                                         static_cast<int>(settings.canvasHeight));
        const auto rgbImage = toRgbImage(scaledImage);
        const auto content = sound_mind::codec::fromRgbImage(rgbImage, sound_mind::core::streamCodecConfigFor(settings));

        sound_mind::core::Layer layer(0, path.filename().string(), sound_mind::core::LayerType::Normal);
        layer.setContent(content);
        project.addLayer(std::move(layer));
        return true;
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
}

int importImageFilesInto(sound_mind::core::Project& project, const std::vector<std::filesystem::path>& paths,
                          ImageScalePickerDialog::Mode mode, bool importAsSequence, QString* errorMessage) {
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
        QString thisError;
        if (!importImageFileInto(project, path, fileMode, &thisError)) {
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
            sound_mind::core::Layer& justImported = project.layers().back();
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

bool exportLayerAudioNow(const sound_mind::core::Layer& layer, const std::filesystem::path& path,
                          QString* errorMessage) {
    const auto format = audioFormatFromExtension(path);
    if (!format.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Unrecognized audio file extension - use .flac, .ogg, or .mp3.");
        }
        return false;
    }

    try {
        return sound_mind::core::exportLayerAudio(layer, path, *format);
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
}

bool exportLayerVideoNow(const sound_mind::core::Layer& layer, const std::filesystem::path& path,
                          std::uint32_t canvasWidth, QString* errorMessage) {
    try {
        return sound_mind::core::exportLayerVideo(layer, path, canvasWidth);
    } catch (const std::exception& e) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromStdString(e.what());
        }
        return false;
    }
}

}  // namespace sound_mind::studio
