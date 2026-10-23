#pragma once

#include <cstdint>

#include "sound_mind/codec/audio_buffer.h"

namespace sound_mind::codec {

/**
 * @brief Converts `audio` to another sample rate (`v0.1.49.3`) - so audio
 *        from a file, a recording device or a source at any rate lands in a
 *        project at the project's own rate, with its pitch and timing kept.
 *
 * Band-limited windowed-sinc interpolation (a Kaiser window, 32 taps each
 * side of the lower rate), read from a precomputed kernel table. Converting
 * down lowers the cutoff to the new rate's Nyquist limit, so content the new
 * rate can't hold is removed rather than aliased. The duration is kept: the
 * result has `round(frames * targetRateHz / audio.sampleRateHz)` frames.
 * Samples beyond either end count as silence, so the first and last few
 * milliseconds fade in and out slightly.
 *
 * @param audio The audio; both channels are converted.
 * @param targetRateHz The rate to convert to.
 * @return The converted audio, tagged `targetRateHz`; an unchanged copy if
 *         the rates already match, or if either rate is `0`.
 * @note Runs on parallelFor()'s worker threads; not for the real-time audio
 *       thread (it allocates).
 */
[[nodiscard]] AudioBuffer resample(const AudioBuffer& audio, std::uint32_t targetRateHz);

}  // namespace sound_mind::codec
