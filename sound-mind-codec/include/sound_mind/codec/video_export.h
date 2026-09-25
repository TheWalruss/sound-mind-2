#pragma once

#include <filesystem>
#include <functional>
#include <stdexcept>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/rgb_image.h"

namespace sound_mind::codec {

/**
 * @brief Thrown by exportVideo() when its own `shouldCancel` callback
 *        starts returning `true` mid-encode - `docs/sound-mind-roadmap.md`'s
 *        "real, non-blocking cancel affordance for long operations"
 *        milestone (`v0.0.45.14`, Installment B).
 *
 * A distinct type, not a plain `std::runtime_error`, specifically so a
 * caller can tell "cancelled on request" apart from a genuine encode
 * failure - e.g. to skip showing an error message for the former while
 * still surfacing the latter. `exportVideo()` itself doesn't clean up
 * whatever partial file ffmpeg had already written by the point of
 * cancellation - matching every other error path in this function, which
 * also leaves the caller responsible for deciding what to do with a
 * partially-written output on failure.
 */
class ExportCancelled : public std::runtime_error {
public:
    ExportCancelled() : std::runtime_error("export cancelled") {}
};

/**
 * @brief Exports an MP4 video of a spectrogram canvas, synced to its audio.
 *
 * Per `docs/sound-mind-design.md`'s Export section: the whole canvas
 * visible for the full duration, with a moving playback indicator (a
 * vertical line sweeping left to right) - the design doc's other option
 * (a scrolling window that pans to keep the indicator in view) is deferred
 * to a later pass, since a single static image plus a moving line is
 * enough to satisfy "animation synced to audio" without also building
 * scroll-window logic.
 *
 * Video: MPEG-4 Part 2 ("mpeg4" in ffmpeg), an LGPL-compatible codec built
 * into ffmpeg itself - not H.264, which needs the GPL-licensed libx264 and
 * so doesn't fit the closed-source distribution posture already committed
 * to via the JUCE Starter tier (see `docs/sound-mind-architecture.md`'s
 * Decisions Made for both). Audio: AAC, ffmpeg's own native encoder (also
 * LGPL-compatible) - MP4's conventional audio codec, independent of the
 * standalone MP3 export `exportCompressedAudio()` provides.
 *
 * `canvas`'s width/height are right/bottom-padded with black to the
 * nearest even dimensions if needed, since the video's pixel format
 * (YUV420P) needs even width/height for its half-resolution chroma planes
 * - transparent to the caller, just a note on why the output frame size
 * can be 1px larger than `canvas` in either dimension.
 *
 * @param path Destination path.
 * @param canvas The (static) spectrogram image to animate a playhead over -
 *        typically `toRgbImage()`'s output for whichever layer/composite
 *        is being exported.
 * @param audio The audio to mux alongside the video, and to derive the
 *        video's total duration and playhead speed from.
 * @param frameRate Video frame rate, in frames per second.
 * @param shouldCancel Consulted once per video frame, right before that
 *        frame's own render+encode work begins - `docs/sound-mind-roadmap.md`'s
 *        "real, non-blocking cancel affordance for long operations"
 *        milestone. Once it returns `true`, throws `ExportCancelled`
 *        immediately rather than doing that frame's work, or any
 *        subsequent one. `nullptr` (the default) never cancels - the
 *        exact prior behavior, unchanged for every existing caller.
 * @throws std::runtime_error if the file can't be written, or ffmpeg
 *         couldn't create/open an encoder or muxer for it.
 * @throws ExportCancelled if `shouldCancel` returns `true` - see its own
 *         docs.
 */
void exportVideo(const std::filesystem::path& path, const RgbImage& canvas, const AudioBuffer& audio,
                  int frameRate = 30, const std::function<bool()>& shouldCancel = nullptr);

}  // namespace sound_mind::codec
