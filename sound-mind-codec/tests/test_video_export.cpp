#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <numbers>
#include <stdexcept>

extern "C" {
#include <libavformat/avformat.h>
}

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/video_export.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::exportVideo;
using sound_mind::codec::RgbImage;

namespace {

AudioBuffer makeSineTone(float frequencyHz, float durationSeconds, std::uint32_t sampleRateHz) {
    AudioBuffer audio;
    audio.sampleRateHz = sampleRateHz;
    const auto sampleCount = static_cast<std::size_t>(durationSeconds * static_cast<float>(sampleRateHz));
    audio.left.resize(sampleCount);
    audio.right.resize(sampleCount);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const float sample = std::sin(2.0f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) /
                                       static_cast<float>(sampleRateHz));
        audio.left[i] = sample;
        audio.right[i] = sample;
    }
    return audio;
}

/// @brief A small solid-color test canvas - odd dimensions deliberately,
/// to exercise exportVideo()'s even-padding behavior (see video_export.h).
RgbImage makeTestCanvas(std::uint32_t width, std::uint32_t height) {
    RgbImage image;
    image.width = width;
    image.height = height;
    image.pixels.assign(std::size_t{width} * height * 3, std::uint8_t{64});
    return image;
}

/// @brief Demuxes just enough of an exported file (stream count/codecs/
/// dimensions/duration) to verify exportVideo()'s output for real, without
/// a full decode - test-only, using ffmpeg's own demuxing API directly
/// (mirroring test_audio_export.cpp's readBackViaJuce() pattern: a
/// verification-only reader, not part of any public API).
struct VideoInfo {
    bool hasMpeg4Video = false;
    bool hasAacAudio = false;
    int videoWidth = 0;
    int videoHeight = 0;
    double durationSeconds = 0.0;
};

VideoInfo readBackVideoInfo(const std::filesystem::path& path) {
    AVFormatContext* formatCtx = nullptr;
    if (avformat_open_input(&formatCtx, path.string().c_str(), nullptr, nullptr) < 0) {
        throw std::runtime_error("could not open exported video for reading: " + path.string());
    }
    if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
        avformat_close_input(&formatCtx);
        throw std::runtime_error("could not read stream info from exported video: " + path.string());
    }

    VideoInfo info;
    for (unsigned int i = 0; i < formatCtx->nb_streams; ++i) {
        const AVCodecParameters* params = formatCtx->streams[i]->codecpar;
        if (params->codec_type == AVMEDIA_TYPE_VIDEO && params->codec_id == AV_CODEC_ID_MPEG4) {
            info.hasMpeg4Video = true;
            info.videoWidth = params->width;
            info.videoHeight = params->height;
        } else if (params->codec_type == AVMEDIA_TYPE_AUDIO && params->codec_id == AV_CODEC_ID_AAC) {
            info.hasAacAudio = true;
        }
    }
    if (formatCtx->duration > 0) {
        info.durationSeconds = static_cast<double>(formatCtx->duration) / AV_TIME_BASE;
    }

    avformat_close_input(&formatCtx);
    return info;
}

}  // namespace

TEST_CASE("exportVideo writes an MP4 with mpeg4 video and AAC audio streams", "[video_export]") {
    const RgbImage canvas = makeTestCanvas(101, 51);  // odd on purpose - exercises even-padding.
    const AudioBuffer audio = makeSineTone(440.0f, 1.0f, 44100);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-export.mp4";

    exportVideo(path, canvas, audio, /*frameRate=*/24);
    const VideoInfo info = readBackVideoInfo(path);
    std::filesystem::remove(path);

    CHECK(info.hasMpeg4Video);
    CHECK(info.hasAacAudio);
    CHECK(info.videoWidth == 102);   // 101 rounded up to even.
    CHECK(info.videoHeight == 52);   // 51 rounded up to even.
    CHECK(info.durationSeconds > 0.8);
    CHECK(info.durationSeconds < 1.2);
}

TEST_CASE("exportVideo throws for an empty canvas", "[video_export]") {
    const RgbImage emptyCanvas;
    const AudioBuffer audio = makeSineTone(440.0f, 0.1f, 44100);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-export-empty.mp4";

    CHECK_THROWS_AS(exportVideo(path, emptyCanvas, audio), std::runtime_error);
}

TEST_CASE("exportVideo throws for a non-positive frame rate", "[video_export]") {
    const RgbImage canvas = makeTestCanvas(16, 16);
    const AudioBuffer audio = makeSineTone(440.0f, 0.1f, 44100);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-export-badrate.mp4";

    CHECK_THROWS_AS(exportVideo(path, canvas, audio, /*frameRate=*/0), std::runtime_error);
}

TEST_CASE("exportVideo throws for an unwritable path", "[video_export]") {
    const RgbImage canvas = makeTestCanvas(16, 16);
    const AudioBuffer audio = makeSineTone(440.0f, 0.1f, 44100);
    const auto path = std::filesystem::path("Z:/does/not/exist/sound-mind-test-export.mp4");

    CHECK_THROWS_AS(exportVideo(path, canvas, audio), std::runtime_error);
}
