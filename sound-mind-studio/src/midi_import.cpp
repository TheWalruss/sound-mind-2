#include "sound_mind/studio/midi_import.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <QCoreApplication>

#include "sound_mind/core/midi_import.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/sequence_operation.h"
#include "sound_mind/core/tool_configuration.h"

namespace sound_mind::studio {

namespace {

/// @brief translate() shorthand matching import_export.cpp's own identical
///        helper - see that file's own docs for why this exists (a free
///        function has no QObject to hang a real tr() off).
QString tr(const char* text) { return QCoreApplication::translate("MidiImport", text); }

/// @brief A fresh `ProceduralConfiguration` with a genuinely opaque, full-
///        volume default gradient - **not** what
///        `std::make_unique<ProceduralConfiguration>()` alone gives.
///
/// A real bug caught by real-world use, 2026-09-27: `ToolConfiguration`'s
/// own bare default gradient stops are fully *transparent*
/// (`GradientStop::leftOpacity`/`rightOpacity` both default to `0.0f` -
/// see that struct's own docs) - deliberately, per
/// `ToolConfigurationPanel`'s own constructor comment ("the transparent
/// default a bare ToolConfiguration starts with"), which is why that
/// panel explicitly reseeds full opacity on construction before a user
/// ever paints a first stroke. Every other real paint path in this
/// codebase either goes through that already-reseeded panel config
/// (`ChordGeneratorController::stampAt()`, in particular) or hand-builds
/// an explicitly opaque gradient in its own tests - this MIDI import code
/// was the first real path to construct and paint through a bare,
/// never-reseeded `ProceduralConfiguration` directly, so every imported
/// note silently painted at zero opacity: real layers, real
/// `SequenceOperation`s, real content() after rebuild, but every cell
/// exactly as silent as the un-painted base underneath it. Mirrors
/// `ToolConfigurationPanel`'s own reseeding exactly (0 dB intensity on
/// both channels - the loudest a stop can be - full opacity on both).
std::unique_ptr<sound_mind::core::ProceduralConfiguration> makeOpaqueDefaultConfiguration() {
    auto config = std::make_unique<sound_mind::core::ProceduralConfiguration>();
    auto stop = config->defaultGradient().stops().front();
    stop.leftIntensity = 0.0f;
    stop.rightIntensity = 0.0f;
    stop.leftOpacity = 1.0f;
    stop.rightOpacity = 1.0f;
    config->defaultGradient().setStopValues(0, stop);
    config->defaultGradient().setStopValues(1, stop);
    return config;
}

/// @brief The `ToolConfiguration` a note on `programNumber` should paint
///        through - `project`'s own `MidiProgramMapping` for that program,
///        if one exists and its own `toolPresetId` still resolves to a
///        real, saved Tool Preset; a fresh, genuinely opaque default
///        `ProceduralConfiguration` (see makeOpaqueDefaultConfiguration()'s
///        own docs) otherwise - the same graceful "a stale reference
///        degrades to the default, it doesn't fail the import" contract
///        `MidiProgramMapping::toolPresetId`'s own docs describe.
std::unique_ptr<sound_mind::core::ToolConfiguration> configurationForProgram(const sound_mind::core::Project& project,
                                                                              int programNumber) {
    const auto* mapping = project.midiProgramMappingForProgram(programNumber);
    if (mapping != nullptr && mapping->toolPresetId.has_value()) {
        const auto* preset = project.toolPresetById(*mapping->toolPresetId);
        if (preset != nullptr && preset->config != nullptr) {
            return preset->config->clone();
        }
    }
    return makeOpaqueDefaultConfiguration();
}

/// @brief Scales every one of `notes`' own `durationSeconds` by
///        `mapping->durationScale` and shifts every `frequencyHz` by
///        `mapping->pitchOffsetSemitones` (equal temperament) in place - a
///        no-op if `mapping` is `nullptr` (the program has no mapping yet,
///        matching every field's own "unmodified" default).
void applyMidiProgramMappingToNotes(std::vector<sound_mind::core::NoteEvent>& notes,
                                     const sound_mind::core::MidiProgramMapping* mapping) {
    if (mapping == nullptr) {
        return;
    }
    const double pitchRatio = std::pow(2.0, mapping->pitchOffsetSemitones / 12.0);
    for (sound_mind::core::NoteEvent& note : notes) {
        note.durationSeconds *= mapping->durationScale;
        note.frequencyHz *= pitchRatio;
    }
}

/// @brief Appends a SequenceOperation carrying `notes` to `project`'s own
///        OperationLog, targeting `layerId`, through `config` - the one
///        line of real logic importMidiChannelsInto()/
///        importMidiSelectionInto() both need, shared rather than
///        duplicated.
void appendSequenceOperation(sound_mind::core::Project& project, sound_mind::core::LayerId layerId,
                              std::vector<sound_mind::core::NoteEvent> notes,
                              std::unique_ptr<sound_mind::core::ToolConfiguration> config) {
    sound_mind::core::OperationLog& log = project.operationLog();
    const auto operationId = log.reserveId();
    log.append(std::make_unique<sound_mind::core::SequenceOperation>(operationId, layerId, std::move(notes),
                                                                       std::move(config)));
}

/// @brief Computes how `channels`' own combined duration (the latest
///        note-end time across every one of them) splits into `project`'s
///        own duration worth of windows - the same
///        `ceil(effectiveDuration / projectDuration)` grid shape
///        `audioSnippetsForFile()` uses, but entirely in seconds: MIDI has
///        no fixed sample rate of its own to round a snippet boundary
///        through, so `project`'s own duration is computed directly as
///        `canvasWidth * timestepMs / 1000.0` (the same formula
///        `frequencyToTimeScaleFor()` uses internally, not exposed as its
///        own function) rather than via `streamCodecConfigFor()`'s own
///        `hopLength` (which rounds to a whole sample count - a real
///        source of drift here that audio's own sample-indexed grid never
///        has to worry about).
std::vector<AudioSnippetPickerDialog::RowData> midiSnippetWindowsFor(
    const sound_mind::core::Project& project, const std::vector<sound_mind::core::MidiChannelNotes>& channels) {
    const double projectDurationSeconds =
        static_cast<double>(project.settings().canvasWidth) * project.settings().timestepMs / 1000.0;
    if (projectDurationSeconds <= 0.0) {
        return {};
    }

    double fileDurationSeconds = 0.0;
    for (const auto& channel : channels) {
        for (const auto& note : channel.notes) {
            fileDurationSeconds =
                std::max(fileDurationSeconds, note.startTimeSeconds + std::max(note.durationSeconds, 0.0));
        }
    }
    if (fileDurationSeconds <= 0.0) {
        return {};
    }

    const std::size_t snippetCount =
        std::max<std::size_t>(static_cast<std::size_t>(std::ceil(fileDurationSeconds / projectDurationSeconds)), 1);

    std::vector<AudioSnippetPickerDialog::RowData> result;
    result.reserve(snippetCount);
    for (std::size_t index = 0; index < snippetCount; ++index) {
        AudioSnippetPickerDialog::RowData row;
        row.index = index;
        row.startSeconds = static_cast<double>(index) * projectDurationSeconds;
        row.endSeconds = std::min(row.startSeconds + projectDurationSeconds, fileDurationSeconds);
        result.push_back(row);
    }
    return result;
}

/// @brief Keeps only the notes of `notes` whose own `startTimeSeconds`
///        falls within `[windowStart, windowEnd)`, rebased so
///        `windowStart` becomes each kept note's own new time `0` - see
///        importMidiSelectionInto()'s own docs on why a note's full
///        duration is kept even past `windowEnd`.
std::vector<sound_mind::core::NoteEvent> clipAndRebase(const std::vector<sound_mind::core::NoteEvent>& notes,
                                                        double windowStart, double windowEnd) {
    std::vector<sound_mind::core::NoteEvent> result;
    for (const auto& note : notes) {
        if (note.startTimeSeconds >= windowStart && note.startTimeSeconds < windowEnd) {
            sound_mind::core::NoteEvent rebased = note;
            rebased.startTimeSeconds -= windowStart;
            result.push_back(rebased);
        }
    }
    return result;
}

/// @brief `value` left-padded with `'0'` to at least `width` digits, e.g.
///        `zeroPadded(7, 4) == "0007"` - the same "`_NNNN`" snippet-index
///        suffix `importAudioSnippetsInto()`'s own naming already uses.
std::string zeroPadded(std::size_t value, int width) {
    std::string text = std::to_string(value);
    if (static_cast<int>(text.size()) < width) {
        text.insert(text.begin(), static_cast<std::size_t>(width) - text.size(), '0');
    }
    return text;
}

}  // namespace

std::vector<sound_mind::core::LayerId> importMidiChannelsInto(sound_mind::core::Project& project,
                                                                const std::filesystem::path& path,
                                                                bool separateLayerPerChannel, QString* errorMessage,
                                                                const std::vector<int>& channelNumbers) {
    std::string parseError;
    auto parsedChannels = sound_mind::core::parseMidiFile(path, &parseError);
    if (parsedChannels.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage =
                parseError.empty() ? tr("The MIDI file has no notes to import.") : QString::fromStdString(parseError);
        }
        return {};
    }

