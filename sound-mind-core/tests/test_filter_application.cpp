#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/filter_application.h"
#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::codec::StreamImage;
using sound_mind::core::applyFilter;
using sound_mind::core::FilterConfiguration;
using sound_mind::core::FilterType;
using sound_mind::core::ProjectSettings;

namespace {

/// @brief A 3-bin, 2-column StreamImage with uniform left/right dB and a
/// distinctive, easy-to-check phase - small enough to hand-verify every
/// cell of applyFilter()'s own output.
StreamImage makeComposite() {
    StreamImage composite;
    composite.config.binCount = 3;
    composite.frameCount = 2;
    composite.leftMagnitudeDb.assign(6, -20.0f);
    composite.rightMagnitudeDb.assign(6, -10.0f);
    composite.sharedPhaseRadians.assign(6, 0.5f);
    return composite;
}

}  // namespace

TEST_CASE("applyFilter's FrequencyAxisGradient blends each bin toward the gradient's own stop at that bin",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::FrequencyAxisGradient);
    auto& gradient = config.frequencyGradient();
    // t=0 (bin 0, lowest frequency): force left to 0 dB, leave right alone.
    gradient.setStopValues(0, {0.0f, 0.0f, -96.0f, 1.0f, 0.0f});
    // t=1 (bin 2, highest frequency): leave left alone, force right to 0 dB.
    gradient.setStopValues(1, {1.0f, -96.0f, 0.0f, 0.0f, 1.0f});

    const auto filtered = applyFilter(makeComposite(), config, ProjectSettings{});

    REQUIRE(filtered.config.binCount == 3);
    REQUIRE(filtered.frameCount == 2);

    // Bin 0 (t=0): left forced to 0 dB, right unchanged (-10 dB).
    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(0.0f));
    CHECK(filtered.leftMagnitudeDb[1] == Catch::Approx(0.0f));
    CHECK(filtered.rightMagnitudeDb[0] == Catch::Approx(-10.0f));
    CHECK(filtered.rightMagnitudeDb[1] == Catch::Approx(-10.0f));

    // Bin 1 (t=0.5): halfway between the original value and the
    // interpolated stop (-48 dB intensity, 0.5 opacity on both channels).
    const std::size_t bin1 = 1 * 2;
    CHECK(filtered.leftMagnitudeDb[bin1] == Catch::Approx(-34.0f));
    CHECK(filtered.rightMagnitudeDb[bin1] == Catch::Approx(-29.0f));

    // Bin 2 (t=1): left unchanged (-20 dB), right forced to 0 dB.
    const std::size_t bin2 = 2 * 2;
    CHECK(filtered.leftMagnitudeDb[bin2] == Catch::Approx(-20.0f));
    CHECK(filtered.leftMagnitudeDb[bin2 + 1] == Catch::Approx(-20.0f));
    CHECK(filtered.rightMagnitudeDb[bin2] == Catch::Approx(0.0f));
    CHECK(filtered.rightMagnitudeDb[bin2 + 1] == Catch::Approx(0.0f));
}

TEST_CASE("applyFilter's FrequencyAxisGradient leaves phase untouched", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::FrequencyAxisGradient);
    config.frequencyGradient().setStopValues(0, {0.0f, 0.0f, 0.0f, 1.0f, 1.0f});

    const auto filtered = applyFilter(makeComposite(), config, ProjectSettings{});

    for (const float phase : filtered.sharedPhaseRadians) {
        CHECK(phase == 0.5f);
    }
}

TEST_CASE("applyFilter is a harmless passthrough for a filter type not implemented yet",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::Sharpen);  // Installment B's own scope, not yet built.

    const auto composite = makeComposite();
    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb == composite.leftMagnitudeDb);
    CHECK(filtered.rightMagnitudeDb == composite.rightMagnitudeDb);
    CHECK(filtered.sharedPhaseRadians == composite.sharedPhaseRadians);
}
