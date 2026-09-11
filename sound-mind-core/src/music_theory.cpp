#include "sound_mind/core/music_theory.h"

#include <array>
#include <cmath>

namespace sound_mind::core {

namespace {

constexpr std::array<const char*, 12> kNoteNames = {"C", "C#", "D",  "D#", "E",  "F",
                                                      "F#", "G",  "G#", "A",  "A#", "B"};

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

}  // namespace sound_mind::core
