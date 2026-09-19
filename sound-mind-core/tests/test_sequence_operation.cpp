#include <memory>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/sequence_operation.h"
#include "sound_mind/core/tool_configuration.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::binIndexToFrequency;
using sound_mind::core::frequencyToBinIndex;
using sound_mind::core::LayerId;
using sound_mind::core::NoteEvent;
using sound_mind::core::OperationId;
using sound_mind::core::ProceduralConfiguration;
using sound_mind::core::SequenceOperation;
using sound_mind::core::ToolConfiguration;

namespace {

StreamCodecConfig makeTestConfig() {
    StreamCodecConfig config;
    config.sampleRateHz = 44100;
    config.hopLength = 441;
    config.binCount = 100;
    config.minFrequencyHz = 20.0f;
    config.maxFrequencyHz = 20000.0f;
    return config;
}

std::vector<NoteEvent> makeTestNotes() {
    return {
        NoteEvent{0.5, 1.0, 300.0},
        NoteEvent{1.0, 0.5, 900.0},
        NoteEvent{0.0, 0.25, 500.0},
    };
}

std::unique_ptr<ToolConfiguration> makeToolConfig() { return std::make_unique<ProceduralConfiguration>(); }

}  // namespace

TEST_CASE("NoteEvent round-trips through JSON", "[core][sequence_operation]") {
    const NoteEvent note{1.5, 0.75, 440.0};
    const nlohmann::json json = note;
    const NoteEvent restored = json.get<NoteEvent>();
    REQUIRE(restored.startTimeSeconds == Catch::Approx(1.5));
    REQUIRE(restored.durationSeconds == Catch::Approx(0.75));
    REQUIRE(restored.frequencyHz == Catch::Approx(440.0));
}

TEST_CASE("SequenceOperation reports the id, target layer, notes, and config it was constructed with",
          "[core][sequence_operation]") {
    const SequenceOperation op(7, LayerId{3}, makeTestNotes(), makeToolConfig());
    REQUIRE(op.id() == OperationId{7});
    REQUIRE(op.targetLayer().has_value());
    REQUIRE(op.targetLayer().value() == LayerId{3});
    REQUIRE(op.notes().size() == std::size_t{3});
    REQUIRE(op.notes()[1].frequencyHz == Catch::Approx(900.0));
    REQUIRE(dynamic_cast<const ProceduralConfiguration*>(&op.config()) != nullptr);
}

TEST_CASE("SequenceOperation has no supersedes reference unless one is given", "[core][sequence_operation]") {
    const SequenceOperation op(1, LayerId{1}, makeTestNotes(), makeToolConfig());
    REQUIRE_FALSE(op.supersedes().has_value());
}

TEST_CASE("SequenceOperation can record which prior operation it supersedes", "[core][sequence_operation]") {
    const SequenceOperation op(2, LayerId{1}, makeTestNotes(), makeToolConfig(), OperationId{1});
    REQUIRE(op.supersedes().has_value());
    REQUIRE(op.supersedes().value() == OperationId{1});
}

TEST_CASE("SequenceOperation::bounds() is the union of every note's own time span and frequency",
          "[core][sequence_operation]") {
    const SequenceOperation op(1, LayerId{1}, makeTestNotes(), makeToolConfig());
    const auto bounds = op.bounds();
    // Notes: [0.5, 1.5] @300Hz, [1.0, 1.5] @900Hz, [0.0, 0.25] @500Hz.
    REQUIRE(bounds.startTimeSeconds == Catch::Approx(0.0));
    REQUIRE(bounds.endTimeSeconds == Catch::Approx(1.5));
    REQUIRE(bounds.lowFrequencyHz == Catch::Approx(300.0));
    REQUIRE(bounds.highFrequencyHz == Catch::Approx(900.0));
}

TEST_CASE("SequenceOperation::bounds() is all-zero for a sequence with no notes", "[core][sequence_operation]") {
    const SequenceOperation op(1, LayerId{1}, {}, makeToolConfig());
    const auto bounds = op.bounds();
    REQUIRE(bounds.startTimeSeconds == 0.0);
    REQUIRE(bounds.endTimeSeconds == 0.0);
    REQUIRE(bounds.lowFrequencyHz == 0.0);
    REQUIRE(bounds.highFrequencyHz == 0.0);
}

TEST_CASE("SequenceOperation::translatedCopy() shifts every note's own time and frequency, keeps config, and "
          "supersedes the original",
          "[core][sequence_operation]") {
    auto config = makeToolConfig();
    config->setName("Original");
    const SequenceOperation original(5, LayerId{2}, makeTestNotes(), std::move(config));
    const StreamCodecConfig codecConfig = makeTestConfig();

    const auto copy = original.translatedCopy(OperationId{9}, 0.1, 0.0, codecConfig);

    REQUIRE(copy != nullptr);
    REQUIRE(copy->id() == OperationId{9});
    REQUIRE(copy->supersedes().has_value());
    REQUIRE(*copy->supersedes() == OperationId{5});
    REQUIRE(copy->targetLayer() == LayerId{2});

    const auto* sequenceCopy = dynamic_cast<const SequenceOperation*>(copy.get());
    REQUIRE(sequenceCopy != nullptr);
    REQUIRE(sequenceCopy->notes().size() == std::size_t{3});
    REQUIRE(sequenceCopy->notes()[0].startTimeSeconds == Catch::Approx(0.6));  // 0.5 + 0.1.
    REQUIRE(sequenceCopy->config().name() == "Original");

    // The original is untouched.
    REQUIRE(original.notes()[0].startTimeSeconds == Catch::Approx(0.5));
}

TEST_CASE("SequenceOperation::translatedCopy() shifts frequency via bins, not a raw Hz offset",
          "[core][sequence_operation]") {
    const std::vector<NoteEvent> notes{NoteEvent{0.0, 1.0, 1000.0}};
    const SequenceOperation original(1, LayerId{1}, notes, makeToolConfig());
    const StreamCodecConfig codecConfig = makeTestConfig();

    const auto copy = original.translatedCopy(OperationId{2}, 0.0, 5.0, codecConfig);
    const auto* sequenceCopy = dynamic_cast<const SequenceOperation*>(copy.get());
    REQUIRE(sequenceCopy != nullptr);

    const float expectedBin = frequencyToBinIndex(1000.0f, codecConfig) + 5.0f;
    const float expectedHz = binIndexToFrequency(expectedBin, codecConfig);
    REQUIRE(sequenceCopy->notes()[0].frequencyHz == Catch::Approx(expectedHz));
}
