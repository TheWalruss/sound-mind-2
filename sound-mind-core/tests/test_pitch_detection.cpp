#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/pitch_detection.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::core::detectPitch;
using sound_mind::core::detectPitches;
using sound_mind::core::noteFor;
using sound_mind::core::removePitchSpikes;

namespace {

/// @brief A harmonic tone: `f0` and harmonics 2-5 at falling levels.
sound_mind::codec::StreamImage harmonicTone(double f0, double fundamentalGain = 1.0) {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 60;
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    sound_mind::codec::AudioBuffer audio;
    audio.sampleRateHz = config.sampleRateHz;
    const auto count = static_cast<std::size_t>(settings.canvasWidth) * config.hopLength;
    for (std::size_t i = 0; i < count; ++i) {
        double value = 0.0;
        for (int harmonic = 1; harmonic <= 5; ++harmonic) {
            const double gain = (harmonic == 1 ? fundamentalGain : 1.0) * 0.3 / harmonic;
            value += gain * std::sin(2.0 * std::numbers::pi * f0 * harmonic * static_cast<double>(i) / config.sampleRateHz);
        }
        audio.left.push_back(static_cast<float>(value));
    }
    audio.right = audio.left;
    return sound_mind::codec::encode(audio, config);
}

/// @brief Two harmonic voices at once: a bass at `bassHz` (silent between
///        `bassGapStart` and `bassGapEnd`, as fractions of the length) and
///        a melody that moves from `melodyA` to `melodyB` halfway through.
sound_mind::codec::StreamImage twoVoices(double bassHz, double melodyA, double melodyB, double bassGapStart = 2.0,
                                         double bassGapEnd = 2.0) {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 120;
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    sound_mind::codec::AudioBuffer audio;
    audio.sampleRateHz = config.sampleRateHz;
    const auto count = static_cast<std::size_t>(settings.canvasWidth) * config.hopLength;
    double melodyPhase = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double position = static_cast<double>(i) / static_cast<double>(count);
        const double melodyHz = position < 0.5 ? melodyA : melodyB;
        melodyPhase += 2.0 * std::numbers::pi * melodyHz / config.sampleRateHz;
        const bool bassOn = position < bassGapStart || position >= bassGapEnd;
        double value = 0.0;
        for (int harmonic = 1; harmonic <= 5; ++harmonic) {
            value += 0.25 / harmonic * std::sin(melodyPhase * harmonic);
            if (bassOn) {
                value += 0.2 / harmonic *
                         std::sin(2.0 * std::numbers::pi * bassHz * harmonic * static_cast<double>(i) / config.sampleRateHz);
            }
        }
        audio.left.push_back(static_cast<float>(value));
    }
    audio.right = audio.left;
    return sound_mind::codec::encode(audio, config);
}

/// @brief A low line throughout and a higher one entering at `entry` (a
///        fraction of the length), over optional white noise - a guitar,
///        then a voice coming in over it.
sound_mind::codec::StreamImage lateHighVoice(double lowHz, double highHz, double entry, double noise = 0.0) {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 150;
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    sound_mind::codec::AudioBuffer audio;
    audio.sampleRateHz = config.sampleRateHz;
    const auto count = static_cast<std::size_t>(settings.canvasWidth) * config.hopLength;
    std::uint32_t random = 12345;
    for (std::size_t i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / config.sampleRateHz;
        double value = 0.0;
        for (int harmonic = 1; harmonic <= 5; ++harmonic) {
            value += 0.2 / harmonic * std::sin(2.0 * std::numbers::pi * lowHz * harmonic * t);
            if (static_cast<double>(i) >= entry * static_cast<double>(count)) {
                value += 0.25 / harmonic * std::sin(2.0 * std::numbers::pi * highHz * harmonic * t);
            }
        }
        random = random * 1664525u + 1013904223u;
        value += noise * (static_cast<double>(random) / 4294967296.0 - 0.5);
        audio.left.push_back(static_cast<float>(value));
    }
    audio.right = audio.left;
    return sound_mind::codec::encode(audio, config);
}

}  // namespace

TEST_CASE("detectPitch finds a harmonic tone's fundamental", "[core][pitch]") {
    for (const double f0 : {110.0, 220.0, 440.0}) {
        const auto pitch = detectPitch(harmonicTone(f0));
        INFO("f0 " << f0);
        REQUIRE(pitch.size() == 60);
        CHECK(pitch[30] == Catch::Approx(f0).epsilon(0.02));
    }
}

TEST_CASE("detectPitch doesn't report a sub-octave for a strong fundamental", "[core][pitch]") {
    const auto pitch = detectPitch(harmonicTone(300.0));
    CHECK(pitch[30] == Catch::Approx(300.0).epsilon(0.02));
}

