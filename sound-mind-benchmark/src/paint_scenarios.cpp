#include "sound_mind/benchmark/scenarios.h"

#include <array>
#include <memory>
#include <string>

#include "sound_mind/core/operation.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/tool_configuration.h"

namespace sound_mind::benchmark {

namespace {

using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::StreamImage;
using sound_mind::core::applyPaintOperation;
using sound_mind::core::Clip;
using sound_mind::core::frequencyToTimeScaleFor;
using sound_mind::core::HealConfiguration;
using sound_mind::core::InstrumentConfiguration;
using sound_mind::core::LayerId;
using sound_mind::core::MindGrainConfiguration;
using sound_mind::core::MindShotConfiguration;
using sound_mind::core::OrderChaosConfiguration;
using sound_mind::core::PaintOperation;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::ProceduralConfiguration;
using sound_mind::core::ProjectSettings;
using sound_mind::core::ResonanceConfiguration;
using sound_mind::core::SmudgeConfiguration;
using sound_mind::core::SoftenConfiguration;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::core::TimeFrequencyRect;
using sound_mind::core::ToolConfiguration;

struct CanvasSize {
    const char* label;
    std::uint32_t binCount;
    std::uint32_t frameCount;
};

constexpr CanvasSize kMediumCanvas{"medium", 512, 2000};
constexpr std::array<CanvasSize, 3> kCanvasSweep{{
    {"small", 128, 500},
    {"medium", 512, 2000},
    {"large", 1024, 8000},
}};

StreamCodecConfig codecConfigFor(const CanvasSize& size) {
    StreamCodecConfig config;
    config.binCount = size.binCount;
    config.minFrequencyHz = 20.0f;
    config.maxFrequencyHz = 20000.0f;
    return config;
}

ProjectSettings projectSettingsFor(const CanvasSize& size) {
    ProjectSettings settings;
    settings.binCount = size.binCount;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 20000.0f;
    settings.canvasWidth = size.frameCount;
    settings.canvasHeight = size.binCount;
    return settings;
}

StreamImage blankContent(const CanvasSize& size) {
    StreamImage image;
    image.config = codecConfigFor(size);
    image.frameCount = size.frameCount;
    const auto cellCount = static_cast<std::size_t>(size.binCount) * size.frameCount;
    image.leftMagnitudeDb.assign(cellCount, -40.0f);
    image.rightMagnitudeDb.assign(cellCount, -40.0f);
    image.sharedPhaseRadians.assign(cellCount, 0.0f);
    return image;
}

/// @brief A diagonal stroke spanning most of `settings`' own encoded
/// range, `lengthFraction` of the way across the time axis - a uniform
/// gradient (constant intensity/opacity) so every stamp along it blends
/// toward the same fixed target regardless of where it lands, keeping
/// repeated timed calls statistically consistent (see
/// `BenchmarkCase::run`'s own docs on why that matters).
Path makeStroke(const ProjectSettings& settings, double startTimeFraction, double lengthFraction) {
    Path path;
    const double durationSeconds = settings.canvasWidth * (settings.timestepMs / 1000.0);
    const double startTime = durationSeconds * startTimeFraction;
    const double endTime = durationSeconds * (startTimeFraction + lengthFraction);
    const double lowFrequency = settings.minFrequencyHz * 4.0;
    const double highFrequency = settings.maxFrequencyHz * 0.5;

    PathNode startNode;
    startNode.anchor = TimeFrequencyPoint{startTime, lowFrequency};
    startNode.type = PathNodeType::Corner;
    path.addNode(startNode);

    PathNode endNode;
    endNode.anchor = TimeFrequencyPoint{endTime, highFrequency};
    endNode.type = PathNodeType::Corner;
    path.addNode(endNode);

    auto stop = path.gradient().stops().front();
    stop.leftIntensity = -6.0f;
    stop.rightIntensity = -6.0f;
    stop.leftOpacity = 0.6f;
    stop.rightOpacity = 0.6f;
    path.gradient().setStopValues(0, stop);
    path.gradient().setStopValues(1, stop);
    return path;
}

/// @brief A single `BenchmarkCase` that applies one stroke, with
/// `toolConfig`, onto a shared mutating canvas of `canvas`'s own size.
BenchmarkCase makeSingleStrokeCase(std::string name, const CanvasSize& canvas,
                                    std::unique_ptr<ToolConfiguration> toolConfig) {
    auto settings = std::make_shared<ProjectSettings>(projectSettingsFor(canvas));
    auto content = std::make_shared<StreamImage>(blankContent(canvas));
    const double frequencyToTimeScale = frequencyToTimeScaleFor(*settings);
    auto path = std::make_shared<Path>(makeStroke(*settings, 0.1, 0.8));
    auto operation = std::make_shared<PaintOperation>(1, LayerId{1}, *path, std::move(toolConfig));

    nlohmann::json parameters;
    parameters["canvas"] = canvas.label;
    parameters["binCount"] = canvas.binCount;
    parameters["frameCount"] = canvas.frameCount;

    return BenchmarkCase{std::move(name), "paint", std::move(parameters), false,
                          [operation, frequencyToTimeScale, content]() {
                              applyPaintOperation(*operation, frequencyToTimeScale, *content);
                          }};
}

void appendOneBaselineCasePerToolType(std::vector<BenchmarkCase>& cases) {
    auto procedural = std::make_unique<ProceduralConfiguration>();
    cases.push_back(makeSingleStrokeCase("Procedural", kMediumCanvas, std::move(procedural)));

    cases.push_back(makeSingleStrokeCase("Instrument", kMediumCanvas, std::make_unique<InstrumentConfiguration>()));

    auto resonance = std::make_unique<ResonanceConfiguration>();
    resonance->setSpectrum(std::nullopt, {1.0f, 0.8f, 0.5f, 0.3f, 0.1f});
    cases.push_back(makeSingleStrokeCase("Resonance", kMediumCanvas, std::move(resonance)));

    cases.push_back(makeSingleStrokeCase("Heal", kMediumCanvas, std::make_unique<HealConfiguration>()));
    cases.push_back(makeSingleStrokeCase("Soften", kMediumCanvas, std::make_unique<SoftenConfiguration>()));
    cases.push_back(makeSingleStrokeCase("Smudge", kMediumCanvas, std::make_unique<SmudgeConfiguration>()));

    auto orderChaos = std::make_unique<OrderChaosConfiguration>();
    orderChaos->setAmount(0.5);
    cases.push_back(makeSingleStrokeCase("OrderChaos", kMediumCanvas, std::move(orderChaos)));

    // MindShot - a small captured Clip, stamped repeatedly along the stroke.
    auto mindShot = std::make_unique<MindShotConfiguration>();
    Clip clip;
    clip.frameCount = 20;
    clip.binCount = 20;
    clip.leftMagnitudeDb.assign(std::size_t{20} * 20, -10.0f);
    clip.rightMagnitudeDb.assign(std::size_t{20} * 20, -10.0f);
    clip.sharedPhaseRadians.assign(std::size_t{20} * 20, 0.0f);
    mindShot->setClip(std::nullopt, std::move(clip));
    cases.push_back(makeSingleStrokeCase("MindShot", kMediumCanvas, std::move(mindShot)));

    // MindGrain - a live reference to another layer's own content, resolved
    // via a fixed synthetic image regardless of which LayerId is asked for
    // (this suite only measures time, never output correctness).
    {
        auto settings = std::make_shared<ProjectSettings>(projectSettingsFor(kMediumCanvas));
        auto content = std::make_shared<StreamImage>(blankContent(kMediumCanvas));
        auto sourceContent = std::make_shared<StreamImage>(blankContent(kMediumCanvas));
        const double frequencyToTimeScale = frequencyToTimeScaleFor(*settings);
        auto path = std::make_shared<Path>(makeStroke(*settings, 0.1, 0.8));

        auto mindGrain = std::make_unique<MindGrainConfiguration>();
        TimeFrequencyRect bounds;
        bounds.startTimeSeconds = 0.0;
        bounds.endTimeSeconds = 2.0;
        bounds.lowFrequencyHz = settings->minFrequencyHz * 4.0;
        bounds.highFrequencyHz = settings->maxFrequencyHz * 0.5;
        mindGrain->setReference(std::nullopt, LayerId{2}, bounds);
        auto operation = std::make_shared<PaintOperation>(1, LayerId{1}, *path, std::move(mindGrain));

        nlohmann::json parameters;
        parameters["canvas"] = kMediumCanvas.label;
        cases.push_back(BenchmarkCase{
            "MindGrain", "paint", parameters, false,
            [operation, frequencyToTimeScale, content, sourceContent]() {
                applyPaintOperation(*operation, frequencyToTimeScale, *content,
                                     [&sourceContent](LayerId) { return sourceContent.get(); });
            }});
    }
}

/// @brief Many small strokes vs. few large strokes, at the same total
/// path length - directly answers "is painting dominated by per-stroke
/// overhead or by total ink", per `docs/sound-mind-benchmarking.md`'s own
/// "Scenario catalog".
void appendStrokeCountSweep(std::vector<BenchmarkCase>& cases) {
    auto settings = std::make_shared<ProjectSettings>(projectSettingsFor(kMediumCanvas));

    for (const int strokeCount : {1, 10, 100}) {
        auto content = std::make_shared<StreamImage>(blankContent(kMediumCanvas));
        const double frequencyToTimeScale = frequencyToTimeScaleFor(*settings);
        const double lengthFraction = 0.8 / static_cast<double>(strokeCount);

        // PaintOperation (via Operation) is deliberately non-copyable -
        // this codebase only ever owns one through a std::unique_ptr (see
        // OperationLog's own docs) - so this holds pointers, not values.
        auto operations = std::make_shared<std::vector<std::unique_ptr<PaintOperation>>>();
        operations->reserve(static_cast<std::size_t>(strokeCount));
        for (int i = 0; i < strokeCount; ++i) {
            const double startFraction = 0.1 + lengthFraction * static_cast<double>(i);
            const Path path = makeStroke(*settings, startFraction, lengthFraction);
            operations->push_back(std::make_unique<PaintOperation>(
                static_cast<sound_mind::core::OperationId>(i + 1), LayerId{1}, path,
                std::make_unique<ProceduralConfiguration>()));
        }

        nlohmann::json parameters;
        parameters["strokeCount"] = strokeCount;
        parameters["totalLengthFraction"] = 0.8;

        cases.push_back(BenchmarkCase{"Procedural strokeCount=" + std::to_string(strokeCount), "paint", parameters,
                                       false, [operations, frequencyToTimeScale, content]() {
                                           for (const auto& operation : *operations) {
                                               applyPaintOperation(*operation, frequencyToTimeScale, *content);
                                           }
                                       }});
    }
}

void appendCanvasSizeSweep(std::vector<BenchmarkCase>& cases) {
    for (const CanvasSize& canvas : kCanvasSweep) {
        cases.push_back(makeSingleStrokeCase("Procedural", canvas, std::make_unique<ProceduralConfiguration>()));
    }
}

}  // namespace

std::vector<BenchmarkCase> buildPaintScenarios() {
    std::vector<BenchmarkCase> cases;
    appendOneBaselineCasePerToolType(cases);
    appendStrokeCountSweep(cases);
    appendCanvasSizeSweep(cases);
    return cases;
}

}  // namespace sound_mind::benchmark
