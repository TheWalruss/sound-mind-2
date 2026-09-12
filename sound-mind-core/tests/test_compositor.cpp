#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/core/compositor.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::codec::StreamImage;
using sound_mind::codec::toRgbImage;
using sound_mind::core::compositeProject;
using sound_mind::core::Layer;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::renderLayer;
using sound_mind::core::streamCodecConfigFor;

TEST_CASE("renderLayer returns nullopt for a layer with no content", "[core][compositor]") {
    const Layer layer(1, "Untitled", LayerType::Normal);
    CHECK_FALSE(renderLayer(layer, 2).has_value());
}

TEST_CASE("renderLayer renders a layer's content the same way toRgbImage does when canvasWidth "
          "matches the content and the layer is untranslated/unrescaled",
          "[core][compositor]") {
    StreamImage content;
    content.config.binCount = 2;
    content.frameCount = 2;
    content.leftMagnitudeDb = {0.0f, -50.0f, -30.0f, -96.0f};
    content.rightMagnitudeDb = {-10.0f, -60.0f, -40.0f, -96.0f};
    content.sharedPhaseRadians = {0.0f, 1.0f, -1.0f, 2.0f};

    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(content);

    const auto rendered = renderLayer(layer, /*canvasWidth=*/2);

    REQUIRE(rendered.has_value());
    const auto expected = toRgbImage(content);
    CHECK(rendered->width == expected.width);
    CHECK(rendered->height == expected.height);
    CHECK(rendered->pixels == expected.pixels);
}

namespace {

/// @brief A single-bin, per-column-distinctive StreamImage: column `i`
/// renders as a grayscale-ish value derived from `i`, so shifting/scaling
/// its columns around is easy to check pixel-by-pixel below. Column 0
/// (0 dB) is brightest; each later column is 20 dB quieter.
StreamImage makeGradientContent(std::uint32_t columnCount) {
    StreamImage content;
    content.config.binCount = 1;
    content.frameCount = columnCount;
    content.leftMagnitudeDb.resize(columnCount);
    content.rightMagnitudeDb.resize(columnCount);
    content.sharedPhaseRadians.assign(columnCount, 0.0f);
    for (std::uint32_t i = 0; i < columnCount; ++i) {
        content.leftMagnitudeDb[i] = -static_cast<float>(i) * 20.0f;
        content.rightMagnitudeDb[i] = -static_cast<float>(i) * 20.0f;
    }
    return content;
}

/// @brief Whether column `x` of `image` is pure black (the padding color
/// renderLayer() uses for columns outside the layer's own content).
bool isBlackColumn(const sound_mind::codec::RgbImage& image, std::uint32_t x) {
    for (std::uint32_t y = 0; y < image.height; ++y) {
        const std::size_t index = (std::size_t{y} * image.width + x) * 3;
        if (image.pixels[index] != 0 || image.pixels[index + 1] != 0 || image.pixels[index + 2] != 0) {
            return false;
        }
    }
    return true;
}

/// @brief A small, single-bin project - matching this file's own
/// single-bin StreamImage fixtures above, so compositeProject()'s own
/// output binCount/frameCount are trivial to index into.
ProjectSettings testSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 3;
    settings.binCount = 1;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 2000.0f;
    return settings;
}

/// @brief A single-bin, `frameCount`-wide StreamImage with the given
/// per-column left/right dB amplitude and (shared) phase - a more
/// direct fixture builder than makeGradientContent() above for
/// compositeProject()'s own tests, which need to control amplitude/phase
/// precisely rather than just needing per-column-distinctive content.
StreamImage makeContent(std::vector<float> leftDb, std::vector<float> rightDb, std::vector<float> phase) {
    StreamImage content;
    content.config.binCount = 1;
    content.frameCount = static_cast<std::uint32_t>(leftDb.size());
    content.leftMagnitudeDb = std::move(leftDb);
    content.rightMagnitudeDb = std::move(rightDb);
    content.sharedPhaseRadians = std::move(phase);
    return content;
}

}  // namespace

TEST_CASE("renderLayer pads a layer narrower than canvasWidth with black on the right",
          "[core][compositor]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(makeGradientContent(2));

    const auto rendered = renderLayer(layer, /*canvasWidth=*/5);

    REQUIRE(rendered.has_value());
    CHECK(rendered->width == 5);
    const auto expected = toRgbImage(makeGradientContent(2));
    CHECK(rendered->pixels[0] == expected.pixels[0]);
    CHECK(rendered->pixels[3] == expected.pixels[3]);  // column 1
    CHECK(isBlackColumn(*rendered, 2));
    CHECK(isBlackColumn(*rendered, 3));
    CHECK(isBlackColumn(*rendered, 4));
}