TEST_CASE("detectPitch leaves silence unvoiced", "[core][pitch]") {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 10;
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    sound_mind::codec::AudioBuffer silence;
    silence.sampleRateHz = config.sampleRateHz;
    silence.left.assign(static_cast<std::size_t>(10) * config.hopLength, 0.0f);
    silence.right = silence.left;
    for (const float hz : detectPitch(sound_mind::codec::encode(silence, config))) {
        CHECK(hz == 0.0f);
    }
}

TEST_CASE("noteFor names notes and their tuning in cents", "[core][pitch]") {
    const auto a4 = noteFor(440.0);
    CHECK(a4.name == "A");
    CHECK(a4.octave == 4);
    CHECK(a4.cents == Catch::Approx(0.0).margin(1e-9));
    const auto middleC = noteFor(261.6256);
    CHECK(middleC.name == "C");
    CHECK(middleC.octave == 4);
    const auto sharp = noteFor(440.0 * std::pow(2.0, 0.2 / 12.0));
    CHECK(sharp.name == "A");
    CHECK(sharp.cents == Catch::Approx(20.0).margin(0.01));
    CHECK(noteFor(55.0).octave == 1);
}

// v0.1.40.9 - removing short pitch spikes. 10 ms frames throughout.
TEST_CASE("A short jump inside a steady note is smoothed back onto it", "[core][pitch]") {
    std::vector<float> track(20, 220.0f);
    track[8] = 440.0f;  // a 20 ms octave error.
    track[9] = 441.0f;
    const auto cleaned = removePitchSpikes(track, 0.01, 0.05);
    REQUIRE(cleaned.size() == track.size());
    CHECK(cleaned[8] == Catch::Approx(220.0f).margin(0.01));
    CHECK(cleaned[9] == Catch::Approx(220.0f).margin(0.01));
    CHECK(cleaned[0] == 220.0f);
    CHECK(cleaned[19] == 220.0f);
}

TEST_CASE("A bridged spike between two different notes glides between them", "[core][pitch]") {
    std::vector<float> track(10, 220.0f);
    for (std::size_t i = 5; i < 10; ++i) {
        track[i] = 226.0f;  // under a semitone above: the same "run" tolerance.
    }
    track[4] = 600.0f;  // spike between them.
    const auto cleaned = removePitchSpikes(track, 0.01, 0.03);
    CHECK(cleaned[4] > 220.0f);
    CHECK(cleaned[4] < 226.0f);
}

TEST_CASE("A short spike whose neighbors disagree is erased", "[core][pitch]") {
    std::vector<float> track(10, 220.0f);
    for (std::size_t i = 6; i < 10; ++i) {
        track[i] = 330.0f;  // a real note change, a fifth up.
    }
    track[5] = 800.0f;
    const auto cleaned = removePitchSpikes(track, 0.01, 0.03);
    CHECK(cleaned[5] == 0.0f);
    CHECK(cleaned[4] == 220.0f);
    CHECK(cleaned[6] == 330.0f);
}

TEST_CASE("A short blip in silence is erased; a run as long as the limit stays", "[core][pitch]") {
    std::vector<float> track(20, 0.0f);
    track[3] = 300.0f;  // 10 ms blip.
    track[4] = 301.0f;
    for (std::size_t i = 10; i < 15; ++i) {
        track[i] = 500.0f;  // 50 ms: not shorter than 50 ms.
    }
    const auto cleaned = removePitchSpikes(track, 0.01, 0.05);
    CHECK(cleaned[3] == 0.0f);
    CHECK(cleaned[4] == 0.0f);
    for (std::size_t i = 10; i < 15; ++i) {
        CHECK(cleaned[i] == 500.0f);
    }
}

TEST_CASE("Glides, vibrato and a zero limit pass through unchanged", "[core][pitch]") {
    std::vector<float> glide;
    for (int i = 0; i < 40; ++i) {
        glide.push_back(220.0f * std::pow(2.0f, static_cast<float>(i) / 40.0f));  // an octave, smoothly.
    }
    CHECK(removePitchSpikes(glide, 0.01, 0.1) == glide);
    std::vector<float> vibrato;
    for (int i = 0; i < 40; ++i) {
        vibrato.push_back(440.0f * std::pow(2.0f, 0.5f * std::sin(static_cast<float>(i)) / 12.0f));
    }
    CHECK(removePitchSpikes(vibrato, 0.01, 0.1) == vibrato);
    std::vector<float> spiky(10, 220.0f);
    spiky[5] = 880.0f;
    CHECK(removePitchSpikes(spiky, 0.01, 0.0) == spiky);
    CHECK(removePitchSpikes({}, 0.01, 0.1).empty());
}

