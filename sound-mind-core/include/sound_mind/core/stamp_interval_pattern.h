#pragma once

#include <string>
#include <vector>

namespace sound_mind::core {

/**
 * @brief One interval in a stamp-interval pattern - `docs/sound-mind-
 *        design.md`'s "Stamp Intervals", `docs/sound-mind-roadmap.md`'s
 *        `v0.Y.54.1` (Paint Tool Enhancements), "non-uniform timing along
 *        a path".
 */
struct StampIntervalToken {
    /// @brief The magnitude - milliseconds if `isBeats` is `false`,
    ///        beats (tempo-relative) if `true`.
    double value = 0.0;

    /// @brief Whether `value` is beats (`"b"` suffix) rather than
    ///        milliseconds (`"ms"` suffix).
    bool isBeats = false;
};

/**
 * @brief Parses a stamp-interval pattern into a cyclic sequence of
 *        tokens - `StampMode::AlongCurve`'s own alternative to a single
 *        fixed `ToolConfiguration::stampInterval()`.
 *
 * **Grammar** (whitespace-separated tokens, in order, at least one
 * required): each token is a positive number immediately followed by
 * exactly one unit suffix - `"ms"` for a fixed millisecond interval, or
 * `"b"` for one relative to the project's own tempo (resolved via
 * `resolveStampIntervalPattern()`, the same `(60 / bpm) * value` beats
 * convention `parseSequenceNotation()`'s own `<duration>` grammar uses).
 * Unlike that grammar, there is no bare-number/no-suffix form here - a
 * fixed millisecond-scale interval left unitless would be too easy to
 * mistake for seconds, given how differently-sized a mistake that would
 * be for something this fine-grained.
 *
 * A single-token pattern (`"100ms"`) is a valid, if degenerate, way to
 * spell a fixed interval - the same effect as leaving `stampIntervalText()`
 * unset, just spelled out explicitly (see that method's own docs for
 * which one actually takes effect when both are present).
 *
 * @param pattern The pattern text to parse.
 * @return The parsed tokens, in the order they appear in `pattern`, ready
 *         to cycle through repeatedly (see `sampleStrokeAlongCurve()`'s
 *         own docs on how the cycle actually advances a stroke).
 * @throws std::invalid_argument if `pattern` is empty, or any token
 *         doesn't match the grammar above - the message names the
 *         offending token and why.
 */
[[nodiscard]] std::vector<StampIntervalToken> parseStampIntervalPattern(const std::string& pattern);

/**
 * @brief The inverse of `parseStampIntervalPattern()` - renders `tokens`
 *        back into pattern text.
 * @param tokens The tokens to render, in order.
 * @return The rendered pattern text; an empty string for an empty
 *         `tokens`.
 */
[[nodiscard]] std::string stampIntervalPatternFor(const std::vector<StampIntervalToken>& tokens);

/**
 * @brief Resolves `tokens` into concrete seconds, given the project's own
 *        tempo - the same "parse once, resolve against `bpm` at the point
 *        of use" split `parseSequenceNotation()`'s own `bpm` parameter
 *        already establishes, so a project's tempo can change without
 *        needing to re-parse anything stored.
 * @param tokens The tokens to resolve, in order.
 * @param bpm The tempo beats-suffixed tokens are resolved against -
 *        ordinarily a project's own `ProjectSettings::defaultTempoBpm`.
 * @return One interval in seconds per token, in the same order.
 */
[[nodiscard]] std::vector<double> resolveStampIntervalPattern(const std::vector<StampIntervalToken>& tokens,
                                                                double bpm);

}  // namespace sound_mind::core
