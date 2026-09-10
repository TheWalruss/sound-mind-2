#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/gradient.h"

using sound_mind::core::Gradient;
using sound_mind::core::GradientStop;

TEST_CASE("A fresh Gradient has exactly two stops, at t=0 and t=1", "[core][gradient]") {
    const Gradient gradient;
    REQUIRE(gradient.stops().size() == 2);
    REQUIRE(gradient.stops().front().t == 0.0f);
    REQUIRE(gradient.stops().back().t == 1.0f);
}

TEST_CASE("A fresh Gradient is fully transparent at both endpoints", "[core][gradient]") {
    const Gradient gradient;
    for (const GradientStop& stop : gradient.stops()) {
        REQUIRE(stop.leftIntensity == 0.0f);
        REQUIRE(stop.rightIntensity == 0.0f);
        REQUIRE(stop.leftOpacity == 0.0f);
        REQUIRE(stop.rightOpacity == 0.0f);
    }
}

TEST_CASE("A fresh Gradient defaults to unlinked channels", "[core][gradient]") {
    const Gradient gradient;
    REQUIRE_FALSE(gradient.linkChannels());
}

TEST_CASE("linkChannels can be toggled", "[core][gradient]") {
    Gradient gradient;
    gradient.setLinkChannels(true);
    REQUIRE(gradient.linkChannels());
    gradient.setLinkChannels(false);
    REQUIRE_FALSE(gradient.linkChannels());
}

TEST_CASE("evaluate() interpolates linearly between two stops", "[core][gradient]") {
    Gradient gradient;
    GradientStop end;
    end.t = 1.0f;
    end.leftIntensity = 1.0f;
    end.rightIntensity = 1.0f;
    end.leftOpacity = 1.0f;
    end.rightOpacity = 1.0f;
    gradient.setStopValues(1, end);

    const GradientStop mid = gradient.evaluate(0.5f);
    REQUIRE(mid.leftIntensity == 0.5f);
    REQUIRE(mid.rightIntensity == 0.5f);
    REQUIRE(mid.leftOpacity == 0.5f);
    REQUIRE(mid.rightOpacity == 0.5f);
}

TEST_CASE("evaluate() clamps t to [0, 1]", "[core][gradient]") {
    Gradient gradient;
    GradientStop end;
    end.t = 1.0f;
    end.leftIntensity = 1.0f;
    gradient.setStopValues(1, end);

    REQUIRE(gradient.evaluate(-5.0f).leftIntensity == 0.0f);
    REQUIRE(gradient.evaluate(5.0f).leftIntensity == 1.0f);
}

TEST_CASE("insertStop adds an interior stop matching the gradient's existing shape at that point",
          "[core][gradient]") {
    Gradient gradient;
    GradientStop end;
    end.t = 1.0f;
    end.leftIntensity = 1.0f;
    gradient.setStopValues(1, end);

    const std::size_t index = gradient.insertStop(0.5f);
    REQUIRE(gradient.stops().size() == 3);
    REQUIRE(index == 1);
    REQUIRE(gradient.stops()[1].t == 0.5f);
    REQUIRE(gradient.stops()[1].leftIntensity == 0.5f);
}

TEST_CASE("insertStop clamps a t at or beyond either endpoint to stay a genuine interior stop",
          "[core][gradient]") {
    Gradient gradient;
    const std::size_t indexAtZero = gradient.insertStop(0.0f);
    REQUIRE(indexAtZero == 1);
    REQUIRE(gradient.stops()[indexAtZero].t > 0.0f);

    const std::size_t indexAtOne = gradient.insertStop(1.0f);
    REQUIRE(gradient.stops()[indexAtOne].t < 1.0f);
}

TEST_CASE("insertStop keeps stops sorted by t regardless of insertion order", "[core][gradient]") {
    Gradient gradient;
    gradient.insertStop(0.75f);
    gradient.insertStop(0.25f);

    const auto& stops = gradient.stops();
    REQUIRE(stops.size() == 4);
    for (std::size_t i = 0; i + 1 < stops.size(); ++i) {
        REQUIRE(stops[i].t <= stops[i + 1].t);
    }
}

TEST_CASE("removeStop removes a real interior stop", "[core][gradient]") {
    Gradient gradient;
    gradient.insertStop(0.5f);
    REQUIRE(gradient.stops().size() == 3);

    REQUIRE(gradient.removeStop(1));
    REQUIRE(gradient.stops().size() == 2);
}

TEST_CASE("removeStop refuses to remove either endpoint", "[core][gradient]") {
    Gradient gradient;
    gradient.insertStop(0.5f);

    REQUIRE_FALSE(gradient.removeStop(0));
    REQUIRE_FALSE(gradient.removeStop(2));
    REQUIRE(gradient.stops().size() == 3);
}

TEST_CASE("removeStop refuses an out-of-range index", "[core][gradient]") {
    Gradient gradient;
    REQUIRE_FALSE(gradient.removeStop(99));
}

TEST_CASE("setStopValues replaces a stop's values without moving it", "[core][gradient]") {
    Gradient gradient;
    GradientStop newValues;
    newValues.t = 0.9f;  // ignored - see this method's own docs.
    newValues.leftIntensity = 0.75f;
    REQUIRE(gradient.setStopValues(0, newValues));

    REQUIRE(gradient.stops()[0].t == 0.0f);  // unchanged.
    REQUIRE(gradient.stops()[0].leftIntensity == 0.75f);
}

TEST_CASE("setStopValues refuses an out-of-range index", "[core][gradient]") {
    Gradient gradient;
    REQUIRE_FALSE(gradient.setStopValues(99, GradientStop{}));
}

TEST_CASE("A Gradient round-trips through JSON", "[core][gradient]") {
    Gradient gradient;
    gradient.insertStop(0.5f);
    gradient.setLinkChannels(true);
    GradientStop values;
    values.leftIntensity = 0.3f;
    values.rightOpacity = 0.6f;
    gradient.setStopValues(1, values);

    const nlohmann::json json = gradient;
    const Gradient roundTripped = json.get<Gradient>();

    REQUIRE(roundTripped.stops().size() == gradient.stops().size());
    REQUIRE(roundTripped.linkChannels() == gradient.linkChannels());
    REQUIRE(roundTripped.stops()[1].leftIntensity == 0.3f);
    REQUIRE(roundTripped.stops()[1].rightOpacity == 0.6f);
}
