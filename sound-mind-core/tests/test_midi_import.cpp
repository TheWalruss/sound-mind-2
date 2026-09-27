#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include <juce_audio_basics/juce_audio_basics.h>

#include "sound_mind/core/midi_import.h"

using sound_mind::core::MidiChannelNotes;
using sound_mind::core::parseMidiFile;

namespace {

/// @brief Writes a small, hand-built two-track Standard MIDI File to a
///        fresh temp path and returns it - the fixture every test below
///        parses back via parseMidiFile(). Track 0 puts a Program Change
///        (Electric Piano 1, GM program 4) and two notes on channel 1;
///        track 1 puts one note on channel 2 with no Program Change at all
///        (so it should default to program 0).
std::filesystem::path writeTestMidiFile() {
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(960);

    // At the MIDI spec's own default tempo (120 BPM, since no explicit Set
    // Tempo meta-event is written here), one quarter note (960 ticks) is
    // exactly 0.5 real seconds - the round numbers this fixture's own
    // expected values below are built from.
    juce::MidiMessageSequence track0;
    track0.addEvent(juce::MidiMessage::programChange(1, 4).withTimeStamp(0.0));
    track0.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)).withTimeStamp(0.0));   // A4
    track0.addEvent(juce::MidiMessage::noteOff(1, 69).withTimeStamp(960.0));
    track0.addEvent(juce::MidiMessage::noteOn(1, 72, static_cast<juce::uint8>(90)).withTimeStamp(1920.0));  // C5
    track0.addEvent(juce::MidiMessage::noteOff(1, 72).withTimeStamp(2880.0));
    track0.updateMatchedPairs();
    midiFile.addTrack(track0);

    juce::MidiMessageSequence track1;
    track1.addEvent(juce::MidiMessage::noteOn(2, 60, static_cast<juce::uint8>(80)).withTimeStamp(480.0));  // C4
    track1.addEvent(juce::MidiMessage::noteOff(2, 60).withTimeStamp(1440.0));
    track1.updateMatchedPairs();
    midiFile.addTrack(track1);

    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-midi-import.mid";
    {
        juce::File file(juce::String(path.string()));
        juce::FileOutputStream stream(file);
        midiFile.writeTo(stream);
    }
    return path;
}

}  // namespace

TEST_CASE("parseMidiFile groups notes by channel with program number and Hz/seconds conversion",
          "[core][midi_import]") {
    const auto path = writeTestMidiFile();

    std::string error;
    const auto channels = parseMidiFile(path, &error);
    std::filesystem::remove(path);

    REQUIRE(error.empty());
    REQUIRE(channels.size() == 2);

    const MidiChannelNotes& channel1 = channels[0];
    CHECK(channel1.channelNumber == 1);
    CHECK(channel1.programNumber == 4);
    CHECK(channel1.instrumentName == "Electric Piano 1");
    REQUIRE(channel1.notes.size() == 2);
    CHECK(channel1.notes[0].startTimeSeconds == Catch::Approx(0.0));
    CHECK(channel1.notes[0].durationSeconds == Catch::Approx(0.5));
    CHECK(channel1.notes[0].frequencyHz == Catch::Approx(440.0));  // A4
    CHECK(channel1.notes[1].startTimeSeconds == Catch::Approx(1.0));
    CHECK(channel1.notes[1].durationSeconds == Catch::Approx(0.5));
    CHECK(channel1.notes[1].frequencyHz == Catch::Approx(523.2511).epsilon(0.001));  // C5

    const MidiChannelNotes& channel2 = channels[1];
    CHECK(channel2.channelNumber == 2);
    CHECK(channel2.programNumber == 0);  // No Program Change on this channel - defaults to GM program 0.
    CHECK(channel2.instrumentName == "Acoustic Grand Piano");
    REQUIRE(channel2.notes.size() == 1);
    CHECK(channel2.notes[0].startTimeSeconds == Catch::Approx(0.25));
    CHECK(channel2.notes[0].durationSeconds == Catch::Approx(0.5));
    CHECK(channel2.notes[0].frequencyHz == Catch::Approx(261.6256).epsilon(0.001));  // C4
}

TEST_CASE("parseMidiFile merges notes from two different tracks on the same channel, sorted by start time",
          "[core][midi_import]") {
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(960);

    // Both tracks target channel 1; track 1's own note starts earlier than
    // track 0's, so a plain track-by-track concatenation would leave the
    // combined result out of order without an explicit final sort.
    juce::MidiMessageSequence track0;
    track0.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)).withTimeStamp(1920.0));
    track0.addEvent(juce::MidiMessage::noteOff(1, 69).withTimeStamp(2880.0));
    track0.updateMatchedPairs();
    midiFile.addTrack(track0);

    juce::MidiMessageSequence track1;
    track1.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(80)).withTimeStamp(0.0));
    track1.addEvent(juce::MidiMessage::noteOff(1, 60).withTimeStamp(960.0));
    track1.updateMatchedPairs();
    midiFile.addTrack(track1);

    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-midi-import-merge.mid";
    {
        juce::File file(juce::String(path.string()));
        juce::FileOutputStream stream(file);
        midiFile.writeTo(stream);
    }

    const auto channels = parseMidiFile(path);
    std::filesystem::remove(path);

    REQUIRE(channels.size() == 1);
    REQUIRE(channels[0].notes.size() == 2);
    CHECK(channels[0].notes[0].startTimeSeconds == Catch::Approx(0.0));
    CHECK(channels[0].notes[1].startTimeSeconds == Catch::Approx(1.0));
}

TEST_CASE("parseMidiFile omits channels with a Program Change but no actual notes", "[core][midi_import]") {
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(960);

    juce::MidiMessageSequence track0;
    track0.addEvent(juce::MidiMessage::programChange(5, 10).withTimeStamp(0.0));  // Channel 5: no notes follow.
    track0.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(80)).withTimeStamp(0.0));
    track0.addEvent(juce::MidiMessage::noteOff(1, 60).withTimeStamp(960.0));
    track0.updateMatchedPairs();
    midiFile.addTrack(track0);

    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-midi-import-empty-channel.mid";
    {
        juce::File file(juce::String(path.string()));
        juce::FileOutputStream stream(file);
        midiFile.writeTo(stream);
    }

    const auto channels = parseMidiFile(path);
    std::filesystem::remove(path);

    REQUIRE(channels.size() == 1);
    CHECK(channels[0].channelNumber == 1);
}

TEST_CASE("parseMidiFile returns empty with an error message for an unreadable file", "[core][midi_import]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-midi-import-does-not-exist.mid";
    std::filesystem::remove(path);

    std::string error;
    const auto channels = parseMidiFile(path, &error);

    CHECK(channels.empty());
    CHECK_FALSE(error.empty());
}

TEST_CASE("parseMidiFile returns empty for a file that isn't valid MIDI at all", "[core][midi_import]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-midi-import-not-midi.mid";
    {
        std::ofstream out(path, std::ios::binary);
        out << "this is not a midi file";
    }

    std::string error;
    const auto channels = parseMidiFile(path, &error);
    std::filesystem::remove(path);

    CHECK(channels.empty());
    CHECK_FALSE(error.empty());
}
