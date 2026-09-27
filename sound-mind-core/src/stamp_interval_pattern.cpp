#include "sound_mind/core/stamp_interval_pattern.h"

#include <array>
#include <cctype>
#include <charconv>
#include <stdexcept>

namespace sound_mind::core {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw std::invalid_argument("Stamp interval pattern: " + message);
}

std::string formatNumber(double value) {
    std::array<char, 32> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string(buffer.data(), result.ptr);
}

/// @brief Parses one `<number>ms`/`<number>b` token - see
/// `parseStampIntervalPattern()`'s own docs on the grammar.
StampIntervalToken parseToken(const std::string& token) {
    if (token.size() >= 2 && (token[token.size() - 2] == 'm' && token[token.size() - 1] == 's')) {
        const std::string numeric = token.substr(0, token.size() - 2);
        if (numeric.empty()) {
            fail("missing number before 'ms' in '" + token + "'");
        }
        std::size_t pos = 0;
        double value = 0.0;
        try {
            value = std::stod(numeric, &pos);
        } catch (const std::exception&) {
            fail("'" + token + "' is not a valid millisecond interval");
        }
        if (pos != numeric.size() || value <= 0.0) {
            fail("'" + token + "' is not a valid millisecond interval");
        }
        return StampIntervalToken{value, false};
    }
    if (token.size() >= 1 && (token.back() == 'b' || token.back() == 'B')) {
        const std::string numeric = token.substr(0, token.size() - 1);
        if (numeric.empty()) {
            fail("missing number before 'b' in '" + token + "'");
        }
        std::size_t pos = 0;
        double value = 0.0;
        try {
            value = std::stod(numeric, &pos);
        } catch (const std::exception&) {
            fail("'" + token + "' is not a valid beats interval");
        }
        if (pos != numeric.size() || value <= 0.0) {
            fail("'" + token + "' is not a valid beats interval");
        }
        return StampIntervalToken{value, true};
    }
    fail("'" + token + "' is missing a unit suffix ('ms' or 'b')");
}

}  // namespace

std::vector<StampIntervalToken> parseStampIntervalPattern(const std::string& pattern) {
    std::vector<StampIntervalToken> tokens;

    std::size_t i = 0;
    const std::size_t n = pattern.size();
    while (i < n) {
        while (i < n && std::isspace(static_cast<unsigned char>(pattern[i]))) {
            ++i;
        }
        if (i >= n) {
            break;
        }
        const std::size_t tokenStart = i;
        while (i < n && !std::isspace(static_cast<unsigned char>(pattern[i]))) {
            ++i;
        }
        tokens.push_back(parseToken(pattern.substr(tokenStart, i - tokenStart)));
    }

    if (tokens.empty()) {
        fail("pattern is empty");
    }
    return tokens;
}

std::string stampIntervalPatternFor(const std::vector<StampIntervalToken>& tokens) {
    std::string result;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) {
            result += " ";
        }
        result += formatNumber(tokens[i].value) + (tokens[i].isBeats ? "b" : "ms");
    }
    return result;
}

std::vector<double> resolveStampIntervalPattern(const std::vector<StampIntervalToken>& tokens, double bpm) {
    std::vector<double> seconds;
    seconds.reserve(tokens.size());
    for (const StampIntervalToken& token : tokens) {
        seconds.push_back(token.isBeats ? (60.0 / bpm) * token.value : token.value / 1000.0);
    }
    return seconds;
}

}  // namespace sound_mind::core
