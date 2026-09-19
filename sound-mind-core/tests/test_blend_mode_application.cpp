#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numbers>

#include "sound_mind/core/blend_mode_application.h"

using sound_mind::core::applyBlendedCell;
using sound_mind::core::BlendedCell;
using sound_mind::core::BlendMode;

namespace {
constexpr float kPi = std::numbers::pi_v<float>;
}

// --- Overwrite -----------------------------------------------------------

TEST_CASE("applyBlendedCell's Overwrite always returns overlay verbatim, regardless of opacity",
          "[core][blend_mode_application]") {
    const BlendedCell base{-10.0f, -20.0f, 0.1f};
    const BlendedCell overlay{-30.0f, -40.0f, 2.0f};

    const auto result1 = applyBlendedCell(BlendMode::Overwrite, base, overlay, 1.0f);
    const auto result0 = applyBlendedCell(BlendMode::Overwrite, base, overlay, 0.0f);

    for (const auto* result : {&result1, &result0}) {
        CHECK(result->leftMagnitudeDb == overlay.leftMagnitudeDb);
        CHECK(result->rightMagnitudeDb == overlay.rightMagnitudeDb);
        CHECK(result->phaseRadians == overlay.phaseRadians);
    }
}

// --- Normal ----------------------------------------------------------------

TEST_CASE("applyBlendedCell's Normal reproduces overlay onto a silent base at full opacity",
          "[core][blend_mode_application]") {
    const BlendedCell base{-96.0f, -96.0f, 0.0f};
    const BlendedCell overlay{-20.0f, -30.0f, 0.5f};

    const auto result = applyBlendedCell(BlendMode::Normal, base, overlay, 1.0f);

    CHECK(result.leftMagnitudeDb == Catch::Approx(-20.0f).margin(0.05));
    CHECK(result.rightMagnitudeDb == Catch::Approx(-30.0f).margin(0.05));
    CHECK(result.phaseRadians == Catch::Approx(0.5f).margin(0.01));
}

TEST_CASE("applyBlendedCell's Normal at zero opacity leaves base unchanged", "[core][blend_mode_application]") {
    const BlendedCell base{-20.0f, -25.0f, 0.3f};
    const BlendedCell overlay{-5.0f, -5.0f, 1.0f};

    const auto result = applyBlendedCell(BlendMode::Normal, base, overlay, 0.0f);

    CHECK(result.leftMagnitudeDb == Catch::Approx(-20.0f).margin(0.05));
    CHECK(result.rightMagnitudeDb == Catch::Approx(-25.0f).margin(0.05));
    CHECK(result.phaseRadians == Catch::Approx(0.3f).margin(0.01));
}

TEST_CASE("applyBlendedCell's Normal sums two equal in-phase signals into a 6dB-louder result",
          "[core][blend_mode_application]") {
    // Two identical, in-phase (same direction) linear-amplitude contributors
    // sum to exactly double the linear amplitude - +6.02dB (20*log10(2)).
    const BlendedCell base{-20.0f, -20.0f, 0.0f};
    const BlendedCell overlay{-20.0f, -20.0f, 0.0f};

    const auto result = applyBlendedCell(BlendMode::Normal, base, overlay, 1.0f);

    CHECK(result.leftMagnitudeDb == Catch::Approx(-20.0f + 6.0206f).margin(0.05));
    CHECK(result.rightMagnitudeDb == Catch::Approx(-20.0f + 6.0206f).margin(0.05));
    CHECK(result.phaseRadians == Catch::Approx(0.0f).margin(0.01));
}

// --- Multiply --------------------------------------------------------------

TEST_CASE("applyBlendedCell's Multiply multiplies unit-normalized amplitude per channel",
          "[core][blend_mode_application]") {
    // dbToUnit(-48) = 0.5 exactly. 0.5 * 0.5 = 0.25 -> unitToDb(0.25) = -72.
    const BlendedCell base{-48.0f, -48.0f, 0.0f};
    const BlendedCell overlay{-48.0f, -48.0f, 0.0f};

    const auto result = applyBlendedCell(BlendMode::Multiply, base, overlay, 1.0f);

    CHECK(result.leftMagnitudeDb == Catch::Approx(-72.0f).margin(0.05));
    CHECK(result.rightMagnitudeDb == Catch::Approx(-72.0f).margin(0.05));
}

