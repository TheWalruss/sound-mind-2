#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/music_theory.h"

using sound_mind::core::frequencyForMidiNote;
using sound_mind::core::noteNameForFrequency;

TEST_CASE("noteNameForFrequency names A4 exactly at the tuning reference itself", "[core][music_theory]") {
    REQUIRE(noteNameForFrequency(440.0, 440.0) == "A4");
}

TEST_CASE("noteNameForFrequency names middle C", "[core][music_theory]") {
    REQUIRE(noteNameForFrequency(261.6256, 440.0) == "C4");
}

TEST_CASE("noteNameForFrequency names a note one octave above the reference", "[core][music_theory]") {
    REQUIRE(noteNameForFrequency(880.0, 440.0) == "A5");
}

TEST_CASE("noteNameForFrequency names a note one octave below the reference", "[core][music_theory]") {
    REQUIRE(noteNameForFrequency(220.0, 440.0) == "A3");
}

TEST_CASE("noteNameForFrequency names a sharp note", "[core][music_theory]") {
    REQUIRE(noteNameForFrequency(466.1638, 440.0) == "A#4");
}

TEST_CASE("noteNameForFrequency rounds to the nearest note, not just the one below", "[core][music_theory]") {
    // A few Hz sharp of A4 - still much closer to A4 than to A#4.
    REQUIRE(noteNameForFrequency(442.0, 440.0) == "A4");
}

TEST_CASE("noteNameForFrequency respects a retuned reference - A4 is wherever referenceHz says it is",
          "[core][music_theory]") {
    REQUIRE(noteNameForFrequency(432.0, 432.0) == "A4");
    // A perfect fifth (7 semitones) above a 432 Hz A4 is still an E5,
    // regardless of the reference no longer being 440 Hz.
    REQUIRE(noteNameForFrequency(432.0 * std::pow(2.0, 7.0 / 12.0), 432.0) == "E5");
}

TEST_CASE("noteNameForFrequency returns empty for a non-positive frequency or reference", "[core][music_theory]") {
    REQUIRE(noteNameForFrequency(0.0, 440.0).empty());
    REQUIRE(noteNameForFrequency(-100.0, 440.0).empty());
    REQUIRE(noteNameForFrequency(440.0, 0.0).empty());
    REQUIRE(noteNameForFrequency(440.0, -440.0).empty());
}

TEST_CASE("frequencyForMidiNote returns the reference itself at MIDI note 69 (A4)", "[core][music_theory]") {
    REQUIRE(frequencyForMidiNote(69, 440.0) == Catch::Approx(440.0));
    REQUIRE(frequencyForMidiNote(69, 432.0) == Catch::Approx(432.0));
}

TEST_CASE("frequencyForMidiNote is the exact inverse of noteNameForFrequency's own semitone math",
          "[core][music_theory]") {
    REQUIRE(frequencyForMidiNote(60, 440.0) == Catch::Approx(261.6255653));
    REQUIRE(frequencyForMidiNote(81, 440.0) == Catch::Approx(880.0));
    REQUIRE(frequencyForMidiNote(57, 440.0) == Catch::Approx(220.0));
    REQUIRE(frequencyForMidiNote(70, 440.0) == Catch::Approx(466.1637615));
}

TEST_CASE("frequencyForMidiNote respects a retuned reference", "[core][music_theory]") {
    REQUIRE(frequencyForMidiNote(69, 432.0) == Catch::Approx(432.0));
    REQUIRE(frequencyForMidiNote(76, 432.0) == Catch::Approx(432.0 * std::pow(2.0, 7.0 / 12.0)));
}

TEST_CASE("frequencyForMidiNote round-trips through noteNameForFrequency's own note-name convention",
          "[core][music_theory]") {
    // C4 is MIDI note 60 by the same 69=A4 convention noteNameForFrequency documents.
    REQUIRE(noteNameForFrequency(frequencyForMidiNote(60, 440.0), 440.0) == "C4");
    REQUIRE(noteNameForFrequency(frequencyForMidiNote(69, 440.0), 440.0) == "A4");
}
