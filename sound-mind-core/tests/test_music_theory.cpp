#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/music_theory.h"

using sound_mind::core::frequencyForMidiNote;
using sound_mind::core::frequencyForTemperamentStep;
using sound_mind::core::noteNameForFrequency;
using sound_mind::core::octaveNumberForFrequency;
using sound_mind::core::pitchClassesInScale;
using sound_mind::core::PitchClass;
using sound_mind::core::ScaleType;
using sound_mind::core::stepsPerOctave;
using sound_mind::core::stepWithinOctaveForPitchClass;
using sound_mind::core::supportsKeyAndScale;
using sound_mind::core::Temperament;

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

TEST_CASE("octaveNumberForFrequency buckets middle C and the A above it into the same octave",
          "[core][music_theory]") {
    REQUIRE(octaveNumberForFrequency(frequencyForMidiNote(60, 440.0), 440.0) == 4);  // C4
    REQUIRE(octaveNumberForFrequency(440.0, 440.0) == 4);                            // A4
    REQUIRE(octaveNumberForFrequency(frequencyForMidiNote(59, 440.0), 440.0) == 3);  // B3, just below C4
}

TEST_CASE("octaveNumberForFrequency returns 0 for a non-positive frequency or reference",
          "[core][music_theory]") {
    REQUIRE(octaveNumberForFrequency(0.0, 440.0) == 0);
    REQUIRE(octaveNumberForFrequency(440.0, 0.0) == 0);
}

TEST_CASE("pitchClassesInScale rooted at C matches the Major scale's own white-key pattern",
          "[core][music_theory]") {
    const auto pitchClasses = pitchClassesInScale(PitchClass::C, ScaleType::Major);
    REQUIRE(pitchClasses == std::vector<PitchClass>{PitchClass::C, PitchClass::D, PitchClass::E, PitchClass::F,
                                                      PitchClass::G, PitchClass::A, PitchClass::B});
}

TEST_CASE("pitchClassesInScale transposes correctly for a non-C root", "[core][music_theory]") {
    // G Major: G A B C D E F#.
    const auto pitchClasses = pitchClassesInScale(PitchClass::G, ScaleType::Major);
    REQUIRE(pitchClasses == std::vector<PitchClass>{PitchClass::G, PitchClass::A, PitchClass::B, PitchClass::C,
                                                      PitchClass::D, PitchClass::E, PitchClass::FSharp});
}

TEST_CASE("pitchClassesInScale for Chromatic always returns all twelve pitch classes regardless of key",
          "[core][music_theory]") {
    REQUIRE(pitchClassesInScale(PitchClass::FSharp, ScaleType::Chromatic).size() == 12);
}

TEST_CASE("stepsPerOctave matches each equal temperament's own N, and 12 for both named tunings",
          "[core][music_theory]") {
    REQUIRE(stepsPerOctave(Temperament::Equal12) == 12);
    REQUIRE(stepsPerOctave(Temperament::Equal19) == 19);
    REQUIRE(stepsPerOctave(Temperament::Equal24) == 24);
    REQUIRE(stepsPerOctave(Temperament::Equal72) == 72);
    REQUIRE(stepsPerOctave(Temperament::QuarterCommaMeantone) == 12);
    REQUIRE(stepsPerOctave(Temperament::Pythagorean) == 12);
}

TEST_CASE("supportsKeyAndScale is true only for the 12-pitch-class-compatible temperaments",
          "[core][music_theory]") {
    REQUIRE(supportsKeyAndScale(Temperament::Equal12));
    REQUIRE(supportsKeyAndScale(Temperament::Equal24));
    REQUIRE(supportsKeyAndScale(Temperament::QuarterCommaMeantone));
    REQUIRE(supportsKeyAndScale(Temperament::Pythagorean));
    REQUIRE_FALSE(supportsKeyAndScale(Temperament::Equal19));
    REQUIRE_FALSE(supportsKeyAndScale(Temperament::Equal31));
    REQUIRE_FALSE(supportsKeyAndScale(Temperament::Equal53));
}