TEST_CASE("renderLayer crops a layer wider than canvasWidth", "[core][compositor]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(makeGradientContent(5));

    const auto rendered = renderLayer(layer, /*canvasWidth=*/2);

    REQUIRE(rendered.has_value());
    CHECK(rendered->width == 2);
    const auto expected = toRgbImage(makeGradientContent(5));
    CHECK(rendered->pixels[0] == expected.pixels[0]);  // column 0, unchanged
    CHECK(rendered->pixels[3] == expected.pixels[3]);  // column 1, unchanged
}

TEST_CASE("renderLayer's positive translationColumns shifts content later (right), padding the "
          "start with black",
          "[core][compositor]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(makeGradientContent(3));
    layer.setTranslationColumns(2);

    const auto rendered = renderLayer(layer, /*canvasWidth=*/5);

    REQUIRE(rendered.has_value());
    const auto expected = toRgbImage(makeGradientContent(3));
    CHECK(isBlackColumn(*rendered, 0));
    CHECK(isBlackColumn(*rendered, 1));
    CHECK(rendered->pixels[2 * 3] == expected.pixels[0]);  // source column 0 now at x=2
    CHECK(rendered->pixels[3 * 3] == expected.pixels[3]);  // source column 1 now at x=3
    CHECK(rendered->pixels[4 * 3] == expected.pixels[6]);  // source column 2 now at x=4
}

TEST_CASE("renderLayer's negative translationColumns shifts content earlier (left), dropping "
          "leading columns and padding the end with black",
          "[core][compositor]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(makeGradientContent(3));
    layer.setTranslationColumns(-1);

    const auto rendered = renderLayer(layer, /*canvasWidth=*/3);

    REQUIRE(rendered.has_value());
    const auto expected = toRgbImage(makeGradientContent(3));
    CHECK(rendered->pixels[0] == expected.pixels[3]);  // source column 1 now at x=0
    CHECK(rendered->pixels[3] == expected.pixels[6]);  // source column 2 now at x=1
    CHECK(isBlackColumn(*rendered, 2));                // source column 3 doesn't exist
}

TEST_CASE("renderLayer's rescaleFactor stretches the layer's own timeline before placement",
          "[core][compositor]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(makeGradientContent(2));
    layer.setRescaleFactor(2.0);  // 2 columns -> 4, each source column doubled up.

    const auto rendered = renderLayer(layer, /*canvasWidth=*/4);

    REQUIRE(rendered.has_value());
    CHECK(rendered->width == 4);
    const auto expected = toRgbImage(makeGradientContent(2));
    CHECK(rendered->pixels[0 * 3] == expected.pixels[0]);  // still source column 0
    CHECK(rendered->pixels[1 * 3] == expected.pixels[0]);  // still source column 0 (stretched)
    CHECK(rendered->pixels[2 * 3] == expected.pixels[3]);  // now source column 1
    CHECK(rendered->pixels[3 * 3] == expected.pixels[3]);  // still source column 1 (stretched)
}

TEST_CASE("renderLayer's rescaleFactor compresses the layer's own timeline before placement",
          "[core][compositor]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(makeGradientContent(4));
    layer.setRescaleFactor(0.5);  // 4 columns -> 2.

    const auto rendered = renderLayer(layer, /*canvasWidth=*/4);

    REQUIRE(rendered.has_value());
    CHECK(rendered->width == 4);
    CHECK(isBlackColumn(*rendered, 2));  // only 2 real columns now - the rest is padding.
    CHECK(isBlackColumn(*rendered, 3));
}

TEST_CASE("compositeProject returns nullopt when no layer has any content", "[core][compositor]") {
    const Project project = Project::createNew(testSettings());  // Background only, no content set.
    CHECK_FALSE(compositeProject(project).has_value());
}

TEST_CASE("compositeProject returns nullopt when the only layer with content is hidden", "[core][compositor]") {
    Project project = Project::createNew(testSettings());
    project.layers()[0].setContent(makeContent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}));
    project.layers()[0].setVisible(false);

    CHECK_FALSE(compositeProject(project).has_value());
}

