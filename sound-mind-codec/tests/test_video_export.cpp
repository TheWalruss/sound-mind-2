#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <numbers>
#include <stdexcept>
#include <algorithm>
#include <exception>
#include <string>
#include <thread>
#include <vector>

#define NOMINMAX
#include <objbase.h>

extern "C" {
#include <libavformat/avformat.h>
}

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/video_export.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::exportVideo;
using sound_mind::codec::ExportCancelled;
using sound_mind::codec::RgbImage;
using sound_mind::codec::VideoCodec;
using sound_mind::codec::VideoExportOptions;

namespace {

/// @brief Runs `work` on a fresh thread and rethrows anything it threw - how
/// the Studio runs exports (a BackgroundTask), away from whatever COM
/// apartment the calling thread is in.
template <typename Work>
void onWorkerThread(Work work) {
    std::exception_ptr error;
    std::thread([&] {
        try {
            work();
        } catch (...) {
            error = std::current_exception();
        }
    }).join();
    if (error) {
        std::rethrow_exception(error);
    }
}

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
    AVCodecID videoCodec = AV_CODEC_ID_NONE;
    AVCodecID audioCodec = AV_CODEC_ID_NONE;
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
        if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
            info.videoCodec = params->codec_id;
            info.videoWidth = params->width;
            info.videoHeight = params->height;
        } else if (params->codec_type == AVMEDIA_TYPE_AUDIO) {
            info.audioCodec = params->codec_id;
        }
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

TEST_CASE("exportVideo throws ExportCancelled once shouldCancel starts returning true",
          "[video_export][cancellation]") {
    // A few seconds at a low frame rate - enough real frames that a
    // cancel-after-N-calls callback below is exercising an in-progress,
    // multi-frame encode, not a one-frame edge case.
    const RgbImage canvas = makeTestCanvas(16, 16);
    const AudioBuffer audio = makeSineTone(440.0f, 3.0f, 44100);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-export-cancel.mp4";

    int callCount = 0;
    const auto shouldCancel = [&callCount]() {
        ++callCount;
        return callCount >= 3;  // cancels partway through, not on the very first frame.
    };

    CHECK_THROWS_AS(exportVideo(path, canvas, audio, /*frameRate=*/10, shouldCancel), ExportCancelled);
    CHECK(callCount == 3);  // stops checking (and encoding) the moment it cancels - doesn't run to completion first.

    std::filesystem::remove(path);  // exportVideo() itself leaves whatever partial file ffmpeg already wrote -
                                     // see this test file's own docs on why cleanup is the caller's job.
}

TEST_CASE("exportVideo with a shouldCancel that never returns true behaves exactly as without one",
          "[video_export][cancellation]") {
    const RgbImage canvas = makeTestCanvas(16, 16);
    const AudioBuffer audio = makeSineTone(440.0f, 0.2f, 44100);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-export-not-cancelled.mp4";

    int callCount = 0;
    const auto neverCancel = [&callCount]() {
        ++callCount;
        return false;
    };

    exportVideo(path, canvas, audio, /*frameRate=*/10, neverCancel);
    const VideoInfo info = readBackVideoInfo(path);
    std::filesystem::remove(path);

    CHECK(info.hasMpeg4Video);
    CHECK(callCount > 0);  // the callback really was consulted, not just accepted and ignored.
}

// v0.1.45.1 - formats, sizes and progress.
TEST_CASE("The always-available formats are found, with their file extensions", "[video_export]") {
    CHECK(sound_mind::codec::isVideoCodecAvailable(VideoCodec::Mpeg4));
    CHECK(sound_mind::codec::isVideoCodecAvailable(VideoCodec::ProRes));
    CHECK(sound_mind::codec::videoFileExtension(VideoCodec::H264) == "mp4");
    CHECK(sound_mind::codec::videoFileExtension(VideoCodec::ProRes) == "mov");
    const VideoCodec preferred = sound_mind::codec::preferredVideoCodec();
    CHECK((preferred == VideoCodec::H264) == sound_mind::codec::isVideoCodecAvailable(VideoCodec::H264));
}

