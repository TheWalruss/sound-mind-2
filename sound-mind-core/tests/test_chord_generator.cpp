#include <algorithm>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/chord_generator.h"
#include "sound_mind/core/music_theory.h"

using sound_mind::core::arpeggioIndexSequence;
using sound_mind::core::ArpeggioOrder;
using sound_mind::core::buildChordNotes;
using sound_mind::core::ChordCategory;
using sound_mind::core::chordsInCategory;
using sound_mind::core::ChordGeneratorParams;
using sound_mind::core::ChordPlaybackMode;
using sound_mind::core::frequencyForMidiNote;

TEST_CASE("chordsInCategory returns every category with the expected chord count", "[core][chord_generator]") {
    REQUIRE(chordsInCategory(ChordCategory::Triads).size() == 7);
    REQUIRE(chordsInCategory(ChordCategory::Sixths).size() == 4);
    REQUIRE(chordsInCategory(ChordCategory::Sevenths).size() == 10);
    REQUIRE(chordsInCategory(ChordCategory::Ninths).size() == 5);
    REQUIRE(chordsInCategory(ChordCategory::ExtendedAdded).size() == 10);
}

TEST_CASE("chordsInCategory's own Major triad has the expected symbol and intervals", "[core][chord_generator]") {
    const auto& major = chordsInCategory(ChordCategory::Triads).at(0);
    REQUIRE(major.name == "Major");
    REQUIRE(major.symbol == "");
    REQUIRE(major.intervals == std::vector<int>{0, 4, 7});
}

TEST_CASE("chordsInCategory's own Minor 7th has the expected symbol and intervals", "[core][chord_generator]") {
    const auto& minor7 = chordsInCategory(ChordCategory::Sevenths).at(1);
    REQUIRE(minor7.name == "Minor 7th");
    REQUIRE(minor7.symbol == "m7");
    REQUIRE(minor7.intervals == std::vector<int>{0, 3, 7, 10});
}

TEST_CASE("arpeggioIndexSequence Ascending is a plain 0..n-1 run", "[core][chord_generator]") {
    REQUIRE(arpeggioIndexSequence(ArpeggioOrder::Ascending, 4, {}, 0) == std::vector<int>{0, 1, 2, 3});
}

TEST_CASE("arpeggioIndexSequence Descending is a plain n-1..0 run", "[core][chord_generator]") {
    REQUIRE(arpeggioIndexSequence(ArpeggioOrder::Descending, 4, {}, 0) == std::vector<int>{3, 2, 1, 0});
}

TEST_CASE("arpeggioIndexSequence UpDown climbs then descends without repeating the two ends",
          "[core][chord_generator]") {
    REQUIRE(arpeggioIndexSequence(ArpeggioOrder::UpDown, 4, {}, 0) == std::vector<int>{0, 1, 2, 3, 2, 1});
}

TEST_CASE("arpeggioIndexSequence DownUp descends then climbs without repeating the two ends",
          "[core][chord_generator]") {
    REQUIRE(arpeggioIndexSequence(ArpeggioOrder::DownUp, 4, {}, 0) == std::vector<int>{3, 2, 1, 0, 1, 2});
}

TEST_CASE("arpeggioIndexSequence Alternating walks inward from both ends", "[core][chord_generator]") {
    REQUIRE(arpeggioIndexSequence(ArpeggioOrder::Alternating, 4, {}, 0) == std::vector<int>{0, 3, 1, 2});
    REQUIRE(arpeggioIndexSequence(ArpeggioOrder::Alternating, 3, {}, 0) == std::vector<int>{0, 2, 1});
}

TEST_CASE("arpeggioIndexSequence OutsideIn and InsideOut match the legacy stable-sort-by-edge-distance formula",
          "[core][chord_generator]") {
    REQUIRE(arpeggioIndexSequence(ArpeggioOrder::OutsideIn, 4, {}, 0) == std::vector<int>{1, 2, 0, 3});
    REQUIRE(arpeggioIndexSequence(ArpeggioOrder::InsideOut, 4, {}, 0) == std::vector<int>{0, 3, 1, 2});
}

TEST_CASE("arpeggioIndexSequence Custom returns the given indices verbatim, or an ascending run if empty",
          "[core][chord_generator]") {
    REQUIRE(arpeggioIndexSequence(ArpeggioOrder::Custom, 4, {2, 0, 0, 1}, 0) == std::vector<int>{2, 0, 0, 1});
    REQUIRE(arpeggioIndexSequence(ArpeggioOrder::Custom, 4, {}, 0) == std::vector<int>{0, 1, 2, 3});
}

