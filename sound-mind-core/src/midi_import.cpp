#include "sound_mind/core/midi_import.h"

#include <algorithm>
#include <array>

#include <juce_audio_basics/juce_audio_basics.h>

namespace sound_mind::core {

namespace {

constexpr int kChannelCount = 16;

}  // namespace

std::vector<MidiChannelNotes> parseMidiFile(const std::filesystem::path& path, std::string* errorMessage) {
    juce::File file(juce::String(path.string()));
    juce::FileInputStream stream(file);
    if (!stream.openedOk()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not open MIDI file: " + path.string();
        }
        return {};
    }

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Not a valid MIDI file: " + path.string();
        }
        return {};
    }
    midiFile.convertTimestampTicksToSeconds();

    std::array<int, kChannelCount> programNumber{};
    std::array<bool, kChannelCount> programFixed{};
    std::array<std::vector<NoteEvent>, kChannelCount> notesByChannel;

    for (int trackIndex = 0; trackIndex < midiFile.getNumTracks(); ++trackIndex) {
        const juce::MidiMessageSequence* track = midiFile.getTrack(trackIndex);
        if (track == nullptr) {
            continue;
        }

        for (const auto* holder : *track) {
            const juce::MidiMessage& message = holder->message;
            const int channelIndex = message.getChannel() - 1;
            if (channelIndex < 0 || channelIndex >= kChannelCount) {
                continue;  // Channel 0 (no channel set) or malformed - nothing meaningful to attach it to.
            }

            if (message.isProgramChange()) {
                if (!programFixed[static_cast<std::size_t>(channelIndex)]) {
                    programNumber[static_cast<std::size_t>(channelIndex)] = message.getProgramChangeNumber();
                }
                continue;
            }

            if (!message.isNoteOn()) {
                continue;
            }

            // See MidiChannelNotes::programNumber's own docs - fixed to
            // whichever Program Change came first, and locked in the
            // instant this channel's first note is seen so a later
            // mid-file Program Change never retroactively changes it.
            programFixed[static_cast<std::size_t>(channelIndex)] = true;

            NoteEvent note;
            note.startTimeSeconds = message.getTimeStamp();
            note.frequencyHz = juce::MidiMessage::getMidiNoteInHertz(message.getNoteNumber());
            if (holder->noteOffObject != nullptr) {
                note.durationSeconds = holder->noteOffObject->message.getTimeStamp() - message.getTimeStamp();
            } else {
                note.durationSeconds = 0.0;
            }
            notesByChannel[static_cast<std::size_t>(channelIndex)].push_back(note);
        }
    }

    std::vector<MidiChannelNotes> result;
    for (int channelIndex = 0; channelIndex < kChannelCount; ++channelIndex) {
        std::vector<NoteEvent>& notes = notesByChannel[static_cast<std::size_t>(channelIndex)];
        if (notes.empty()) {
            continue;
        }
        std::stable_sort(notes.begin(), notes.end(),
                          [](const NoteEvent& a, const NoteEvent& b) { return a.startTimeSeconds < b.startTimeSeconds; });

        MidiChannelNotes channel;
        channel.channelNumber = channelIndex + 1;
        channel.programNumber = programNumber[static_cast<std::size_t>(channelIndex)];
        channel.instrumentName = juce::MidiMessage::getGMInstrumentName(channel.programNumber);
        channel.notes = std::move(notes);
        result.push_back(std::move(channel));
    }
    return result;
}

}  // namespace sound_mind::core
