#pragma once

#include <string>
#include <vector>

#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::core {

/// @brief Where pitch detection looks, and how strong a pitch must be.
struct PitchSettings {
    /// @brief The lowest fundamental considered, Hz.
    double minHz = 50.0;
    /// @brief The highest fundamental considered, Hz.
    double maxHz = 2000.0;
    /// @brief A frame quieter than this (its loudest cell, A-weighting
    ///        removed) is unvoiced, dB.
    double silenceDb = -60.0;
    /// @brief The fundamental must be within this many dB of the frame's
    ///        loudest cell - otherwise a strong harmonic's sub-octave could
    ///        win on its harmonics alone.
    double fundamentalWithinDb = 30.0;
    /// @brief detectPitches() only: a second or later voice in a frame must
    ///        score at least this fraction of the frame's strongest voice's
    ///        harmonic sum - weaker leftovers aren't reported as voices.
    double additionalVoiceMinScoreRatio = 0.2;
};

/// @brief The most voices detectPitches() will track.
inline constexpr int kMaxPitchVoices = 6;

/**
 * @brief Finds the fundamental frequency in every frame of `content`, read
 *        straight from the spectral data (Pre-Beta Plan `v0.Y.80.1`).
 *
 * Each candidate bin in `[minHz, maxHz]` is scored by **harmonic
 * summation** - the sum of the linear amplitudes (both channels, A-weighting
 * removed) at its first five harmonics, each divided by its harmonic number,
 * taking the loudest bin within half a bin of each harmonic. The best
 * candidate whose own level is within `fundamentalWithinDb` of the frame's
 * loudest cell wins; its frequency is refined by fitting a parabola through
 * its neighbors' levels (sub-bin accuracy). A frame below `silenceDb`, or
 * with no qualifying candidate, is unvoiced.
 *
 * @param content Layer or composite content.
 * @param settings The search range and thresholds.
 * @return One fundamental per frame, in Hz; `0` where unvoiced.
 */
[[nodiscard]] std::vector<float> detectPitch(const sound_mind::codec::StreamImage& content,
                                             const PitchSettings& settings = {});

/**
 * @brief Tracks up to `voiceCount` simultaneous pitches - a melody over a
 *        bassline, say - and keeps each on its own consistent track
 *        (`v0.1.40.11`).
 *
 * **Per frame:** the strongest pitch is found exactly as detectPitch()
 * finds it. Its harmonics (up to the 12th, each ±3% or ± one of the codec's
 * linear FFT bins, whichever is wider) are then removed from a working copy
 * of the spectrum, and the search repeats on what's left, up to
 * `voiceCount` times. A later pick must score at least
 * `additionalVoiceMinScoreRatio` of the first and lie more than a semitone
 * from every voice already found in the frame, so a single voice isn't
 * padded out with its own leftovers.
 *
 * **Limitations:** a voice sharing most of its harmonics with a louder one
 * (an octave above or below it) is absorbed by it. Two partials closer than
 * about one linear FFT bin (25 Hz at the defaults) beat against each other,
 * so the weaker voice can drop out now and then.
 *
 * **Voices are ordered layers** (`v0.1.40.12`): voice 1 is the highest
 * line, the last voice the lowest, and in every frame a higher pitch is on a
 * higher voice - voices never cross. Each frame's pitches, highest first,
 * go to voices in that same order, choosing - among the order-keeping
 * assignments - the one with the lowest total cost:
 * - a voice heard within the last 250 ms costs the distance, in semitones,
 *   from its last pitch;
 * - otherwise, 1 plus half the distance from the voice's **register**: a
 *   running average of its own pitch with a ~2 s memory, or, until it's
 *   first heard, a default spread evenly (in log frequency) from about
 *   800 Hz down to 80 Hz - two voices start near 450 and 140 Hz, so a lone
 *   bass or guitar line lands on the low voice and a singer coming in over
 *   it takes the high one;
 * - leaving a pitch unassigned costs 14 - allowed for all but the frame's
 *   strongest, so a stray extra pitch (two notes seen at once just where a
 *   note changes, say) is dropped rather than pulled into a voice.
 *
 * @param content Layer or composite content.
 * @param voiceCount How many tracks, clamped to [1, `kMaxPitchVoices`].
 *        With 1, the single track equals detectPitch()'s.
 * @param settings The search range and thresholds.
 * @return `voiceCount` tracks, each one fundamental per frame in Hz (`0`
 *         where that voice is silent).
 */
[[nodiscard]] std::vector<std::vector<float>> detectPitches(const sound_mind::codec::StreamImage& content,
                                                            int voiceCount, const PitchSettings& settings = {});

/// @brief How far apart, in semitones, two neighboring frames' pitches can
///        be and still count as one continuous run - see removePitchSpikes().
inline constexpr double kPitchRunToleranceSemitones = 1.0;

/**
 * @brief Removes pitch "spikes" - runs shorter than `maxSpikeSeconds` -
 *        from a detected pitch track (`v0.1.40.9`).
 *
 * The voiced frames are split into **runs**: consecutive frames whose
 * pitches are within `kPitchRunToleranceSemitones` of each other, so a glide
 * or vibrato stays one run while a sudden jump (an octave error, a
 * transient) starts a new one. A run lasting less than `maxSpikeSeconds`
 * (frames × `secondsPerFrame`) - or several such runs back to back,
 * together lasting less - is a spike:
 * - **smoothed** when the frames on both sides are voiced and within
 *   tolerance of each other: it's replaced by a glide between them,
 *   interpolated in log frequency;
 * - **erased** (made unvoiced, `0`) otherwise - a blip in silence, at the
 *   track's edges, or between two different notes.
 *
 * @param hzPerFrame The track - see detectPitch() (`0` unvoiced).
 * @param secondsPerFrame The frame duration.
 * @param maxSpikeSeconds Runs shorter than this are spikes; `0` or less
 *        returns the track unchanged.
 * @return The cleaned track, the same length.
 */
[[nodiscard]] std::vector<float> removePitchSpikes(const std::vector<float>& hzPerFrame, double secondsPerFrame,
                                                   double maxSpikeSeconds);

/// @brief A frequency as a musical note.
struct NoteName {
    /// @brief The pitch class, e.g. "A" or "C#".
    std::string name;
    /// @brief The octave in scientific pitch notation (middle C is C4).
    int octave = 4;
    /// @brief How far from the note's exact pitch, in cents, `[-50, 50)`.
    double cents = 0.0;
};

/**
 * @brief The nearest equal-tempered note to `frequencyHz`.
 * @param frequencyHz The frequency; must be positive.
 * @param a4Hz Concert A's tuning.
 * @return The note.
 */
[[nodiscard]] NoteName noteFor(double frequencyHz, double a4Hz = 440.0);

}  // namespace sound_mind::core
