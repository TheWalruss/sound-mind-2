#include "sound_mind/studio/midi_import.h"

#include <memory>
#include <string>
#include <utility>

#include <QCoreApplication>

#include "sound_mind/core/midi_import.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/sequence_operation.h"
#include "sound_mind/core/tool_configuration.h"

namespace sound_mind::studio {

namespace {

/// @brief translate() shorthand matching import_export.cpp's own identical
///        helper - see that file's own docs for why this exists (a free
///        function has no QObject to hang a real tr() off).
QString tr(const char* text) { return QCoreApplication::translate("MidiImport", text); }

}  // namespace

std::vector<sound_mind::core::LayerId> importMidiChannelsInto(sound_mind::core::Project& project,
                                                                const std::filesystem::path& path,
                                                                bool separateLayerPerChannel, QString* errorMessage) {
    std::string parseError;
    auto channels = sound_mind::core::parseMidiFile(path, &parseError);
    if (channels.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage =
                parseError.empty() ? tr("The MIDI file has no notes to import.") : QString::fromStdString(parseError);
        }
        return {};
    }

    const std::string stem = path.stem().string();
    std::vector<sound_mind::core::LayerId> newLayerIds;

    if (separateLayerPerChannel) {
        newLayerIds.reserve(channels.size());
        for (auto& channel : channels) {
            const std::string name =
                stem + " - Ch" + std::to_string(channel.channelNumber) + " (" + channel.instrumentName + ")";
            const auto layerId =
                project.addLayer(sound_mind::core::Layer(0, name, sound_mind::core::LayerType::Normal));

            sound_mind::core::OperationLog& log = project.operationLog();
            const auto operationId = log.reserveId();
            log.append(std::make_unique<sound_mind::core::SequenceOperation>(
                operationId, layerId, std::move(channel.notes),
                std::make_unique<sound_mind::core::ProceduralConfiguration>()));
            newLayerIds.push_back(layerId);
        }
    } else {
        const auto layerId = project.addLayer(sound_mind::core::Layer(0, stem, sound_mind::core::LayerType::Normal));
        for (auto& channel : channels) {
            sound_mind::core::OperationLog& log = project.operationLog();
            const auto operationId = log.reserveId();
            log.append(std::make_unique<sound_mind::core::SequenceOperation>(
                operationId, layerId, std::move(channel.notes),
                std::make_unique<sound_mind::core::ProceduralConfiguration>()));
        }
        newLayerIds.push_back(layerId);
    }

    return newLayerIds;
}

}  // namespace sound_mind::studio
