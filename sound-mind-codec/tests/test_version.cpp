#include <catch2/catch_test_macros.hpp>

#include "sound_mind/codec/version.h"

TEST_CASE("version_string reports the expected placeholder version", "[codec][toolchain]") {
    REQUIRE(sound_mind::codec::version_string() == "0.1.0");
}
