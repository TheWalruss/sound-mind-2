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
};

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