TEST_CASE("applyBlendedCell's Multiply at zero opacity leaves base unchanged", "[core][blend_mode_application]") {
    const BlendedCell base{-30.0f, -40.0f, 0.2f};
    const BlendedCell overlay{-10.0f, -10.0f, 1.5f};

    const auto result = applyBlendedCell(BlendMode::Multiply, base, overlay, 0.0f);

    CHECK(result.leftMagnitudeDb == Catch::Approx(-30.0f).margin(0.05));
    CHECK(result.rightMagnitudeDb == Catch::Approx(-40.0f).margin(0.05));
    CHECK(result.phaseRadians == Catch::Approx(0.2f).margin(0.01));
}

TEST_CASE("applyBlendedCell's Multiply adds phases at full opacity", "[core][blend_mode_application]") {
    const BlendedCell base{-48.0f, -48.0f, 0.3f};    // unit 0.5.
    const BlendedCell overlay{-48.0f, -48.0f, 0.5f};  // unit 0.5.

    const auto result = applyBlendedCell(BlendMode::Multiply, base, overlay, 1.0f);

    CHECK(result.phaseRadians == Catch::Approx(0.8f).margin(0.01));
}

// --- Screen ------------------------------------------------------------

TEST_CASE("applyBlendedCell's Screen is Multiply's own inverse", "[core][blend_mode_application]") {
    // 1 - (1-0.5)*(1-0.5) = 0.75 -> unitToDb(0.75) = -24.
    const BlendedCell base{-48.0f, -48.0f, 0.0f};
    const BlendedCell overlay{-48.0f, -48.0f, 0.0f};

    const auto result = applyBlendedCell(BlendMode::Screen, base, overlay, 1.0f);

    CHECK(result.leftMagnitudeDb == Catch::Approx(-24.0f).margin(0.05));
}

TEST_CASE("applyBlendedCell's Screen falls back to base phase when base and overlay cancel exactly",
          "[core][blend_mode_application]") {
    const BlendedCell base{-48.0f, -48.0f, 0.0f};       // unit 0.5, phase 0.
    const BlendedCell overlay{-48.0f, -48.0f, kPi};      // unit 0.5, phase pi - exactly opposing.

    const auto result = applyBlendedCell(BlendMode::Screen, base, overlay, 1.0f);

    CHECK(result.phaseRadians == Catch::Approx(0.0f).margin(0.01));
}

// --- Overlay -----------------------------------------------------------

TEST_CASE("applyBlendedCell's Overlay uses Multiply's own formula when base is dark",
          "[core][blend_mode_application]") {
    // base unit 0.25 (< 0.5): 2*0.25*0.5 = 0.25 -> unitToDb(0.25) = -72.
    const BlendedCell base{-72.0f, -72.0f, 0.0f};
    const BlendedCell overlay{-48.0f, -48.0f, 0.0f};

    const auto result = applyBlendedCell(BlendMode::Overlay, base, overlay, 1.0f);

    CHECK(result.leftMagnitudeDb == Catch::Approx(-72.0f).margin(0.05));
}

TEST_CASE("applyBlendedCell's Overlay uses Screen's own formula when base is light",
          "[core][blend_mode_application]") {
    // base unit 0.75 (>= 0.5): 1 - 2*(1-0.75)*(1-0.5) = 1 - 0.25 = 0.75 -> unitToDb(0.75) = -24.
    const BlendedCell base{-24.0f, -24.0f, 0.0f};
    const BlendedCell overlay{-48.0f, -48.0f, 0.0f};

    const auto result = applyBlendedCell(BlendMode::Overlay, base, overlay, 1.0f);

    CHECK(result.leftMagnitudeDb == Catch::Approx(-24.0f).margin(0.05));
}

// --- Difference --------------------------------------------------------