TEST_CASE("compositeProject reproduces a single full-opacity layer's own content exactly",
          "[core][compositor]") {
    Project project = Project::createNew(testSettings());
    project.layers()[0].setContent(makeContent({-6.0f, -20.0f, -96.0f}, {-3.0f, -40.0f, -96.0f}, {0.5f, -1.0f, 0.0f}));

    const auto composite = compositeProject(project);

    REQUIRE(composite.has_value());
    CHECK(composite->leftMagnitudeDb[0] == Catch::Approx(-6.0f).margin(0.01));
    CHECK(composite->leftMagnitudeDb[1] == Catch::Approx(-20.0f).margin(0.01));
    CHECK(composite->rightMagnitudeDb[0] == Catch::Approx(-3.0f).margin(0.01));
    CHECK(composite->rightMagnitudeDb[1] == Catch::Approx(-40.0f).margin(0.01));
    CHECK(composite->sharedPhaseRadians[0] == Catch::Approx(0.5f).margin(0.001));
    CHECK(composite->sharedPhaseRadians[1] == Catch::Approx(-1.0f).margin(0.001));
}

TEST_CASE("compositeProject scales a layer's amplitude by its own opacity, as a linear gain",
          "[core][compositor]") {
    Project project = Project::createNew(testSettings());
    project.layers()[0].setContent(makeContent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}));
    project.layers()[0].setOpacity(0.5f);

    const auto composite = compositeProject(project);

    REQUIRE(composite.has_value());
    const float halfGainDb = 20.0f * std::log10(0.5f);  // ~ -6.02 dB - half the linear amplitude.
    CHECK(composite->leftMagnitudeDb[0] == Catch::Approx(halfGainDb).margin(0.01));
}

TEST_CASE("compositeProject sums two full-opacity overlapping layers, mixing rather than muting",
          "[core][compositor]") {
    Project project = Project::createNew(testSettings());
    const float halfDb = 20.0f * std::log10(0.5f);  // ~ -6.02 dB - half the linear amplitude.
    project.layers()[0].setContent(makeContent({halfDb, halfDb, halfDb}, {halfDb, halfDb, halfDb}, {0.0f, 0.0f, 0.0f}));
    Layer second(0, "Second", LayerType::Normal);
    second.setContent(makeContent({halfDb, halfDb, halfDb}, {halfDb, halfDb, halfDb}, {0.0f, 0.0f, 0.0f}));
    project.addLayer(std::move(second));

    const auto composite = compositeProject(project);

    // Two layers each at half linear amplitude (same phase) sum to full
    // linear amplitude (0 dB) - image alpha-over would instead leave the
    // top layer's own -6 dB unchanged, since both are equally "opaque".
    REQUIRE(composite.has_value());
    CHECK(composite->leftMagnitudeDb[0] == Catch::Approx(0.0f).margin(0.01));
    CHECK(composite->rightMagnitudeDb[0] == Catch::Approx(0.0f).margin(0.01));
}

TEST_CASE("compositeProject skips a hidden layer's own contribution", "[core][compositor]") {
    Project project = Project::createNew(testSettings());
    project.layers()[0].setContent(makeContent({-10.0f, -10.0f, -10.0f}, {-10.0f, -10.0f, -10.0f}, {0.0f, 0.0f, 0.0f}));
    Layer loud(0, "Loud", LayerType::Normal);
    loud.setContent(makeContent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}));
    loud.setVisible(false);
    project.addLayer(std::move(loud));

    const auto composite = compositeProject(project);

    REQUIRE(composite.has_value());
    CHECK(composite->leftMagnitudeDb[0] == Catch::Approx(-10.0f).margin(0.01));
}

TEST_CASE("compositeProject skips a layer with no content, without crashing", "[core][compositor]") {
    Project project = Project::createNew(testSettings());
    project.layers()[0].setContent(makeContent({-10.0f, -10.0f, -10.0f}, {-10.0f, -10.0f, -10.0f}, {0.0f, 0.0f, 0.0f}));
    project.addLayer(Layer(0, "Empty", LayerType::Normal));  // No content set at all.

    const auto composite = compositeProject(project);

    REQUIRE(composite.has_value());
    CHECK(composite->leftMagnitudeDb[0] == Catch::Approx(-10.0f).margin(0.01));
}