// v0.1.40.11 - tracking several voices.
TEST_CASE("detectPitches separates a melody from a bassline, highest first", "[core][pitch]") {
    const auto tracks = detectPitches(twoVoices(110.0, 370.0, 392.0), 2);
    REQUIRE(tracks.size() == 2);
    REQUIRE(tracks[0].size() == 120);
    // Track 0 is the melody, moving between the halves; track 1 the steady bass.
    // (The melody is kept clear of the bass's harmonics - 220, 330, 440 Hz:
    // partials closer than the codec's frequency resolution beat against
    // each other, which makes the weaker one drop out now and then.)
    CHECK(tracks[0][30] == Catch::Approx(370.0).epsilon(0.03));
    CHECK(tracks[0][90] == Catch::Approx(392.0).epsilon(0.02));
    CHECK(tracks[1][30] == Catch::Approx(110.0).epsilon(0.03));
    CHECK(tracks[1][90] == Catch::Approx(110.0).epsilon(0.03));
}

TEST_CASE("detectPitches keeps a voice on its own track across a gap", "[core][pitch]") {
    // The bass drops out in the middle third and comes back.
    const auto tracks = detectPitches(twoVoices(110.0, 370.0, 392.0, 0.33, 0.66), 2);
    REQUIRE(tracks.size() == 2);
    CHECK(tracks[1][20] == Catch::Approx(110.0).epsilon(0.03));
    CHECK(tracks[1][50] == 0.0f);  // silent while the bass is.
    CHECK(tracks[0][50] == Catch::Approx(370.0).epsilon(0.03));
    CHECK(tracks[1][70] == 0.0f);  // still silent after the melody changes note mid-gap.
    CHECK(tracks[0][70] == Catch::Approx(392.0).epsilon(0.02));
    CHECK(tracks[1][100] == Catch::Approx(110.0).epsilon(0.03));  // back on its own track.
    CHECK(tracks[0][100] == Catch::Approx(392.0).epsilon(0.02));
}

TEST_CASE("detectPitches with one voice matches detectPitch; extra voices stay silent", "[core][pitch]") {
    const auto content = harmonicTone(220.0);
    const auto single = detectPitches(content, 1);
    REQUIRE(single.size() == 1);
    CHECK(single[0] == detectPitch(content));
    const auto three = detectPitches(content, 3);  // only one voice is there.
    REQUIRE(three.size() == 3);
    // It sits on the voice whose default register suits it (3 voices: about
    // 450, 250 and 140 Hz - 220 Hz is the middle one); the others stay silent.
    CHECK(three[1][30] == Catch::Approx(220.0).epsilon(0.02));
    std::size_t voicedElsewhere = 0;
    for (std::size_t frame = 0; frame < three[1].size(); ++frame) {
        voicedElsewhere += (three[0][frame] > 0.0f ? 1 : 0) + (three[2][frame] > 0.0f ? 1 : 0);
    }
    CHECK(voicedElsewhere < three[1].size() / 10);  // at most stray frames.
    CHECK(detectPitches(content, 0).size() == 1);  // clamped.
    CHECK(detectPitches(content, 99).size() == static_cast<std::size_t>(sound_mind::core::kMaxPitchVoices));
}

// v0.1.40.12 - voices are ordered layers.
TEST_CASE("A higher voice entering later takes voice 1; the lower line stays on voice 2", "[core][pitch]") {
    const auto tracks = detectPitches(lateHighVoice(147.0, 330.0, 0.6), 2);
    REQUIRE(tracks.size() == 2);
    CHECK(tracks[1][30] == Catch::Approx(147.0).epsilon(0.03));  // the guitar alone: the low voice.
    CHECK(tracks[0][30] == 0.0f);
    CHECK(tracks[1][120] == Catch::Approx(147.0).epsilon(0.03));  // still there once the voice enters...
    CHECK(tracks[0][120] == Catch::Approx(330.0).epsilon(0.03));  // ...which takes the high voice.
}

TEST_CASE("Voice 1 is never below voice 2, even over noise", "[core][pitch]") {
    const auto tracks = detectPitches(lateHighVoice(147.0, 330.0, 0.3, 0.3), 2);
    std::size_t both = 0;
    for (std::size_t frame = 0; frame < tracks[0].size(); ++frame) {
        if (tracks[0][frame] > 0.0f && tracks[1][frame] > 0.0f) {
            ++both;
            INFO("frame " << frame);
            CHECK(tracks[0][frame] > tracks[1][frame]);
        }
    }
    CHECK(both > 50);  // the two voices were really found together.
}