TEST_CASE("applyBlendedCell's Difference is the absolute difference of unit-normalized amplitude",
          "[core][blend_mode_application]") {
    // dbToUnit(-28.8) = 0.7, dbToUnit(-67.2) = 0.3. |0.7-0.3| = 0.4 -> unitToDb(0.4) = -57.6.
    const BlendedCell base{-28.8f, -28.8f, 0.0f};
    const BlendedCell overlay{-67.2f, -67.2f, 0.0f};

    const auto result = applyBlendedCell(BlendMode::Difference, base, overlay, 1.0f);

    CHECK(result.leftMagnitudeDb == Catch::Approx(-57.6f).margin(0.1));
}

TEST_CASE("applyBlendedCell's Difference amplitude can cancel to silence while phase still reflects "
          "real complex interference",
          "[core][blend_mode_application]") {
    // Equal unit amplitude (1.0, i.e. 0dB) on both sides means the plain
    // |a-b| amplitude formula cancels to exactly 0 (silence) - but the
    // *complex* difference (opposing phases) is very much not zero,
    // demonstrating the deliberate decoupling from legacy's own formula.
    const BlendedCell base{0.0f, 0.0f, 0.0f};    // unit 1.0, phase 0.
    const BlendedCell overlay{0.0f, 0.0f, kPi};   // unit 1.0, phase pi.

    const auto result = applyBlendedCell(BlendMode::Difference, base, overlay, 1.0f);

    CHECK(result.leftMagnitudeDb == Catch::Approx(-96.0f).margin(0.05));  // amplitude cancels to silence.
    CHECK(result.phaseRadians == Catch::Approx(0.0f).margin(0.01));       // Z_base - Z_overlay = 1 - (-1) = 2 (phase 0).
}

// --- Add -----------------------------------------------------------------

TEST_CASE("applyBlendedCell's Add sums unit-normalized amplitude", "[core][blend_mode_application]") {
    // dbToUnit(-38.4)=0.6? Let's use clean fractions instead: 0.3+0.4=0.7.
    const BlendedCell base{-96.0f + 0.3f * 96.0f, -96.0f + 0.3f * 96.0f, 0.0f};
    const BlendedCell overlay{-96.0f + 0.4f * 96.0f, -96.0f + 0.4f * 96.0f, 0.0f};

    const auto result = applyBlendedCell(BlendMode::Add, base, overlay, 1.0f);

    CHECK(result.leftMagnitudeDb == Catch::Approx(-96.0f + 0.7f * 96.0f).margin(0.1));
}

TEST_CASE("applyBlendedCell's Add clamps amplitude at full loudness rather than overflowing",
          "[core][blend_mode_application]") {
    const BlendedCell base{-96.0f + 0.8f * 96.0f, -96.0f + 0.8f * 96.0f, 0.0f};
    const BlendedCell overlay{-96.0f + 0.5f * 96.0f, -96.0f + 0.5f * 96.0f, 0.0f};

    const auto result = applyBlendedCell(BlendMode::Add, base, overlay, 1.0f);

    CHECK(result.leftMagnitudeDb == Catch::Approx(0.0f).margin(0.05));  // clamped to unit 1.0 (0dB).
}

TEST_CASE("applyBlendedCell's Add sums phases as a complex sum", "[core][blend_mode_application]") {
    const BlendedCell base{0.0f, 0.0f, 0.0f};                                  // unit 1.0, phase 0.
    const BlendedCell overlay{0.0f, 0.0f, std::numbers::pi_v<float> / 2.0f};    // unit 1.0, phase pi/2.

    const auto result = applyBlendedCell(BlendMode::Add, base, overlay, 1.0f);

    // Z = 1 + i -> angle = pi/4.
    CHECK(result.phaseRadians == Catch::Approx(kPi / 4.0f).margin(0.01));
}

TEST_CASE("applyBlendedCell falls back to base phase when the blended result is exactly silent",
          "[core][blend_mode_application]") {
    // Multiply with one side at absolute silence -> blended magnitude is
    // exactly 0, so the phase-space result has no real direction at all.
    const BlendedCell base{-96.0f, -96.0f, 0.0f};   // unit 0, phase 0.
    const BlendedCell overlay{-96.0f, -96.0f, 1.0f};  // unit 0, phase 1.0 - irrelevant, base wins the fallback.

    const auto result = applyBlendedCell(BlendMode::Multiply, base, overlay, 1.0f);

    CHECK(result.phaseRadians == Catch::Approx(0.0f).margin(0.01));
}