    std::vector<sound_mind::core::MidiChannelNotes> channels;
    if (channelNumbers.empty()) {
        channels = std::move(parsedChannels);
    } else {
        for (auto& channel : parsedChannels) {
            if (std::find(channelNumbers.begin(), channelNumbers.end(), channel.channelNumber) !=
                channelNumbers.end()) {
                channels.push_back(std::move(channel));
            }
        }
        if (channels.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = tr("No channels were selected.");
            }
            return {};
        }
    }

    const std::string stem = path.stem().string();
    std::vector<sound_mind::core::LayerId> newLayerIds;

    if (separateLayerPerChannel) {
        newLayerIds.reserve(channels.size());
        for (auto& channel : channels) {
            applyMidiProgramMappingToNotes(channel.notes, project.midiProgramMappingForProgram(channel.programNumber));
            auto config = configurationForProgram(project, channel.programNumber);
            const std::string name =
                stem + " - Ch" + std::to_string(channel.channelNumber) + " (" + channel.instrumentName + ")";
            const auto layerId =
                project.addLayer(sound_mind::core::Layer(0, name, sound_mind::core::LayerType::Normal));
            appendSequenceOperation(project, layerId, std::move(channel.notes), std::move(config));
            newLayerIds.push_back(layerId);
        }
    } else {
        const auto layerId = project.addLayer(sound_mind::core::Layer(0, stem, sound_mind::core::LayerType::Normal));
        for (auto& channel : channels) {
            applyMidiProgramMappingToNotes(channel.notes, project.midiProgramMappingForProgram(channel.programNumber));
            auto config = configurationForProgram(project, channel.programNumber);
            appendSequenceOperation(project, layerId, std::move(channel.notes), std::move(config));
        }
        newLayerIds.push_back(layerId);
    }

    return newLayerIds;
}

