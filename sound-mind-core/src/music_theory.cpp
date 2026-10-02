#include "sound_mind/core/music_theory.h"

#include <array>
#include <cmath>

namespace sound_mind::core {

namespace {

constexpr std::array<const char*, 12> kNoteNames = {"C", "C#", "D",  "D#", "E",  "F",
                                                      "F#", "G",  "G#", "A",  "A#", "B"};

/// @brief `kNoteNames`' own index for pitch class A (`frequencyForMidiNote()`'s
/// `69 = A4` anchor) - every step/pitch-class conversion below re-centers
/// on this, since `frequencyForTemperamentStep()`'s own `step == 0` is
/// the reference pitch (A), not pitch class C.
constexpr int kPitchClassIndexOfA = 9;

/// @brief Pythagorean tuning's own 12 pitch classes, as cents above the
/// reference pitch (A) - derived from a chain of pure 3:2 fifths (701.955
/// cents each): ascending from A to G# (A-E-B-F#-C#-G#-D#-A#... reduced
/// mod 1200), with F reached by one descending fifth (a pure 4:3 fourth
/// above C) instead of an eleventh ascending fifth, matching the
/// conventional "one wolf interval" chain. **Indexed by step-within-
/// octave (`0 = A`, `1 = A#`, `2 = B`, `3 = C`, ... `11 = G#`), matching
/// `frequencyForTemperamentStep()`'s own `step == 0` reference anchor
/// directly - not `PitchClass`'s own `C = 0` indexing** (see
/// `stepWithinOctaveForPitchClass()` for the translation between the
/// two).
constexpr std::array<double, 12> kPythagoreanCentsFromA = {
    0.0, 113.685, 203.910, 294.135, 407.820, 498.045, 611.730, 701.955, 792.180, 905.865, 996.090, 1109.775,
};

/// @brief Quarter-comma meantone's own 12 pitch classes, as cents above
/// the reference pitch (A) - the same chain-of-fifths derivation as
/// `kPythagoreanCentsFromA`, except each fifth is narrowed to 696.578
/// cents (tempered by 1/4 of the syntonic comma) so every major third
/// four fifths apart - e.g. A-C# - lands on a pure 5:4 ratio (386.314
/// cents) instead of Pythagorean's own slightly-sharp one. **Indexed by
/// step-within-octave, the same `0 = A` convention as
/// `kPythagoreanCentsFromA`'s own docs describe - not `PitchClass`.**
constexpr std::array<double, 12> kMeantoneCentsFromA = {
    0.0, 76.046, 193.156, 310.266, 386.312, 503.422, 579.468, 696.578, 813.688, 889.734, 1006.844, 1082.890,
};

/// @brief `scaleSemitoneOffsets()`'s own backing storage, indexed the
/// same way `ScaleType`'s own enumerators are declared.
const std::vector<int>& offsetsFor(ScaleType scale) {
    static const std::vector<int> chromatic{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    static const std::vector<int> major{0, 2, 4, 5, 7, 9, 11};
    static const std::vector<int> dorian{0, 2, 3, 5, 7, 9, 10};
    static const std::vector<int> phrygian{0, 1, 3, 5, 7, 8, 10};
    static const std::vector<int> lydian{0, 2, 4, 6, 7, 9, 11};
    static const std::vector<int> mixolydian{0, 2, 4, 5, 7, 9, 10};
    static const std::vector<int> minor{0, 2, 3, 5, 7, 8, 10};
    static const std::vector<int> locrian{0, 1, 3, 5, 6, 8, 10};
    static const std::vector<int> harmonicMinor{0, 2, 3, 5, 7, 8, 11};
    static const std::vector<int> melodicMinor{0, 2, 3, 5, 7, 9, 11};
    static const std::vector<int> majorPentatonic{0, 2, 4, 7, 9};
    static const std::vector<int> minorPentatonic{0, 3, 5, 7, 10};
    static const std::vector<int> blues{0, 3, 5, 6, 7, 10};
    static const std::vector<int> wholeTone{0, 2, 4, 6, 8, 10};
    static const std::vector<int> octatonic{0, 2, 3, 5, 6, 8, 9, 11};

    switch (scale) {
        case ScaleType::Chromatic:
            return chromatic;
        case ScaleType::Major:
            return major;
        case ScaleType::Dorian:
            return dorian;
        case ScaleType::Phrygian:
            return phrygian;
        case ScaleType::Lydian:
            return lydian;
        case ScaleType::Mixolydian:
            return mixolydian;
        case ScaleType::Minor:
            return minor;
        case ScaleType::Locrian:
            return locrian;
        case ScaleType::HarmonicMinor:
            return harmonicMinor;
        case ScaleType::MelodicMinor:
            return melodicMinor;
        case ScaleType::MajorPentatonic:
            return majorPentatonic;
        case ScaleType::MinorPentatonic:
            return minorPentatonic;
        case ScaleType::Blues:
            return blues;
        case ScaleType::WholeTone:
            return wholeTone;
        case ScaleType::Octatonic:
            return octatonic;
    }
    return chromatic;
}

/// @brief Floor-mod, not C++'s own truncate-toward-zero `%` - keeps the
/// result in `[0, modulus)` even for a negative `value`, the same
/// reasoning `noteNameForFrequency()`'s own `octaveIndex` derivation
/// already relies on.
int floorMod(int value, int modulus) {
    const int remainder = value % modulus;
    return remainder < 0 ? remainder + modulus : remainder;
}

int floorDiv(int value, int modulus) {
    return (value - floorMod(value, modulus)) / modulus;
}

}  // namespace

std::string noteNameForFrequency(double frequencyHz, double referenceHz) noexcept {
    if (frequencyHz <= 0.0 || referenceHz <= 0.0) {
        return "";
    }

    // A4 is MIDI note 69, regardless of what frequency referenceHz says
    // A4 actually sits at - retuning changes which Hz value A4 *is*, not
    // the semitone math relating any other note to it.
    const double semitonesFromA4 = 12.0 * std::log2(frequencyHz / referenceHz);
    const int midiNote = 69 + static_cast<int>(std::lround(semitonesFromA4));

    // Floor division, not C++'s own truncate-toward-zero `/` - keeps
    // nameIndex in [0, 11] even for a midiNote below 0 (an octave
    // number below the one C0 itself starts, at the very bottom of any
    // real project's own frequency range, but not worth a hard failure
    // over).
    const int octaveIndex = static_cast<int>(std::floor(static_cast<double>(midiNote) / 12.0));
    const int nameIndex = midiNote - octaveIndex * 12;
    const int octave = octaveIndex - 1;

    return std::string(kNoteNames[static_cast<std::size_t>(nameIndex)]) + std::to_string(octave);
}

double frequencyForMidiNote(int midiNote, double referenceHz) noexcept {
    if (referenceHz <= 0.0) {
        return 0.0;
    }
    return referenceHz * std::pow(2.0, (static_cast<double>(midiNote) - 69.0) / 12.0);
}

int octaveNumberForFrequency(double frequencyHz, double referenceHz) noexcept {
    if (frequencyHz <= 0.0 || referenceHz <= 0.0) {
        return 0;
    }
    // C0's own frequency (MIDI note 12, the same 69 = A4 anchor every
    // other function here uses) - the universal "octave N starts here"
    // boundary in scientific pitch notation.
    const double c0Hz = frequencyForMidiNote(12, referenceHz);
    return static_cast<int>(std::floor(std::log2(frequencyHz / c0Hz)));
}

std::string pitchClassName(PitchClass pitchClass) noexcept {
    return kNoteNames[static_cast<std::size_t>(pitchClass)];
}

std::string scaleTypeName(ScaleType scale) noexcept {
    switch (scale) {
        case ScaleType::Chromatic:
            return "Chromatic";
        case ScaleType::Major:
            return "Major (Ionian)";
        case ScaleType::Dorian:
            return "Dorian";
        case ScaleType::Phrygian:
            return "Phrygian";
        case ScaleType::Lydian:
            return "Lydian";
        case ScaleType::Mixolydian:
            return "Mixolydian";
        case ScaleType::Minor:
            return "Minor (Aeolian)";
        case ScaleType::Locrian:
            return "Locrian";
        case ScaleType::HarmonicMinor:
            return "Harmonic Minor";
        case ScaleType::MelodicMinor:
            return "Melodic Minor";
        case ScaleType::MajorPentatonic:
            return "Major Pentatonic";
        case ScaleType::MinorPentatonic:
            return "Minor Pentatonic";
        case ScaleType::Blues:
            return "Blues";
        case ScaleType::WholeTone:
            return "Whole Tone";
        case ScaleType::Octatonic:
            return "Octatonic (Diminished)";
    }
    return "Chromatic";
}

const std::vector<int>& scaleSemitoneOffsets(ScaleType scale) noexcept { return offsetsFor(scale); }

std::vector<PitchClass> pitchClassesInScale(PitchClass key, ScaleType scale) {
    std::vector<PitchClass> result;
    const auto& offsets = offsetsFor(scale);
    result.reserve(offsets.size());
    for (const int offset : offsets) {
        result.push_back(static_cast<PitchClass>(floorMod(static_cast<int>(key) + offset, 12)));
    }
    return result;
}

std::string temperamentName(Temperament temperament) noexcept {
    switch (temperament) {
        case Temperament::Equal12:
            return "12-TET";
        case Temperament::Equal15:
            return "15-TET";
        case Temperament::Equal17:
            return "17-TET";
        case Temperament::Equal19:
            return "19-TET";
        case Temperament::Equal22:
            return "22-TET";
        case Temperament::Equal24:
            return "24-TET (Quarter Tone)";
        case Temperament::Equal31:
            return "31-TET";
        case Temperament::Equal34:
            return "34-TET";
        case Temperament::Equal41:
            return "41-TET";
        case Temperament::Equal53:
            return "53-TET";
        case Temperament::Equal72:
            return "72-TET";
        case Temperament::QuarterCommaMeantone:
            return "Quarter-Comma Meantone";
        case Temperament::Pythagorean:
            return "Pythagorean";
    }
    return "12-TET";
}

int stepsPerOctave(Temperament temperament) noexcept {
    switch (temperament) {
        case Temperament::Equal12:
            return 12;
        case Temperament::Equal15:
            return 15;
        case Temperament::Equal17:
            return 17;
        case Temperament::Equal19:
            return 19;
        case Temperament::Equal22:
            return 22;
        case Temperament::Equal24:
            return 24;
        case Temperament::Equal31:
            return 31;
        case Temperament::Equal34:
            return 34;
        case Temperament::Equal41:
            return 41;
        case Temperament::Equal53:
            return 53;
        case Temperament::Equal72:
            return 72;
        case Temperament::QuarterCommaMeantone:
        case Temperament::Pythagorean:
            return 12;
    }
    return 12;
}

bool supportsKeyAndScale(Temperament temperament) noexcept {
    switch (temperament) {
        case Temperament::Equal12:
        case Temperament::Equal24:
        case Temperament::QuarterCommaMeantone:
        case Temperament::Pythagorean:
            return true;
        default:
            return false;
    }
}

double frequencyForTemperamentStep(Temperament temperament, int step, double referenceHz) noexcept {
    if (referenceHz <= 0.0) {
        return 0.0;
    }

    if (temperament == Temperament::QuarterCommaMeantone || temperament == Temperament::Pythagorean) {
        const int octave = floorDiv(step, 12);
        const int stepWithinOctave = step - octave * 12;
        const auto& centsTable =
            temperament == Temperament::Pythagorean ? kPythagoreanCentsFromA : kMeantoneCentsFromA;
        const double cents = centsTable[static_cast<std::size_t>(stepWithinOctave)];
        return referenceHz * std::pow(2.0, static_cast<double>(octave)) * std::pow(2.0, cents / 1200.0);
    }

    const int divisions = stepsPerOctave(temperament);
    return referenceHz * std::pow(2.0, static_cast<double>(step) / static_cast<double>(divisions));
}

int stepWithinOctaveForPitchClass(Temperament temperament, PitchClass pitchClass) noexcept {
    const int stepsFor12 = floorMod(static_cast<int>(pitchClass) - kPitchClassIndexOfA, 12);
    const int divisionsPerSemitone = stepsPerOctave(temperament) / 12;
    return stepsFor12 * divisionsPerSemitone;
}

}  // namespace sound_mind::core
