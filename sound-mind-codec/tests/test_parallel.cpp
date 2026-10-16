#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "sound_mind/codec/parallel.h"
#include "sound_mind/codec/stream_codec.h"

using sound_mind::codec::parallelFor;
using sound_mind::codec::setMaxWorkerThreads;

namespace {

/// @brief Restores the default thread count however a test ends.
struct ThreadCountGuard {
    ~ThreadCountGuard() { setMaxWorkerThreads(0); }
};

sound_mind::codec::AudioBuffer chord(double seconds) {
    sound_mind::codec::AudioBuffer audio;
    audio.sampleRateHz = 44100;
    const auto count = static_cast<std::size_t>(seconds * audio.sampleRateHz);
    for (std::size_t i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / audio.sampleRateHz;
        double value = 0.0;
        for (const double hz : {220.0, 277.18, 329.63, 1250.0}) {
            value += 0.1 * std::sin(2.0 * std::numbers::pi * hz * t);
        }
        audio.left.push_back(static_cast<float>(value));
        audio.right.push_back(static_cast<float>(0.7 * value));
    }
    return audio;
}

}  // namespace

TEST_CASE("parallelFor visits every item exactly once, in fixed blocks", "[codec][parallel]") {
    const ThreadCountGuard guard;
    for (const std::size_t threads : {std::size_t{1}, std::size_t{4}}) {
        setMaxWorkerThreads(threads);
        std::vector<std::atomic<int>> visits(1000);
        std::atomic<int> blocks{0};
        parallelFor(1000, 64, [&](std::size_t begin, std::size_t end) {
            CHECK(begin % 64 == 0);
            CHECK(end - begin <= 64);
            ++blocks;
            for (std::size_t i = begin; i < end; ++i) {
                ++visits[i];
            }
        });
        CHECK(blocks == 16);  // ceil(1000 / 64).
        for (const auto& count : visits) {
            CHECK(count == 1);
        }
    }
    parallelFor(0, 8, [](std::size_t, std::size_t) { FAIL("no items, no calls"); });
}

TEST_CASE("parallelFor rethrows a block's exception on the caller", "[codec][parallel]") {
    const ThreadCountGuard guard;
    setMaxWorkerThreads(4);
    CHECK_THROWS_AS(parallelFor(100, 10,
                                [](std::size_t begin, std::size_t) {
                                    if (begin == 50) {
                                        throw std::runtime_error("block failed");
                                    }
                                }),
                    std::runtime_error);
}

TEST_CASE("The worker thread limit defaults to the machine and can be overridden", "[codec][parallel]") {
    const ThreadCountGuard guard;
    CHECK(sound_mind::codec::maxWorkerThreads() >= 1);
    setMaxWorkerThreads(3);
    CHECK(sound_mind::codec::maxWorkerThreads() == 3);
    setMaxWorkerThreads(0);
    CHECK(sound_mind::codec::maxWorkerThreads() >= 1);
}

TEST_CASE("encode and decode give identical results on one thread and on many", "[codec][parallel]") {
    const ThreadCountGuard guard;
    const auto audio = chord(3.0);
    const sound_mind::codec::StreamCodecConfig config;

    setMaxWorkerThreads(1);
    const auto serialImage = sound_mind::codec::encode(audio, config);
    const auto serialAudio = sound_mind::codec::decode(serialImage);
    setMaxWorkerThreads(8);
    const auto parallelImage = sound_mind::codec::encode(audio, config);
    const auto parallelAudio = sound_mind::codec::decode(serialImage);

    CHECK(parallelImage.leftMagnitudeDb == serialImage.leftMagnitudeDb);
    CHECK(parallelImage.rightMagnitudeDb == serialImage.rightMagnitudeDb);
    CHECK(parallelImage.sharedPhaseRadians == serialImage.sharedPhaseRadians);
    CHECK(parallelAudio.left == serialAudio.left);
    CHECK(parallelAudio.right == serialAudio.right);
}