std::optional<MidiImportPreview> midiImportPreviewForFile(const sound_mind::core::Project& project,
                                                           const std::filesystem::path& path,
                                                           QString* errorMessage) {
    std::string parseError;
    auto channels = sound_mind::core::parseMidiFile(path, &parseError);
    if (channels.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage =
                parseError.empty() ? tr("The MIDI file has no notes to import.") : QString::fromStdString(parseError);
        }
        return std::nullopt;
    }

    auto snippets = midiSnippetWindowsFor(project, channels);
    if (snippets.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("The project's own duration is zero - nothing to split against.");
        }
        return std::nullopt;
    }

    MidiImportPreview preview;
    preview.channels = std::move(channels);
    preview.snippets = std::move(snippets);
    return preview;
}

std::vector<sound_mind::core::LayerId> importMidiSelectionInto(sound_mind::core::Project& project,
                                                                 const std::filesystem::path& path,
                                                                 const std::vector<int>& channelNumbers,
                                                                 const std::vector<std::size_t>& snippetIndices,
                                                                 bool separateLayerPerChannel,
                                                                 QString* errorMessage) {
    std::string parseError;
    auto channels = sound_mind::core::parseMidiFile(path, &parseError);
    if (channels.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage =
                parseError.empty() ? tr("The MIDI file has no notes to import.") : QString::fromStdString(parseError);
        }
        return {};
    }

    std::vector<sound_mind::core::MidiChannelNotes> selectedChannels;
    for (auto& channel : channels) {
        if (std::find(channelNumbers.begin(), channelNumbers.end(), channel.channelNumber) != channelNumbers.end()) {
            selectedChannels.push_back(std::move(channel));
        }
    }
    if (selectedChannels.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No channels were selected.");
        }
        return {};
    }
    for (auto& channel : selectedChannels) {
        applyMidiProgramMappingToNotes(channel.notes, project.midiProgramMappingForProgram(channel.programNumber));
    }

    const auto windows = midiSnippetWindowsFor(project, selectedChannels);
    if (windows.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("The project's own duration is zero - nothing to split against.");
        }
        return {};
    }
    const bool needsSuffix = windows.size() > 1;

    const std::string stem = path.stem().string();
    std::vector<sound_mind::core::LayerId> newLayerIds;

    if (separateLayerPerChannel) {
        for (const auto& channel : selectedChannels) {
            for (std::size_t snippetIndex : snippetIndices) {
                if (snippetIndex >= windows.size()) {
                    continue;
                }
                const auto& window = windows[snippetIndex];
                auto clipped = clipAndRebase(channel.notes, window.startSeconds, window.endSeconds);
                if (clipped.empty()) {
                    continue;
                }
                std::string name =
                    stem + " - Ch" + std::to_string(channel.channelNumber) + " (" + channel.instrumentName + ")";
                if (needsSuffix) {
                    name += "_" + zeroPadded(snippetIndex, 4);
                }
                const auto layerId =
                    project.addLayer(sound_mind::core::Layer(0, name, sound_mind::core::LayerType::Normal));
                appendSequenceOperation(project, layerId, std::move(clipped),
                                        configurationForProgram(project, channel.programNumber));
                newLayerIds.push_back(layerId);
            }
        }
    } else {
        for (std::size_t snippetIndex : snippetIndices) {
            if (snippetIndex >= windows.size()) {
                continue;
            }
            const auto& window = windows[snippetIndex];
            std::optional<sound_mind::core::LayerId> layerId;
            for (const auto& channel : selectedChannels) {
                auto clipped = clipAndRebase(channel.notes, window.startSeconds, window.endSeconds);
                if (clipped.empty()) {
                    continue;
                }
                if (!layerId.has_value()) {
                    std::string name = stem;
                    if (needsSuffix) {
                        name += "_" + zeroPadded(snippetIndex, 4);
                    }
                    layerId = project.addLayer(sound_mind::core::Layer(0, name, sound_mind::core::LayerType::Normal));
                }
                appendSequenceOperation(project, *layerId, std::move(clipped),
                                        configurationForProgram(project, channel.programNumber));
            }
            if (layerId.has_value()) {
                newLayerIds.push_back(*layerId);
            }
        }
    }

    return newLayerIds;
}

}  // namespace sound_mind::studio
