#include "sound_mind/core/chord_generator.h"

#include <algorithm>
#include <array>
#include <random>

#include "sound_mind/core/music_theory.h"

namespace sound_mind::core {

namespace {

// Ported directly from the legacy Studio's own data/chords.json - see
// chord_generator.h's own docs on why this is a compiled-in table here
// rather than a runtime-loaded file.
const std::vector<ChordDefinition> kTriads = {
    {"Major", "", {0, 4, 7}},
    {"Minor", "m", {0, 3, 7}},
    {"Diminished", "dim", {0, 3, 6}},
    {"Augmented", "aug", {0, 4, 8}},
    {"Sus2", "sus2", {0, 2, 7}},
    {"Sus4", "sus4", {0, 5, 7}},
    {"Power", "5", {0, 7}},
};

const std::vector<ChordDefinition> kSixths = {
    {"Major 6th", "6", {0, 4, 7, 9}},
    {"Minor 6th", "m6", {0, 3, 7, 9}},
    {"6/9", "69", {0, 4, 7, 9, 14}},
    {"Minor 6/9", "m69", {0, 3, 7, 9, 14}},
};

const std::vector<ChordDefinition> kSevenths = {
    {"Major 7th", "maj7", {0, 4, 7, 11}},
    {"Minor 7th", "m7", {0, 3, 7, 10}},
    {"Dominant 7th", "7", {0, 4, 7, 10}},
    {"Diminished 7th", "dim7", {0, 3, 6, 9}},
    {"Half-Diminished (m7b5)", "m7b5", {0, 3, 6, 10}},
    {"Minor Major 7th", "mM7", {0, 3, 7, 11}},
    {"Augmented Major 7th", "M7+5", {0, 4, 8, 11}},
    {"Dominant 7th Sus4", "7sus4", {0, 5, 7, 10}},
    {"Dominant 7th b5", "7b5", {0, 4, 6, 10}},
    {"Dominant 7th #5", "7#5", {0, 4, 8, 10}},
};

const std::vector<ChordDefinition> kNinths = {
    {"Major 9th", "maj9", {0, 4, 7, 11, 14}},
    {"Minor 9th", "m9", {0, 3, 7, 10, 14}},
    {"Dominant 9th", "9", {0, 4, 7, 10, 14}},
    {"9th Sus4", "9sus4", {0, 5, 7, 10, 14}},
    {"Dominant 7th b9", "7b9", {0, 4, 7, 10, 13}},
};

const std::vector<ChordDefinition> kExtendedAdded = {
    {"Add 9", "add9", {0, 4, 7, 14}},
    {"Minor Add 9", "madd9", {0, 3, 7, 14}},
    {"Add 11", "add11", {0, 4, 7, 17}},
    {"Add 4", "add4", {0, 4, 5, 7}},
    {"Minor Add 4", "madd4", {0, 3, 5, 7}},
    {"Sus4 Add 9", "sus4add9", {0, 5, 7, 14}},
    {"Dominant 7th #11", "7#11", {0, 4, 7, 10, 18}},
    {"Minor 7th Add 11", "m7add11", {0, 3, 7, 10, 17}},
    {"Minor Major 7th Add 11", "mM7add11", {0, 3, 7, 11, 17}},
    {"Half-Dim b9 (m7b9b5)", "m7b9b5", {0, 3, 6, 10, 13}},
};

std::vector<int> ascendingRun(int n) {
    std::vector<int> result;
    if (n <= 0) {
        return result;
    }
    result.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        result.push_back(i);
    }
    return result;
}

std::vector<int> descendingRun(int n) {
    std::vector<int> result = ascendingRun(n);
    std::reverse(result.begin(), result.end());
    return result;
}

std::vector<int> upDown(int n) {
    std::vector<int> result = ascendingRun(n);
    for (int i = n - 2; i > 0; --i) {
        result.push_back(i);
    }
    return result;
}

std::vector<int> downUp(int n) {
    std::vector<int> result = descendingRun(n);
    for (int i = 1; i < n - 1; ++i) {
        result.push_back(i);
    }
    return result;
}

std::vector<int> alternating(int n) {
    std::vector<int> result;
    if (n <= 0) {
        return result;
    }
    int lo = 0;
    int hi = n - 1;
    while (lo <= hi) {
        result.push_back(lo);
        if (lo != hi) {
            result.push_back(hi);
        }
        ++lo;
        --hi;
    }
    return result;
}

// Stable sort of 0..n-1 by an edge-distance key - shared by OutsideIn (key
// descending, i.e. farthest-from-edge/most-central first) and InsideOut
// (key ascending, edges first) - see chord_generator.h's own docs on why
// these names read backwards from what the sort actually does; a faithful
// port of arpeggiator.py's own _preset_outside_in()/_preset_inside_out().
std::vector<int> sortByEdgeDistance(int n, bool centralFirst) {
    std::vector<int> indices = ascendingRun(n);
    std::stable_sort(indices.begin(), indices.end(), [n, centralFirst](int a, int b) {
        const int keyA = std::min(a, n - 1 - a);
        const int keyB = std::min(b, n - 1 - b);
        return centralFirst ? keyA > keyB : keyA < keyB;
    });
    return indices;
}

}  // namespace

