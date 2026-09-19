#include <stdexcept>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/music_theory.h"
#include "sound_mind/core/sequence_notation.h"

using sound_mind::core::frequencyForMidiNote;
using sound_mind::core::NoteEvent;
using sound_mind::core::parseSequenceNotation;
using sound_mind::core::sequenceNotationFor;

namespace {
constexpr double kReferenceHz = 440.0;
constexpr double kBpm = 120.0;  // 0.5s per beat.
}  // namespace

TEST_CASE("parseSequenceNotation parses a single note by name", "[core][sequence_notation]") {
    const auto notes = parseSequenceNotation("A4:0.5", kReferenceHz, kBpm);

    REQUIRE(notes.size() == 1);
    CHECK(notes[0].frequencyHz == Catch::Approx(frequencyForMidiNote(69, kReferenceHz)));
    CHECK(notes[0].durationSeconds == Catch::Approx(0.5));
    CHECK(notes[0].startTimeSeconds == Catch::Approx(0.0));
}

TEST_CASE("parseSequenceNotation parses a single note by raw Hz", "[core][sequence_notation]") {
    const auto notes = parseSequenceNotation("440:1.0", kReferenceHz, kBpm);

    REQUIRE(notes.size() == 1);
    CHECK(notes[0].frequencyHz == Catch::Approx(440.0));
}

TEST_CASE("parseSequenceNotation resolves a sharp note name", "[core][sequence_notation]") {
    const auto notes = parseSequenceNotation("C#5:0.5", kReferenceHz, kBpm);

    REQUIRE(notes.size() == 1);
    CHECK(notes[0].frequencyHz == Catch::Approx(frequencyForMidiNote(73, kReferenceHz)));
}

TEST_CASE("parseSequenceNotation resolves a negative octave note name", "[core][sequence_notation]") {
    const auto notes = parseSequenceNotation("D-1:0.5", kReferenceHz, kBpm);

    REQUIRE(notes.size() == 1);
    // D-1: octaveIndex 0 (since octave = octaveIndex - 1 = -1), nameIndex for D is 2 -> midiNote = 2.
    CHECK(notes[0].frequencyHz == Catch::Approx(frequencyForMidiNote(2, kReferenceHz)));
}

TEST_CASE("parseSequenceNotation chains sequential notes back to back", "[core][sequence_notation]") {
    const auto notes = parseSequenceNotation("A4:0.5 C5:0.25", kReferenceHz, kBpm);

    REQUIRE(notes.size() == 2);
    CHECK(notes[0].startTimeSeconds == Catch::Approx(0.0));
    CHECK(notes[1].startTimeSeconds == Catch::Approx(0.5));
}

TEST_CASE("parseSequenceNotation offsets the whole sequence by startTimeSeconds", "[core][sequence_notation]") {
    const auto notes = parseSequenceNotation("A4:0.5 C5:0.25", kReferenceHz, kBpm, 10.0);

    REQUIRE(notes.size() == 2);
    CHECK(notes[0].startTimeSeconds == Catch::Approx(10.0));
    CHECK(notes[1].startTimeSeconds == Catch::Approx(10.5));
}

TEST_CASE("parseSequenceNotation advances the timeline for a rest without emitting a note",
          "[core][sequence_notation]") {
    const auto notes = parseSequenceNotation("A4:0.5 z0.25 C5:0.25", kReferenceHz, kBpm);

    REQUIRE(notes.size() == 2);
    CHECK(notes[0].startTimeSeconds == Catch::Approx(0.0));
    CHECK(notes[1].startTimeSeconds == Catch::Approx(0.75));  // 0.5 (note) + 0.25 (rest).
}

TEST_CASE("parseSequenceNotation starts every note in a +-joined chord together", "[core][sequence_notation]") {
    const auto notes = parseSequenceNotation("A4:0.5+C#5:0.25+E5:0.75", kReferenceHz, kBpm);

    REQUIRE(notes.size() == 3);
    CHECK(notes[0].startTimeSeconds == Catch::Approx(0.0));
    CHECK(notes[1].startTimeSeconds == Catch::Approx(0.0));
    CHECK(notes[2].startTimeSeconds == Catch::Approx(0.0));
    CHECK(notes[0].durationSeconds == Catch::Approx(0.5));
    CHECK(notes[1].durationSeconds == Catch::Approx(0.25));
    CHECK(notes[2].durationSeconds == Catch::Approx(0.75));
}

TEST_CASE("parseSequenceNotation advances past a chord by its own longest note", "[core][sequence_notation]") {
    const auto notes = parseSequenceNotation("A4:0.5+C#5:0.75 E5:0.25", kReferenceHz, kBpm);

    REQUIRE(notes.size() == 3);
    CHECK(notes[2].startTimeSeconds == Catch::Approx(0.75));  // the chord's own longest note (0.75), not 0.5.
}

