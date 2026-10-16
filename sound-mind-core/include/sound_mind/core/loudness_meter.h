#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::core {

/// @brief The level reported for silence (or anything below the BS.1770
///        absolute gate) - a finite stand-in for minus infinity.
inline constexpr double kLoudnessFloorLufs = -70.0;

/// @brief The step between successive momentary/short-term readings, in
///        seconds - BS.1770's 75% block overlap of a 400 ms block.
inline constexpr double kLoudnessStepSeconds = 0.1;

/**
 * @brief Everything the Analysis panel's loudness meter shows for one piece
 *        of audio - the mix or a single layer (Pre-Beta Plan `v0.Y.80.1`).
 *
 * Loudness follows ITU-R BS.1770-4 / EBU R128: K-weighted, channels summed,
 * integrated loudness gated at -70 LUFS absolute and -10 LU relative over
 * 400 ms blocks with 75% overlap.
 */
struct LoudnessMeasurement {
    /// @brief Gated integrated loudness of the whole buffer, LUFS
    ///        (`kLoudnessFloorLufs` when silent).
    double integratedLufs = kLoudnessFloorLufs;
    /// @brief The loudest short-term (3 s) reading, LUFS.
    double maxShortTermLufs = kLoudnessFloorLufs;
    /// @brief The loudest momentary (400 ms) reading, LUFS.
    double maxMomentaryLufs = kLoudnessFloorLufs;
    /// @brief The largest absolute sample value, dBFS.
    double samplePeakDbfs = kLoudnessFloorLufs;
    /// @brief The largest inter-sample peak (4x oversampled), dBTP - at
    ///        least `samplePeakDbfs`.
    double truePeakDbtp = kLoudnessFloorLufs;
    /// @brief How many samples, in either channel, exceed full scale
    ///        (`|x| > 1`) - each would hard-clip on output.
    std::size_t clippedSamples = 0;
    /// @brief Momentary loudness every `kLoudnessStepSeconds` (reading `i`
    ///        covers the 400 ms ending at `(i + 1) * kLoudnessStepSeconds`).
    std::vector<float> momentaryLufs;
    /// @brief Short-term loudness every `kLoudnessStepSeconds` (the 3 s
    ///        ending at the same moment as the matching momentary reading).
    std::vector<float> shortTermLufs;
};

/**
 * @brief Measures `audio`'s loudness, peaks and clipping - see
 *        `LoudnessMeasurement`.
 *
 * K-weighting uses BS.1770's two filters (high shelf ~1.68 kHz +4 dB,
 * high-pass ~38 Hz), designed for `audio.sampleRateHz` from their analog
 * prototypes, so any sample rate reads correctly. True peak interpolates
 * three points between samples with a windowed-sinc kernel, only near
 * samples within 6 dB of the sample peak (an inter-sample peak can't exceed
 * its neighbors by more than that in practice), which keeps a long mix fast.
 *
 * @param audio The audio to measure (mono is read from `left` only).
 * @param shouldCancel Polled between stages; an empty function never cancels.
 * @return The measurement; all floors for an empty buffer.
 * @throws std::runtime_error ("cancelled") if `shouldCancel` returns `true`.
 * @note Allocates; run off the audio and GUI threads for long buffers.
 */
[[nodiscard]] LoudnessMeasurement measureLoudness(const sound_mind::codec::AudioBuffer& audio,
                                                  const std::function<bool()>& shouldCancel = {});

/**
 * @brief Just the gated integrated loudness of `audio`, LUFS - what output
 *        loudness normalization needs, without the peak search or the
 *        momentary/short-term series, so it's quick enough for the GUI thread
 *        when playback audio is loaded.
 * @param audio The audio to measure.
 * @return The same value `measureLoudness(audio).integratedLufs` gives.
 */
[[nodiscard]] double integratedLoudnessLufs(const sound_mind::codec::AudioBuffer& audio);

/// @brief One frequency band of the per-band loudness view.
struct LoudnessBand {
    /// @brief Display name, e.g. "Bass (60–250 Hz)".
    std::string label;
    /// @brief Lower edge, Hz.
    double lowHz = 0.0;
    /// @brief Upper edge, Hz.
    double highHz = 0.0;
};

/// @brief The six bands the loudness meter splits the spectrum into: sub,
///        bass, low-mid, mid, high-mid and air (the legacy Studio's own).
/// @return The bands, lowest first.
[[nodiscard]] const std::vector<LoudnessBand>& loudnessBands();

/**
 * @brief Each band's level in every frame of `content`, read straight from
 *        the spectral data: the power mean of the band's bins, with the
 *        codec's A-weighting removed, in dB.
 * @param content Layer or composite content.
 * @return `bands x frameCount` values, row-major by band; a band with no
 *         bins in `content`'s range reads `kLoudnessFloorLufs`.
 */
[[nodiscard]] std::vector<float> bandLevelsOverTime(const sound_mind::codec::StreamImage& content);

}  // namespace sound_mind::core