TEST_CASE("frequencyForTemperamentStep step 0 is always the reference frequency itself",
          "[core][music_theory]") {
    for (const Temperament temperament :
         {Temperament::Equal12, Temperament::Equal19, Temperament::Equal24, Temperament::QuarterCommaMeantone,
          Temperament::Pythagorean}) {
        REQUIRE(frequencyForTemperamentStep(temperament, 0, 440.0) == Catch::Approx(440.0));
    }
}

TEST_CASE("frequencyForTemperamentStep for Equal12 matches frequencyForMidiNote shifted by the A4 anchor",
          "[core][music_theory]") {
    for (int step = -24; step <= 24; ++step) {
        REQUIRE(frequencyForTemperamentStep(Temperament::Equal12, step, 440.0) ==
                Catch::Approx(frequencyForMidiNote(step + 69, 440.0)));
    }
}

TEST_CASE("frequencyForTemperamentStep doubles an octave at stepsPerOctave steps for any equal temperament",
          "[core][music_theory]") {
    for (const Temperament temperament : {Temperament::Equal12, Temperament::Equal19, Temperament::Equal31}) {
        const int n = stepsPerOctave(temperament);
        REQUIRE(frequencyForTemperamentStep(temperament, n, 440.0) == Catch::Approx(880.0));
        REQUIRE(frequencyForTemperamentStep(temperament, -n, 440.0) == Catch::Approx(220.0));
    }
}

TEST_CASE("frequencyForTemperamentStep returns 0 for a non-positive reference", "[core][music_theory]") {
    REQUIRE(frequencyForTemperamentStep(Temperament::Equal12, 5, 0.0) == 0.0);
}

TEST_CASE("frequencyForTemperamentStep repeats every 12 steps for the named historical tunings",
          "[core][music_theory]") {
    for (const Temperament temperament : {Temperament::QuarterCommaMeantone, Temperament::Pythagorean}) {
        const double upOneOctave = frequencyForTemperamentStep(temperament, 12, 440.0);
        const double downOneOctave = frequencyForTemperamentStep(temperament, -12, 440.0);
        REQUIRE(upOneOctave == Catch::Approx(880.0));
        REQUIRE(downOneOctave == Catch::Approx(220.0));
    }
}

TEST_CASE("frequencyForTemperamentStep gives Pythagorean tuning's own pure 3:2 fifth between A and E",
          "[core][music_theory]") {
    // stepWithinOctaveForPitchClass(Pythagorean, E) locates E's own step;
    // a pure fifth above A is exactly referenceHz * 3/2.
    const int eStep = stepWithinOctaveForPitchClass(Temperament::Pythagorean, PitchClass::E);
    const double eFrequency = frequencyForTemperamentStep(Temperament::Pythagorean, eStep, 440.0);
    REQUIRE(eFrequency == Catch::Approx(440.0 * 3.0 / 2.0).margin(0.01));
}

TEST_CASE("frequencyForTemperamentStep gives quarter-comma meantone's own pure 5:4 major third between A and C#",
          "[core][music_theory]") {
    // The whole point of quarter-comma meantone: four tempered fifths up
    // from A lands on a pure major third, not Pythagorean's sharp one.
    const int cSharpStep = stepWithinOctaveForPitchClass(Temperament::QuarterCommaMeantone, PitchClass::CSharp);
    const double cSharpFrequency = frequencyForTemperamentStep(Temperament::QuarterCommaMeantone, cSharpStep, 440.0);
    REQUIRE(cSharpFrequency == Catch::Approx(440.0 * 5.0 / 4.0).margin(0.01));
}

TEST_CASE("stepWithinOctaveForPitchClass maps A to step 0 and doubles for Equal24", "[core][music_theory]") {
    REQUIRE(stepWithinOctaveForPitchClass(Temperament::Equal12, PitchClass::A) == 0);
    REQUIRE(stepWithinOctaveForPitchClass(Temperament::Equal12, PitchClass::C) == 3);
    REQUIRE(stepWithinOctaveForPitchClass(Temperament::Equal24, PitchClass::A) == 0);
    REQUIRE(stepWithinOctaveForPitchClass(Temperament::Equal24, PitchClass::C) == 6);
}
