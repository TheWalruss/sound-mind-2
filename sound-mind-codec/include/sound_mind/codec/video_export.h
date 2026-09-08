#pragma once

#include <filesystem>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/rgb_image.h"

namespace sound_mind::codec {

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
 * @throws std::runtime_error if the file can't be written, or ffmpeg
 *         couldn't create/open an encoder or muxer for it.
 */
void exportVideo(const std::filesystem::path& path, const RgbImage& canvas, const AudioBuffer& audio,
                  int frameRate = 30);

}  // namespace sound_mind::codec