TEST_CASE("compositeProject respects a layer's own translationColumns when placing it",
          "[core][compositor]") {
    Project project = Project::createNew(testSettings());  // canvasWidth = 3.
    project.layers()[0].setContent(makeContent({0.0f, -20.0f}, {0.0f, -20.0f}, {0.0f, 0.0f}));
    project.layers()[0].setTranslationColumns(1);

    const auto composite = compositeProject(project);

    REQUIRE(composite.has_value());
    // Source column 0 (0 dB) now sits at output column 1; output column 0
    // has nothing placed there - pure silence (the floor dB value).
    CHECK(composite->leftMagnitudeDb[1] == Catch::Approx(0.0f).margin(0.01));
    CHECK(composite->leftMagnitudeDb[2] == Catch::Approx(-20.0f).margin(0.01));
    CHECK(composite->leftMagnitudeDb[0] < -100.0f);  // silence floor, not 0 dB.
}

TEST_CASE("compositeProject respects a layer's own rescaleFactor when placing it", "[core][compositor]") {
    Project project = Project::createNew(testSettings());  // canvasWidth = 3.
    project.layers()[0].setContent(makeContent({0.0f}, {0.0f}, {0.0f}));
    project.layers()[0].setRescaleFactor(3.0);  // 1 column -> 3, stretched to fill the whole canvas.

    const auto composite = compositeProject(project);

    REQUIRE(composite.has_value());
    CHECK(composite->leftMagnitudeDb[0] == Catch::Approx(0.0f).margin(0.01));
    CHECK(composite->leftMagnitudeDb[1] == Catch::Approx(0.0f).margin(0.01));
    CHECK(composite->leftMagnitudeDb[2] == Catch::Approx(0.0f).margin(0.01));
}

TEST_CASE("compositeProject always produces exactly canvasWidth columns, regardless of layer width",
          "[core][compositor]") {
    Project project = Project::createNew(testSettings());  // canvasWidth = 3.
    project.layers()[0].setContent(makeContent({0.0f}, {0.0f}, {0.0f}));  // Only 1 column of real content.

    const auto composite = compositeProject(project);

    REQUIRE(composite.has_value());
    CHECK(composite->frameCount == 3);
    CHECK(composite->leftMagnitudeDb.size() == std::size_t{3});
    CHECK(composite->leftMagnitudeDb[1] < -100.0f);  // padding, not the source column's own 0 dB.
}

TEST_CASE("compositeProject's own sampleCount is canvasWidth frames' worth of samples",
          "[core][compositor]") {
    Project project = Project::createNew(testSettings());
    project.layers()[0].setContent(makeContent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}));

    const auto composite = compositeProject(project);

    REQUIRE(composite.has_value());
    const auto config = streamCodecConfigFor(testSettings());
    CHECK(composite->sampleCount == static_cast<std::uint64_t>(testSettings().canvasWidth) * config.hopLength);
}

TEST_CASE("compositeProject's own binCount is the tallest among the contributing layers' own content",
          "[core][compositor]") {
    // Reproduces a real bug found in v0.Y.27.1's own Installment B: a
    // layer's cached content can have a different binCount than the
    // project's own current settings declare (settings.binCount defaults
    // to 512, but a hand-built test fixture - or a project reconfigured
    // since a layer was last encoded - can easily leave a layer with far
    // fewer) - compositeProject() used to index every layer's own arrays
    // using the *project's* bin count unconditionally, reading past a
    // narrower layer's own end. Also confirms a shorter layer's own bins
    // beyond its own range contribute nothing (silently, not a crash) to
    // a bin only a *taller* layer actually reaches.
    Project project = Project::createNew(testSettings());  // canvasWidth = 3.
    project.layers()[0].setContent(makeContent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}));  // 1 bin.

    StreamImage tallerContent;
    tallerContent.config.binCount = 3;
    tallerContent.frameCount = 3;
    tallerContent.leftMagnitudeDb.assign(9, -50.0f);
    tallerContent.rightMagnitudeDb.assign(9, -50.0f);
    tallerContent.sharedPhaseRadians.assign(9, 0.0f);
    Layer taller(0, "Taller", LayerType::Normal);
    taller.setContent(tallerContent);
    project.addLayer(std::move(taller));

    const auto composite = compositeProject(project);

    REQUIRE(composite.has_value());
    CHECK(composite->config.binCount == 3);  // The taller layer's own binCount, not settings.binCount (512).
    // Bin 0: both layers contribute - the -50 dB layer barely moves the
    // Background layer's own 0 dB.
    CHECK(composite->leftMagnitudeDb[0] > -1.0f);
    // Bin 1 (output cell 1*3 = 3): only the taller layer reaches here -
    // the shorter layer's own (nonexistent) bin 1 contributes nothing,
    // read without crashing.
    CHECK(composite->leftMagnitudeDb[3] == Catch::Approx(-50.0f).margin(0.5f));
}