TEST_CASE("parseSequenceNotation converts a beats-suffixed duration using bpm", "[core][sequence_notation]") {
    // At 120 bpm, one beat is 0.5s - "1b" should resolve to 0.5s.
    const auto notes = parseSequenceNotation("A4:1b", kReferenceHz, kBpm);

    REQUIRE(notes.size() == 1);
    CHECK(notes[0].durationSeconds == Catch::Approx(0.5));
}

TEST_CASE("parseSequenceNotation of an empty or whitespace-only string returns no notes",
          "[core][sequence_notation]") {
    CHECK(parseSequenceNotation("", kReferenceHz, kBpm).empty());
    CHECK(parseSequenceNotation("   \t  ", kReferenceHz, kBpm).empty());
}

TEST_CASE("parseSequenceNotation throws for an invalid pitch", "[core][sequence_notation]") {
    CHECK_THROWS_AS(parseSequenceNotation("H4:0.5", kReferenceHz, kBpm), std::invalid_argument);
}

TEST_CASE("parseSequenceNotation throws for a missing duration", "[core][sequence_notation]") {
    CHECK_THROWS_AS(parseSequenceNotation("A4", kReferenceHz, kBpm), std::invalid_argument);
}

TEST_CASE("parseSequenceNotation throws for a non-numeric duration", "[core][sequence_notation]") {
    CHECK_THROWS_AS(parseSequenceNotation("A4:abc", kReferenceHz, kBpm), std::invalid_argument);
}

TEST_CASE("parseSequenceNotation throws for a non-positive Hz value", "[core][sequence_notation]") {
    CHECK_THROWS_AS(parseSequenceNotation("0:0.5", kReferenceHz, kBpm), std::invalid_argument);
    CHECK_THROWS_AS(parseSequenceNotation("-440:0.5", kReferenceHz, kBpm), std::invalid_argument);
}

TEST_CASE("sequenceNotationFor of an empty vector is an empty string", "[core][sequence_notation]") {
    CHECK(sequenceNotationFor({}).empty());
}

TEST_CASE("sequenceNotationFor always renders raw Hz, never a note name", "[core][sequence_notation]") {
    const std::vector<NoteEvent> notes = {{0.0, 0.5, frequencyForMidiNote(69, 440.0)}};

    const auto text = sequenceNotationFor(notes);

    CHECK(text.find('A') == std::string::npos);
    CHECK(text.find(':') != std::string::npos);
}

TEST_CASE("sequenceNotationFor groups simultaneous notes with + and round-trips through parseSequenceNotation",
          "[core][sequence_notation]") {
    const std::vector<NoteEvent> original = {
        {0.0, 0.5, 440.0},
        {0.0, 0.25, 550.0},
        {0.5, 0.25, 660.0},
    };

    const auto text = sequenceNotationFor(original);
    const auto roundTripped = parseSequenceNotation(text, kReferenceHz, kBpm);

    REQUIRE(roundTripped.size() == 3);
    for (std::size_t i = 0; i < original.size(); ++i) {
        CHECK(roundTripped[i].startTimeSeconds == Catch::Approx(original[i].startTimeSeconds));
        CHECK(roundTripped[i].durationSeconds == Catch::Approx(original[i].durationSeconds));
        CHECK(roundTripped[i].frequencyHz == Catch::Approx(original[i].frequencyHz));
    }
}

TEST_CASE("sequenceNotationFor inserts a rest for a gap between notes and round-trips it",
          "[core][sequence_notation]") {
    const std::vector<NoteEvent> original = {
        {0.0, 0.5, 440.0},
        {1.0, 0.5, 550.0},  // a 0.5s gap after the first note ends.
    };

    const auto text = sequenceNotationFor(original);
    const auto roundTripped = parseSequenceNotation(text, kReferenceHz, kBpm);

    REQUIRE(roundTripped.size() == 2);
    CHECK(roundTripped[1].startTimeSeconds == Catch::Approx(1.0));
}

TEST_CASE("sequenceNotationFor offsets output so the earliest note need not start at zero",
          "[core][sequence_notation]") {
    const std::vector<NoteEvent> original = {{5.0, 0.5, 440.0}};

    const auto text = sequenceNotationFor(original);
    // Parsed fresh with startTimeSeconds explicitly re-applied reproduces the original.
    const auto roundTripped = parseSequenceNotation(text, kReferenceHz, kBpm, 5.0);

    REQUIRE(roundTripped.size() == 1);
    CHECK(roundTripped[0].startTimeSeconds == Catch::Approx(5.0));
}