TEST_CASE("Every available format writes a file with the right video and audio codecs", "[video_export]") {
    const RgbImage canvas = makeTestCanvas(64, 32);
    const AudioBuffer audio = makeSineTone(440.0f, 0.5f, 44100);
    for (const VideoCodec codec : sound_mind::codec::availableVideoCodecs()) {
        VideoExportOptions options;
        options.codec = codec;
        options.width = 320;
        options.height = 180;
        const auto path = std::filesystem::temp_directory_path() /
                          ("sound-mind-test-export-format." + sound_mind::codec::videoFileExtension(codec));
        onWorkerThread([&] { exportVideo(path, canvas, audio, options); });
        const VideoInfo info = readBackVideoInfo(path);
        std::filesystem::remove(path);

        const AVCodecID expectedVideo = codec == VideoCodec::H264    ? AV_CODEC_ID_H264
                                        : codec == VideoCodec::Hevc  ? AV_CODEC_ID_HEVC
                                        : codec == VideoCodec::ProRes ? AV_CODEC_ID_PRORES
                                                                      : AV_CODEC_ID_MPEG4;
        CHECK(info.videoCodec == expectedVideo);
        CHECK(info.audioCodec == (codec == VideoCodec::ProRes ? AV_CODEC_ID_PCM_S16LE : AV_CODEC_ID_AAC));
        CHECK(info.videoWidth == 320);
        CHECK(info.videoHeight == 180);
        CHECK(info.durationSeconds > 0.3);
    }
}

TEST_CASE("exportVideo reports progress up to the end", "[video_export]") {
    const RgbImage canvas = makeTestCanvas(32, 16);
    const AudioBuffer audio = makeSineTone(440.0f, 2.5f, 44100);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-export-progress.mp4";
    std::vector<double> reported;
    exportVideo(path, canvas, audio, VideoExportOptions{}, nullptr, [&](double done) { reported.push_back(done); });
    std::filesystem::remove(path);
    REQUIRE(reported.size() == 3);  // one per second of audio.
    CHECK(reported.front() > 0.0);
    CHECK(std::is_sorted(reported.begin(), reported.end()));
    CHECK(reported.back() == 1.0);
}

// v0.1.46.1: Media Foundation refuses a thread in a single-threaded COM
// apartment (a Qt GUI thread, say) - the export says so instead of failing
// with "Invalid argument".
TEST_CASE("H.264 export from a single-threaded COM apartment explains what's wrong", "[video_export]") {
    if (!sound_mind::codec::isVideoCodecAvailable(VideoCodec::H264)) {
        SKIP("no H.264 encoder on this computer");
    }
    std::string message;
    std::thread([&] {
        (void)CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        VideoExportOptions options;
        options.codec = VideoCodec::H264;
        options.width = 64;
        options.height = 32;
        try {
            exportVideo(std::filesystem::temp_directory_path() / "sound-mind-test-export-sta.mp4", makeTestCanvas(64, 32),
                        makeSineTone(440.0f, 0.1f, 44100), options);
        } catch (const std::runtime_error& e) {
            message = e.what();
        }
        CoUninitialize();
    }).join();
    std::filesystem::remove(std::filesystem::temp_directory_path() / "sound-mind-test-export-sta.mp4");
    CHECK(message.find("worker thread") != std::string::npos);
}

TEST_CASE("A format this computer lacks is refused up front", "[video_export]") {
    for (const VideoCodec codec : {VideoCodec::H264, VideoCodec::Hevc}) {
        if (sound_mind::codec::isVideoCodecAvailable(codec)) {
            continue;
        }
        VideoExportOptions options;
        options.codec = codec;
        const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-export-missing.mp4";
        CHECK_THROWS_AS(exportVideo(path, makeTestCanvas(16, 16), makeSineTone(440.0f, 0.1f, 44100), options),
                        std::runtime_error);
    }
}