const std::vector<ChordDefinition>& chordsInCategory(ChordCategory category) {
    switch (category) {
        case ChordCategory::Triads:
            return kTriads;
        case ChordCategory::Sixths:
            return kSixths;
        case ChordCategory::Sevenths:
            return kSevenths;
        case ChordCategory::Ninths:
            return kNinths;
        case ChordCategory::ExtendedAdded:
            return kExtendedAdded;
    }
    return kTriads;
}

std::vector<int> arpeggioIndexSequence(ArpeggioOrder order, int noteCount, const std::vector<int>& customIndices,
                                        std::uint64_t randomSeed) {
    if (noteCount <= 0) {
        return {};
    }

    switch (order) {
        case ArpeggioOrder::Ascending:
            return ascendingRun(noteCount);
        case ArpeggioOrder::Descending:
            return descendingRun(noteCount);
        case ArpeggioOrder::UpDown:
            return upDown(noteCount);
        case ArpeggioOrder::DownUp:
            return downUp(noteCount);
        case ArpeggioOrder::Alternating:
            return alternating(noteCount);
        case ArpeggioOrder::OutsideIn:
            return sortByEdgeDistance(noteCount, /*centralFirst=*/true);
        case ArpeggioOrder::InsideOut:
            return sortByEdgeDistance(noteCount, /*centralFirst=*/false);
        case ArpeggioOrder::Custom:
            return customIndices.empty() ? ascendingRun(noteCount) : customIndices;
        case ArpeggioOrder::Random: {
            std::vector<int> indices = ascendingRun(noteCount);
            std::mt19937_64 rng(randomSeed);
            std::shuffle(indices.begin(), indices.end(), rng);
            return indices;
        }
    }
    return ascendingRun(noteCount);
}

std::vector<NoteEvent> buildChordNotes(const ChordGeneratorParams& params) {
    const std::vector<ChordDefinition>& chords = chordsInCategory(params.category);
    if (params.chordIndex >= chords.size()) {
        return {};
    }
    const ChordDefinition& chord = chords[params.chordIndex];
    const auto noteCount = static_cast<int>(chord.intervals.size());

    std::vector<double> notesHz;
    notesHz.reserve(chord.intervals.size());
    for (int interval : chord.intervals) {
        notesHz.push_back(frequencyForMidiNote(params.rootMidiNote + interval, params.referenceHz));
    }

    std::vector<NoteEvent> notes;

    if (params.mode == ChordPlaybackMode::Block) {
        notes.reserve(notesHz.size());
        for (double hz : notesHz) {
            notes.push_back(NoteEvent{params.startTimeSeconds, params.blockDurationSeconds, hz});
        }
        return notes;
    }

    // Arpeggio mode. bpm clamped to a positive minimum, matching
    // arpeggiator.py's own subdivision_to_cols() precedent
    // (`60_000.0 / max(1.0, bpm)`), rather than dividing by (or near) zero.
    const double stepDurationSeconds = (60.0 / std::max(1.0, params.bpm)) * params.stepBeats;
    const double noteDurationSeconds = stepDurationSeconds * params.noteDurationFraction;
    const std::vector<int> sequence =
        arpeggioIndexSequence(params.order, noteCount, params.customOrderIndices, params.randomSeed);
    const int repeats = std::max(1, params.repeats);

    notes.reserve(sequence.size() * static_cast<std::size_t>(repeats));
    int stepIndex = 0;
    for (int repeat = 0; repeat < repeats; ++repeat) {
        for (int idx : sequence) {
            if (idx >= 0 && idx < noteCount) {
                const double startTime = params.startTimeSeconds + static_cast<double>(stepIndex) * stepDurationSeconds;
                notes.push_back(NoteEvent{startTime, noteDurationSeconds, notesHz[static_cast<std::size_t>(idx)]});
            }
            ++stepIndex;
        }
    }
    return notes;
}

}  // namespace sound_mind::core
