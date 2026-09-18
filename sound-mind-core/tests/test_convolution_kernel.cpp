#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/convolution_kernel.h"

using sound_mind::core::NamedConvolutionKernel;

TEST_CASE("A fresh NamedConvolutionKernel is a 3x3 identity kernel", "[core][convolution_kernel]") {
    const NamedConvolutionKernel kernel;
    REQUIRE(kernel.size == 3);
    REQUIRE(kernel.coefficients == std::vector<float>{0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    REQUIRE_FALSE(kernel.normalize);
}

TEST_CASE("A NamedConvolutionKernel round-trips through JSON unchanged", "[core][convolution_kernel]") {
    NamedConvolutionKernel original;
    original.id = 7;
    original.name = "Sharpen";
    original.size = 3;
    original.coefficients = {0.0f, -1.0f, 0.0f, -1.0f, 5.0f, -1.0f, 0.0f, -1.0f, 0.0f};
    original.normalize = true;

    const nlohmann::json json = original;
    const auto restored = json.get<NamedConvolutionKernel>();

    REQUIRE(restored.id == original.id);
    REQUIRE(restored.name == original.name);
    REQUIRE(restored.size == original.size);
    REQUIRE(restored.coefficients == original.coefficients);
    REQUIRE(restored.normalize == original.normalize);
}