TEST_CASE("arpeggioIndexSequence Random is a permutation of 0..n-1 and is reproducible from the same seed",
          "[core][chord_generator]") {
    const auto first = arpeggioIndexSequence(ArpeggioOrder::Random, 5, {}, 12345);
    const auto second = arpeggioIndexSequence(ArpeggioOrder::Random, 5, {}, 12345);
    REQUIRE(first == second);

    std::vector<int> sorted = first;
    std::sort(sorted.begin(), sorted.end());
    REQUIRE(sorted == std::vector<int>{0, 1, 2, 3, 4});

    const auto differentSeed = arpeggioIndexSequence(ArpeggioOrder::Random, 5, {}, 999);
    REQUIRE(differentSeed != first);
}

TEST_CASE("buildChordNotes in Block mode stamps every note at the same start time", "[core][chord_generator]") {
    ChordGeneratorParams params;
    params.rootMidiNote = 60;  // C4
    params.category = ChordCategory::Triads;
    params.chordIndex = 0;  // Major: 0, 4, 7
    params.mode = ChordPlaybackMode::Block;
    params.startTimeSeconds = 2.0;
    params.blockDurationSeconds = 1.5;

    const auto notes = buildChordNotes(params);
    REQUIRE(notes.size() == 3);
    for (const auto& note : notes) {
        REQUIRE(note.startTimeSeconds == Catch::Approx(2.0));
        REQUIRE(note.durationSeconds == Catch::Approx(1.5));
    }
    REQUIRE(notes[0].frequencyHz == Catch::Approx(frequencyForMidiNote(60, 440.0)));
    REQUIRE(notes[1].frequencyHz == Catch::Approx(frequencyForMidiNote(64, 440.0)));
    REQUIRE(notes[2].frequencyHz == Catch::Approx(frequencyForMidiNote(67, 440.0)));
}

TEST_CASE("buildChordNotes in Arpeggio mode steps notes out at the configured tempo/subdivision",
          "[core][chord_generator]") {
    ChordGeneratorParams params;
    params.rootMidiNote = 60;
    params.category = ChordCategory::Triads;
    params.chordIndex = 0;  // Major: 0, 4, 7 (3 notes)
    params.mode = ChordPlaybackMode::Arpeggio;
    params.startTimeSeconds = 0.0;
    params.order = ArpeggioOrder::Ascending;
    params.bpm = 120.0;
    params.stepBeats = 1.0;  // quarter notes -> 0.5s per step at 120bpm
    params.noteDurationFraction = 0.5;
    params.repeats = 1;

    const auto notes = buildChordNotes(params);
    REQUIRE(notes.size() == 3);
    REQUIRE(notes[0].startTimeSeconds == Catch::Approx(0.0));
    REQUIRE(notes[1].startTimeSeconds == Catch::Approx(0.5));
    REQUIRE(notes[2].startTimeSeconds == Catch::Approx(1.0));
    for (const auto& note : notes) {
        REQUIRE(note.durationSeconds == Catch::Approx(0.25));  // 0.5 step * 0.5 fraction
    }
    REQUIRE(notes[0].frequencyHz == Catch::Approx(frequencyForMidiNote(60, 440.0)));
    REQUIRE(notes[1].frequencyHz == Catch::Approx(frequencyForMidiNote(64, 440.0)));
    REQUIRE(notes[2].frequencyHz == Catch::Approx(frequencyForMidiNote(67, 440.0)));
}

TEST_CASE("buildChordNotes in Arpeggio mode repeats the full cycle repeats() times", "[core][chord_generator]") {
    ChordGeneratorParams params;
    params.rootMidiNote = 60;
    params.category = ChordCategory::Triads;
    params.chordIndex = 6;  // Power: 0, 7 (2 notes)
    params.mode = ChordPlaybackMode::Arpeggio;
    params.order = ArpeggioOrder::Ascending;
    params.bpm = 60.0;
    params.stepBeats = 1.0;  // 1s per step at 60bpm
    params.repeats = 2;

    const auto notes = buildChordNotes(params);
    REQUIRE(notes.size() == 4);
    REQUIRE(notes[0].startTimeSeconds == Catch::Approx(0.0));
    REQUIRE(notes[1].startTimeSeconds == Catch::Approx(1.0));
    REQUIRE(notes[2].startTimeSeconds == Catch::Approx(2.0));
    REQUIRE(notes[3].startTimeSeconds == Catch::Approx(3.0));
}

TEST_CASE("buildChordNotes honors referenceHz for a retuned project", "[core][chord_generator]") {
    ChordGeneratorParams params;
    params.rootMidiNote = 69;  // A4
    params.category = ChordCategory::Triads;
    params.chordIndex = 0;
    params.mode = ChordPlaybackMode::Block;
    params.referenceHz = 432.0;

    const auto notes = buildChordNotes(params);
    REQUIRE(notes[0].frequencyHz == Catch::Approx(432.0));
}

TEST_CASE("buildChordNotes returns an empty vector for an out-of-range chordIndex", "[core][chord_generator]") {
    ChordGeneratorParams params;
    params.category = ChordCategory::Triads;
    params.chordIndex = 999;
    REQUIRE(buildChordNotes(params).empty());
}
