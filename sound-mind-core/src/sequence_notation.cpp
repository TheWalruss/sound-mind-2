#include "sound_mind/core/sequence_notation.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <stdexcept>

#include "sound_mind/core/music_theory.h"

namespace sound_mind::core {

namespace {

// Semitone offset for each letter A-G, indexed by (letter - 'A') - matches
// music_theory.cpp's own kNoteNames ordering (C=0, C#=1, ..., B=11).
constexpr std::array<int, 7> kSemitoneForLetter = {9, 11, 0, 2, 4, 5, 7};

[[noreturn]] void fail(const std::string& message) { throw std::invalid_argument("Sequence notation: " + message); }

std::string formatNumber(double value) {
    std::array<char, 32> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string(buffer.data(), result.ptr);
}

double parseNumber(const std::string& text, const std::string& context) {
    if (text.empty()) {
        fail("missing number in " + context);
    }
    std::size_t pos = 0;
    double value = 0.0;
    try {
        value = std::stod(text, &pos);
    } catch (const std::exception&) {
        fail("'" + text + "' is not a valid number in " + context);
    }
    if (pos != text.size()) {
        fail("'" + text + "' is not a valid number in " + context);
    }
    return value;
}

int parseInt(const std::string& text, const std::string& context) {
    if (text.empty()) {
        fail("missing octave in " + context);
    }
    std::size_t pos = 0;
    int value = 0;
    try {
        value = std::stoi(text, &pos);
    } catch (const std::exception&) {
        fail("'" + text + "' is not a valid octave in " + context);
    }
    if (pos != text.size()) {
        fail("'" + text + "' is not a valid octave in " + context);
    }
    return value;
}

/// @brief Parses a `<duration>` token - a plain non-negative number,
/// optionally 'b'/'B'-suffixed for beats (converted via `(60/bpm) * value`).
double parseDuration(const std::string& text, double bpm, const std::string& context) {
    if (text.empty()) {
        fail("missing duration in " + context);
    }
    const bool beats = text.back() == 'b' || text.back() == 'B';
    const std::string numeric = beats ? text.substr(0, text.size() - 1) : text;
    const double value = parseNumber(numeric, context);
    if (value < 0.0) {
        fail("duration '" + text + "' must not be negative in " + context);
    }
    return beats ? (60.0 / bpm) * value : value;
}

/// @brief Parses a `<pitch>` token - a note name (`A`-`G`, optional `#`,
/// signed octave) or a bare positive Hz value.
double parsePitch(const std::string& text, double referenceHz, const std::string& context) {
    if (text.empty()) {
        fail("missing pitch in " + context);
    }
    const char first = text.front();
    if (first >= 'A' && first <= 'G') {
        std::size_t index = 1;
        int semitone = kSemitoneForLetter[static_cast<std::size_t>(first - 'A')];
        if (index < text.size() && text[index] == '#') {
            semitone += 1;
            ++index;
        }
        const int octave = parseInt(text.substr(index), context);
        const int midiNote = (octave + 1) * 12 + semitone;
        return frequencyForMidiNote(midiNote, referenceHz);
    }
    const double hz = parseNumber(text, context);
    if (hz <= 0.0) {
        fail("Hz value '" + text + "' must be positive in " + context);
    }
    return hz;
}

}  // namespace

std::vector<NoteEvent> parseSequenceNotation(const std::string& notation, double referenceHz, double bpm,
                                              double startTimeSeconds) {
    std::vector<NoteEvent> notes;
    double currentTime = startTimeSeconds;

    std::size_t i = 0;
    const std::size_t n = notation.size();
    while (i < n) {
        while (i < n && std::isspace(static_cast<unsigned char>(notation[i]))) {
            ++i;
        }
        if (i >= n) {
            break;
        }
        const std::size_t tokenStart = i;
        while (i < n && !std::isspace(static_cast<unsigned char>(notation[i]))) {
            ++i;
        }
        const std::string token = notation.substr(tokenStart, i - tokenStart);

        if (token.front() == 'z' || token.front() == 'Z') {
            const double duration = parseDuration(token.substr(1), bpm, "rest '" + token + "'");
            currentTime += duration;
            continue;
        }

        std::vector<std::string> subtokens;
        std::size_t subStart = 0;
        for (std::size_t j = 0; j <= token.size(); ++j) {
            if (j == token.size() || token[j] == '+') {
                subtokens.push_back(token.substr(subStart, j - subStart));
                subStart = j + 1;
            }
        }

        double groupMaxDuration = 0.0;
        for (const std::string& sub : subtokens) {
            if (sub.empty()) {
                fail("empty note in chord group '" + token + "'");
            }
            const std::size_t colon = sub.find(':');
            if (colon == std::string::npos) {
                fail("note '" + sub + "' is missing a ':duration'");
            }
            const std::string pitchText = sub.substr(0, colon);
            const std::string durationText = sub.substr(colon + 1);
            const double frequencyHz = parsePitch(pitchText, referenceHz, "note '" + sub + "'");
            const double duration = parseDuration(durationText, bpm, "note '" + sub + "'");
            notes.push_back(NoteEvent{currentTime, duration, frequencyHz});
            groupMaxDuration = std::max(groupMaxDuration, duration);
        }
        currentTime += groupMaxDuration;
    }

    return notes;
}

std::string sequenceNotationFor(const std::vector<NoteEvent>& notes) {
    if (notes.empty()) {
        return "";
    }

    std::vector<NoteEvent> sorted = notes;
    std::stable_sort(sorted.begin(), sorted.end(),
                      [](const NoteEvent& a, const NoteEvent& b) { return a.startTimeSeconds < b.startTimeSeconds; });

    std::vector<std::string> tokens;
    double cursorTime = sorted.front().startTimeSeconds;
    std::size_t i = 0;
    while (i < sorted.size()) {
        const double groupStart = sorted[i].startTimeSeconds;
        std::size_t j = i;
        double groupMaxDuration = 0.0;
        std::string groupToken;
        while (j < sorted.size() && sorted[j].startTimeSeconds == groupStart) {
            if (j > i) {
                groupToken += "+";
            }
            groupToken += formatNumber(sorted[j].frequencyHz) + ":" + formatNumber(sorted[j].durationSeconds);
            groupMaxDuration = std::max(groupMaxDuration, sorted[j].durationSeconds);
            ++j;
        }

        const double gap = groupStart - cursorTime;
        if (gap > 0.0) {
            tokens.push_back("z" + formatNumber(gap));
        }
        tokens.push_back(groupToken);

        cursorTime = groupStart + groupMaxDuration;
        i = j;
    }

    std::string result;
    for (std::size_t k = 0; k < tokens.size(); ++k) {
        if (k > 0) {
            result += " ";
        }
        result += tokens[k];
    }
    return result;
}

}  // namespace sound_mind::core
