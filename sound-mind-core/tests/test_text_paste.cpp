#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "sound_mind/core/operation_clone.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paste_application.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/text_paste.h"

using sound_mind::core::BlendMode;
using sound_mind::core::GlyphRaster;
using sound_mind::core::LayerId;
using sound_mind::core::makeTextPaste;
using sound_mind::core::OperationId;

namespace {

sound_mind::codec::StreamCodecConfig testConfig() {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 20;
    settings.binCount = 16;
    return sound_mind::core::streamCodecConfigFor(settings);
}

/// @brief A 3x2 raster: top row "full, empty, half", bottom row empty.
GlyphRaster smallRaster() {
    GlyphRaster raster;
    raster.width = 3;
    raster.height = 2;
    raster.coverage = {1.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f};
    return raster;
}

sound_mind::codec::StreamImage blankContent(const sound_mind::codec::StreamCodecConfig& config) {
    sound_mind::codec::StreamImage content;
    content.config = config;
    content.frameCount = 20;
    const std::size_t cells = std::size_t{config.binCount} * content.frameCount;
    content.leftMagnitudeDb.assign(cells, -50.0f);
    content.rightMagnitudeDb.assign(cells, -50.0f);
    content.sharedPhaseRadians.assign(cells, 0.0f);
    return content;
}

}  // namespace

TEST_CASE("makeTextPaste writes glyph cells only, fading edges by coverage", "[core][text_paste]") {
    const auto config = testConfig();
    const auto paste = makeTextPaste(OperationId{1}, LayerId{2}, smallRaster(), 5, 4, -10.0f, -20.0f, 1.0f,
                                     BlendMode::Overwrite, config);
    REQUIRE(paste != nullptr);
    CHECK(paste->targetLayer() == LayerId{2});
    CHECK(paste->clip().frameCount == 3);
    CHECK(paste->clip().binCount == 2);

    auto content = blankContent(config);
    sound_mind::core::applyPasteOperation(*paste, content);

    using sound_mind::core::cellIndex;
    // The raster's top row lands on the higher bin (4 + 1 = 5).
    CHECK(content.leftMagnitudeDb[cellIndex(5u, 5u, 20)] == Catch::Approx(-10.0f).margin(0.01));
    CHECK(content.rightMagnitudeDb[cellIndex(5u, 5u, 20)] == Catch::Approx(-20.0f).margin(0.01));
    CHECK(content.leftMagnitudeDb[cellIndex(5u, 7u, 20)] == Catch::Approx(-10.0f - 6.02f).margin(0.05));
    // Uncovered cells are left exactly as they were, even with Overwrite.
    CHECK(content.leftMagnitudeDb[cellIndex(5u, 6u, 20)] == -50.0f);
    CHECK(content.leftMagnitudeDb[cellIndex(4u, 5u, 20)] == -50.0f);
    CHECK(content.leftMagnitudeDb[cellIndex(5u, 8u, 20)] == -50.0f);
}

TEST_CASE("makeTextPaste scales by opacity and places its bounds on the covered cells", "[core][text_paste]") {
    const auto config = testConfig();
    const auto paste = makeTextPaste(OperationId{1}, LayerId{2}, smallRaster(), 5, 4, -10.0f, -10.0f, 0.5f,
                                     BlendMode::Overwrite, config);
    REQUIRE(paste != nullptr);
    auto content = blankContent(config);
    sound_mind::core::applyPasteOperation(*paste, content);
    CHECK(content.leftMagnitudeDb[sound_mind::core::cellIndex(5u, 5u, 20)] == Catch::Approx(-16.02f).margin(0.05));

    const auto bounds = paste->bounds();
    CHECK(sound_mind::core::timeToFrameIndex(bounds.startTimeSeconds, config) == Catch::Approx(5.0).margin(1e-6));
    CHECK(sound_mind::core::timeToFrameIndex(bounds.endTimeSeconds, config) == Catch::Approx(7.0).margin(1e-6));
    CHECK(sound_mind::core::frequencyToBinIndex(static_cast<float>(bounds.lowFrequencyHz), config) ==
          Catch::Approx(4.0f).margin(1e-3));
}

TEST_CASE("makeTextPaste returns nothing for empty or invisible text", "[core][text_paste]") {
    const auto config = testConfig();
    CHECK(makeTextPaste(OperationId{1}, LayerId{2}, GlyphRaster{}, 0, 0, -10.0f, -10.0f, 1.0f, BlendMode::Normal,
                        config) == nullptr);
    GlyphRaster faint = smallRaster();
    CHECK(makeTextPaste(OperationId{1}, LayerId{2}, faint, 0, 0, -10.0f, -10.0f, 0.0f, BlendMode::Normal, config) ==
          nullptr);
}

// v0.1.40.13 - applied text keeps what it was rendered from.
TEST_CASE("A text paste keeps its source through moves, copies and saving", "[core][text_paste]") {
    const auto config = testConfig();
    sound_mind::core::TextSource source;
    source.text = "Hi";
    source.fontFamily = "Arial";
    source.fontSize = 12;
    source.bold = true;
    source.leftIntensityDb = -10.0f;
    source.rightIntensityDb = -14.0f;
    source.opacity = 0.75f;
    source.rotationDegrees = 15.0;
    auto paste = makeTextPaste(OperationId{1}, LayerId{2}, smallRaster(), 5, 4, -10.0f, -14.0f, 0.75f,
                               BlendMode::Normal, config, source, OperationId{0});
    REQUIRE(paste != nullptr);
    REQUIRE(paste->textSource().has_value());
    CHECK(*paste->textSource() == source);
    CHECK(paste->supersedes() == OperationId{0});

    // Its box comes back exactly: 3 x 2 cells at (5, 4).
    const auto box = sound_mind::core::textPasteBox(*paste, config);
    CHECK(box.frameLow == 5);
    CHECK(box.binLow == 4);
    CHECK(box.width == 3);
    CHECK(box.height == 2);

    const auto moved = paste->translatedCopy(OperationId{3}, 0.0, 2.0, config);
    const auto* movedPaste = dynamic_cast<const sound_mind::core::PasteOperation*>(moved.get());
    REQUIRE(movedPaste != nullptr);
    CHECK(movedPaste->textSource() == paste->textSource());
    CHECK(sound_mind::core::textPasteBox(*movedPaste, config).binLow == 6);  // the box moves with it.

    const auto copied = sound_mind::core::cloneOperation(*paste, OperationId{4}, LayerId{2});
    CHECK(dynamic_cast<const sound_mind::core::PasteOperation&>(*copied).textSource() == paste->textSource());

    sound_mind::core::OperationLog log;
    log.append(std::move(paste));
    const nlohmann::json json = log;
    const auto restored = json.get<sound_mind::core::OperationLog>();
    const auto& restoredPaste = dynamic_cast<const sound_mind::core::PasteOperation&>(restored.at(0));
    REQUIRE(restoredPaste.textSource().has_value());
    CHECK(*restoredPaste.textSource() == source);

    // An ordinary paste has none.
    CHECK(!makeTextPaste(OperationId{5}, LayerId{2}, smallRaster(), 0, 0, -10.0f, -10.0f, 1.0f, BlendMode::Normal,
                         config)
               ->textSource()
               .has_value());
}

