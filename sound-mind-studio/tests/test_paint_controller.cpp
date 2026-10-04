#include "test_paint_controller.h"

#include <algorithm>
#include <memory>
#include <numbers>

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/mind_grain.h"
#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/tool_configuration.h"
#include "sound_mind/studio/paint_controller.h"

using sound_mind::core::InstrumentConfiguration;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::MindGrainConfiguration;
using sound_mind::core::MindWave;
using sound_mind::core::PeriodicWaveform;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::core::TimeFrequencyRect;
using sound_mind::studio::PaintController;

namespace {

/// @brief Small, fast project settings - matching test_main_window.cpp's
/// own smallCanvasProjectSettings(), plus a real, distinct frequency
/// range so frequencyToTimeScale() derives a sensible, checkable value.
ProjectSettings testSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 100;
    settings.canvasHeight = 50;
    settings.binCount = 50;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 2020.0f;  // a round 2000 Hz range.
    settings.timestepMs = 10.0;         // 100 * 10ms = 1 second total duration.
    return settings;
}

/// @brief A Normal layer, added to `project`, with real (blank) content -
/// most tests use this rather than relying on PaintController's own
/// lazy silent-base synthesis (see rebuildLayerContent()'s own docs) so
/// the "known starting content" they assert against is exact and
/// predictable, not whatever silentContentFor() happens to produce.
LayerId addBlankNormalLayer(Project& project) {
    Layer layer(0, "Test Layer", LayerType::Normal);
    sound_mind::codec::StreamImage content;
    content.config = sound_mind::core::streamCodecConfigFor(project.settings());
    content.frameCount = project.settings().canvasWidth;
    const std::size_t pixelCount = std::size_t{content.config.binCount} * content.frameCount;
    content.leftMagnitudeDb.assign(pixelCount, 0.0f);
    content.rightMagnitudeDb.assign(pixelCount, 0.0f);
    content.sharedPhaseRadians.assign(pixelCount, 0.0f);
    layer.setContent(content);
    return project.addLayer(std::move(layer));
}

/// @brief A Procedural tool configuration with a real (non-transparent)
/// uniform gradient - a fresh ToolConfiguration's own default gradient is
/// deliberately fully transparent (see ToolConfiguration's own docs), so
/// any test that needs painting to actually leave a visible mark needs
/// one of these instead.
std::unique_ptr<sound_mind::core::ProceduralConfiguration> makeOpaqueTool(double size = 0.05, float falloff = 0.0f,
                                                                            float intensity = -10.0f) {
    auto config = std::make_unique<sound_mind::core::ProceduralConfiguration>();
    config->setSize(size);
    config->setFalloff(falloff);
    auto stop = config->defaultGradient().stops().front();
    stop.leftIntensity = intensity;
    stop.rightIntensity = intensity;
    stop.leftOpacity = 1.0f;
    stop.rightOpacity = 1.0f;
    config->defaultGradient().setStopValues(0, stop);
    config->defaultGradient().setStopValues(1, stop);
    return config;
}

/// @brief A single-harmonic InstrumentConfiguration with a fully opaque
/// gradient - the Instrument counterpart to makeOpaqueTool() above, for
/// vibrato/tremolo resolver tests that need a real, checkable paint result.
std::unique_ptr<InstrumentConfiguration> makeOpaqueInstrumentTool(double size = 0.05, float falloff = 0.0f,
                                                                    float intensity = -10.0f) {
    auto config = std::make_unique<InstrumentConfiguration>();
    config->setHarmonicStrengths({1.0});
    config->setSize(size);
    config->setFalloff(falloff);
    auto stop = config->defaultGradient().stops().front();
    stop.leftIntensity = intensity;
    stop.rightIntensity = intensity;
    stop.leftOpacity = 1.0f;
    stop.rightOpacity = 1.0f;
    config->defaultGradient().setStopValues(0, stop);
    config->defaultGradient().setStopValues(1, stop);
    return config;
}

/// @brief A MindWave that reads as 0.0 (baseline) for any small `t` - see
/// `test_filter_application.cpp`'s own `alwaysBaselineWave()` precedent.
MindWave alwaysBaselineWave() {
    MindWave wave;
    wave.setPeriodicWaveform(PeriodicWaveform::Square);
    wave.setPeriod(1'000'000.0);
    wave.setPhaseRadians(std::numbers::pi_v<double>);
    return wave;
}

}  // namespace

void PaintControllerTest::freshControllerHasNoStrokeInProgress() {
    const PaintController controller;
    QVERIFY(!controller.isStrokeInProgress());
    QVERIFY(controller.currentPreviewPath().nodes().empty());
}

void PaintControllerTest::freshControllerCannotUndoOrRedo() {
    const PaintController controller;
    QVERIFY(!controller.canUndo());
    QVERIFY(!controller.canRedo());
}

void PaintControllerTest::beginStrokeDoesNothingWithNoProjectSet() {
    PaintController controller;
    controller.beginStroke(LayerId{1}, TimeFrequencyPoint{0.0, 100.0});
    QVERIFY(!controller.isStrokeInProgress());
}

void PaintControllerTest::beginStrokeStartsAStroke() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);

    controller.beginStroke(layerId, TimeFrequencyPoint{0.1, 500.0});

    QVERIFY(controller.isStrokeInProgress());
}

void PaintControllerTest::beginStrokeDoesNothingWhileAStrokeIsAlreadyInProgress() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    controller.beginStroke(layerId, TimeFrequencyPoint{0.1, 500.0});

    controller.beginStroke(layerId, TimeFrequencyPoint{0.2, 600.0});  // ignored - a stroke's already in progress.

    controller.continueStroke(TimeFrequencyPoint{0.3, 500.0});
    QVERIFY(controller.currentPreviewPath().nodes().front().anchor.timeSeconds == 0.1);
}

void PaintControllerTest::continueStrokeEmitsPathChangedOnceThereAreTwoPoints() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    controller.beginStroke(layerId, TimeFrequencyPoint{0.1, 500.0});
    QSignalSpy spy(&controller, &PaintController::pathChanged);

    controller.continueStroke(TimeFrequencyPoint{0.2, 500.0});

    QCOMPARE(spy.count(), 1);
    QVERIFY(!controller.currentPreviewPath().nodes().empty());
}

void PaintControllerTest::continueStrokeDoesNothingWithNoStrokeInProgress() {
    PaintController controller;
    QSignalSpy spy(&controller, &PaintController::pathChanged);

    controller.continueStroke(TimeFrequencyPoint{0.1, 500.0});

    QCOMPARE(spy.count(), 0);
}

void PaintControllerTest::endStrokeAppendsAPaintOperationAndEmitsContentChanged() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    controller.beginStroke(layerId, TimeFrequencyPoint{0.1, 500.0});
    controller.continueStroke(TimeFrequencyPoint{0.5, 500.0});
    QSignalSpy spy(&controller, &PaintController::contentChanged);

    controller.endStroke();

    QVERIFY(!controller.isStrokeInProgress());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), layerId);
    QCOMPARE(project.operationLog().size(), std::size_t{1});
}

void PaintControllerTest::endStrokeWithOnlyOnePointStillPaintsATap() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    controller.beginStroke(layerId, TimeFrequencyPoint{0.1, 500.0});

    controller.endStroke();

    QCOMPARE(project.operationLog().size(), std::size_t{1});
}

void PaintControllerTest::endStrokeActuallyChangesTheLayersStoredContent() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    controller.setToolConfiguration(makeOpaqueTool());

    controller.beginStroke(layerId, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    const auto& content = *project.layerById(layerId)->content();
    const bool anyPainted = std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                                         [](float value) { return value != 0.0f; });
    QVERIFY(anyPainted);
}

void PaintControllerTest::endStrokeSynthesizesASilentBaseForAContentLessLayer() {
    // Project::createNew()'s own Background layer - content-less by
    // construction (see docs/sound-mind-architecture.md's Decisions
    // Made) - is exactly the case this test exercises: painting it
    // should work, not silently do nothing.
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    QVERIFY(!project.layers().front().content().has_value());
    PaintController controller;
    controller.setProject(&project);
    controller.setToolConfiguration(makeOpaqueTool());

    controller.beginStroke(backgroundId, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    QVERIFY(project.layers().front().content().has_value());
    const auto& content = *project.layers().front().content();
    // makeOpaqueTool()'s own default intensity (-10 dB) is well above the
    // silent base's own floor (well below -40 dB) - see silentContentFor()'s
    // own docs.
    const bool anyPainted = std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                                         [](float value) { return value > -40.0f; });
    QVERIFY(anyPainted);
}

void PaintControllerTest::cancelStrokeDiscardsTheStrokeWithoutAppendingAnOperation() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    controller.beginStroke(layerId, TimeFrequencyPoint{0.1, 500.0});
    controller.continueStroke(TimeFrequencyPoint{0.5, 500.0});

    controller.cancelStroke();

    QVERIFY(!controller.isStrokeInProgress());
    QVERIFY(controller.currentPreviewPath().nodes().empty());
    QCOMPARE(project.operationLog().size(), std::size_t{0});
}

void PaintControllerTest::undoRevertsTheLayersContentAndRedoReappliesIt() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    controller.setToolConfiguration(makeOpaqueTool());
    controller.beginStroke(layerId, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    const auto paintedContent = *project.layerById(layerId)->content();
    const bool wasPainted = std::any_of(paintedContent.leftMagnitudeDb.begin(), paintedContent.leftMagnitudeDb.end(),
                                         [](float value) { return value != 0.0f; });
    QVERIFY(wasPainted);

    QVERIFY(controller.canUndo());
    controller.undo();
    const auto undoneContent = *project.layerById(layerId)->content();
    QVERIFY(std::all_of(undoneContent.leftMagnitudeDb.begin(), undoneContent.leftMagnitudeDb.end(),
                         [](float value) { return value == 0.0f; }));

    QVERIFY(controller.canRedo());
    controller.redo();
    const auto redoneContent = *project.layerById(layerId)->content();
    QVERIFY(std::any_of(redoneContent.leftMagnitudeDb.begin(), redoneContent.leftMagnitudeDb.end(),
                         [](float value) { return value != 0.0f; }));
}

void PaintControllerTest::setProjectClearsAnyInProgressStroke() {
    Project firstProject = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(firstProject);
    PaintController controller;
    controller.setProject(&firstProject);
    controller.beginStroke(layerId, TimeFrequencyPoint{0.1, 500.0});
    QVERIFY(controller.isStrokeInProgress());

    Project secondProject = Project::createNew(testSettings());
    controller.setProject(&secondProject);

    QVERIFY(!controller.isStrokeInProgress());
}

void PaintControllerTest::beginStrokeRefusesSilentlyOnAnEqualizerLayer() {
    Project project = Project::createNew(testSettings());
    const LayerId equalizerId = project.layers().back().id();  // see Project::createNew()'s own docs.
    QCOMPARE(project.layers().back().type(), LayerType::Equalizer);
    PaintController controller;
    controller.setProject(&project);

    controller.beginStroke(equalizerId, TimeFrequencyPoint{0.1, 500.0});

    QVERIFY(!controller.isStrokeInProgress());
    QCOMPARE(project.operationLog().size(), std::size_t{0});
}

void PaintControllerTest::beginStrokeRefusesSilentlyOnAFilterLayer() {
    Project project = Project::createNew(testSettings());
    Layer filterLayer(0, "My Filter", LayerType::Filter);
    const LayerId filterId = project.addLayer(std::move(filterLayer));
    PaintController controller;
    controller.setProject(&project);

    controller.beginStroke(filterId, TimeFrequencyPoint{0.1, 500.0});

    QVERIFY(!controller.isStrokeInProgress());
    QCOMPARE(project.operationLog().size(), std::size_t{0});
}

// --- Mind Grains (v0.Y.33.1 Installment B) ----------------------------------

void PaintControllerTest::beginStrokeRefusesSilentlyWhenTheMindGrainToolIsNotAllowedOnTheTargetLayer() {
    Project project = Project::createNew(testSettings());
    const LayerId lower = addBlankNormalLayer(project);
    const LayerId upper = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    auto config = std::make_unique<MindGrainConfiguration>();
    config->setReference(std::nullopt, upper, TimeFrequencyRect{});
    controller.setToolConfiguration(std::move(config));

    // lower is NOT above upper (its own configured source) - must refuse
    // to even start the stroke.
    controller.beginStroke(lower, TimeFrequencyPoint{0.1, 500.0});

    QVERIFY(!controller.isStrokeInProgress());
    QCOMPARE(project.operationLog().size(), std::size_t{0});
}

void PaintControllerTest::beginStrokeStartsNormallyWhenTheMindGrainToolIsAllowedOnTheTargetLayer() {
    Project project = Project::createNew(testSettings());
    const LayerId lower = addBlankNormalLayer(project);
    const LayerId upper = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    auto config = std::make_unique<MindGrainConfiguration>();
    config->setReference(std::nullopt, lower, TimeFrequencyRect{});
    controller.setToolConfiguration(std::move(config));

    // upper IS above lower (its own configured source) - allowed.
    controller.beginStroke(upper, TimeFrequencyPoint{0.1, 500.0});

    QVERIFY(controller.isStrokeInProgress());
}

void PaintControllerTest::endStrokeWithAMindGrainToolPaintsFromTheSourceLayersCurrentContent() {
    Project project = Project::createNew(testSettings());
    const LayerId lower = addBlankNormalLayer(project);
    const LayerId upper = addBlankNormalLayer(project);
    // Give the source layer real, non-zero content to read from.
    auto sourceContent = *project.layerById(lower)->content();
    std::fill(sourceContent.leftMagnitudeDb.begin(), sourceContent.leftMagnitudeDb.end(), -6.0f);
    project.layerById(lower)->setContent(sourceContent);

    PaintController controller;
    controller.setProject(&project);
    auto config = std::make_unique<MindGrainConfiguration>();
    config->setReference(std::nullopt, lower, TimeFrequencyRect{0.0, 1.0, 20.0, 2020.0});
    controller.setToolConfiguration(std::move(config));

    controller.beginStroke(upper, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    const auto& content = *project.layerById(upper)->content();
    const bool anyPaintedFromSource = std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                                                   [](float value) { return value == -6.0f; });
    QVERIFY(anyPaintedFromSource);
}

void PaintControllerTest::rebuildLayerContentRereadsTheSourceLayersCurrentContentAfterInvalidateContentCaches() {
    Project project = Project::createNew(testSettings());
    const LayerId lower = addBlankNormalLayer(project);
    const LayerId upper = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    auto config = std::make_unique<MindGrainConfiguration>();
    config->setReference(std::nullopt, lower, TimeFrequencyRect{0.0, 1.0, 20.0, 2020.0});
    controller.setToolConfiguration(std::move(config));
    controller.beginStroke(upper, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    // lower's own content is still blank at this point - nothing visible
    // was actually painted onto upper yet.
    {
        const auto& content = *project.layerById(upper)->content();
        const bool anyNonZero = std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                                             [](float value) { return value != 0.0f; });
        QVERIFY(!anyNonZero);
    }

    // The source layer's content is replaced directly (bypassing
    // PaintController entirely - e.g. a fresh import into it, not a
    // paint-triggered rebuild PaintController would already know about).
    // v0.1.8.1's own PaintContentCache means `upper`'s own next rebuild no
    // longer notices this on its own (see PaintContentCache's own docs on
    // why - its own operations list hasn't changed, so without being told
    // otherwise, it reuses what it already has) - a caller that knows a
    // source layer changed through some path PaintController wasn't
    // already driving must say so explicitly, the same way MainWindow's
    // own mindWavesChanged handler does for the MindWave case below.
    auto sourceContent = *project.layerById(lower)->content();
    std::fill(sourceContent.leftMagnitudeDb.begin(), sourceContent.leftMagnitudeDb.end(), -9.0f);
    project.layerById(lower)->setContent(sourceContent);
    controller.invalidateContentCaches();

    controller.rebuildLayerContent(upper);

    const auto& content = *project.layerById(upper)->content();
    const bool anyFromNewSource = std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                                               [](float value) { return value == -9.0f; });
    QVERIFY(anyFromNewSource);
}

void PaintControllerTest::rebuildLayerContentDoesNotRereadTheSourceLayersCurrentContentWithoutInvalidateContentCaches() {
    // The deliberate narrowing the test above's own comment describes,
    // confirmed with the user: unlike before v0.1.8.1, a Mind Grain
    // stroke no longer re-samples its source on *every* rebuild of its
    // own layer, only when told to (explicitly, or via the existing Mind
    // Grain cascade - see paintingOnASourceLayerImmediatelyCascadesToDependentMindGrainLayers()
    // below for that path). A regression guard, not just documentation -
    // if this starts failing, PaintContentCache's own fast path stopped
    // being a cache and became correctness-breaking staleness instead.
    Project project = Project::createNew(testSettings());
    const LayerId lower = addBlankNormalLayer(project);
    const LayerId upper = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    auto config = std::make_unique<MindGrainConfiguration>();
    config->setReference(std::nullopt, lower, TimeFrequencyRect{0.0, 1.0, 20.0, 2020.0});
    controller.setToolConfiguration(std::move(config));
    controller.beginStroke(upper, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    auto sourceContent = *project.layerById(lower)->content();
    std::fill(sourceContent.leftMagnitudeDb.begin(), sourceContent.leftMagnitudeDb.end(), -9.0f);
    project.layerById(lower)->setContent(sourceContent);
    // No invalidateContentCaches() call this time.

    controller.rebuildLayerContent(upper);

    const auto& content = *project.layerById(upper)->content();
    const bool anyFromNewSource = std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                                               [](float value) { return value == -9.0f; });
    QVERIFY(!anyFromNewSource);
}

void PaintControllerTest::paintingOnASourceLayerImmediatelyCascadesToDependentMindGrainLayers() {
    Project project = Project::createNew(testSettings());
    const LayerId lower = addBlankNormalLayer(project);
    const LayerId upper = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);

    // A Mind Grain stroke on `upper`, sourced from `lower`.
    auto config = std::make_unique<MindGrainConfiguration>();
    config->setReference(std::nullopt, lower, TimeFrequencyRect{0.0, 1.0, 20.0, 2020.0});
    controller.setToolConfiguration(std::move(config));
    controller.beginStroke(upper, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    // `lower` is still blank - nothing visible was actually painted yet.
    {
        const auto& content = *project.layerById(upper)->content();
        QVERIFY(std::none_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                              [](float value) { return value != 0.0f; }));
    }

    // Paint directly on `lower` - a plain, unrelated stroke - and never
    // touch `upper` again ourselves (no rebuildLayerContent(upper) call).
    controller.setToolConfiguration(makeOpaqueTool());
    controller.beginStroke(lower, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    // `upper`'s own Mind Grain stroke must already reflect it - the whole
    // point of this fix: a repaint of the source cascades immediately,
    // rather than waiting for `upper`'s own next, unrelated rebuild.
    const auto& content = *project.layerById(upper)->content();
    QVERIFY(std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                         [](float value) { return value != 0.0f; }));
}

void PaintControllerTest::cascadeVisitsEachDependentLayerOnlyOncePerRebuild() {
    Project project = Project::createNew(testSettings());
    const LayerId a = addBlankNormalLayer(project);
    const LayerId b = addBlankNormalLayer(project);
    const LayerId c = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);

    // `b` has a Mind Grain sourced from `a`.
    auto configB = std::make_unique<MindGrainConfiguration>();
    configB->setReference(std::nullopt, a, TimeFrequencyRect{0.0, 1.0, 20.0, 2020.0});
    controller.setToolConfiguration(std::move(configB));
    controller.beginStroke(b, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    // `c` has two Mind Grains: one sourced from `a` directly, another from
    // `b` - a diamond (`a` -> `b` -> `c`, and `a` -> `c` directly), so a
    // naive cascade could visit `c` twice for a single repaint of `a`.
    auto configCFromA = std::make_unique<MindGrainConfiguration>();
    configCFromA->setReference(std::nullopt, a, TimeFrequencyRect{0.0, 1.0, 20.0, 2020.0});
    controller.setToolConfiguration(std::move(configCFromA));
    controller.beginStroke(c, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    auto configCFromB = std::make_unique<MindGrainConfiguration>();
    configCFromB->setReference(std::nullopt, b, TimeFrequencyRect{0.0, 1.0, 20.0, 2020.0});
    controller.setToolConfiguration(std::move(configCFromB));
    controller.beginStroke(c, TimeFrequencyPoint{0.6, 700.0});
    controller.endStroke();

    // Repaint `a` directly - the cascade must visit `b` and `c` exactly
    // once each, not once per dependency edge.
    controller.setToolConfiguration(makeOpaqueTool());
    QSignalSpy spy(&controller, &PaintController::contentChanged);
    controller.beginStroke(a, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    QCOMPARE(spy.count(), 3);  // a, b, c - each exactly once.
    std::vector<LayerId> seen;
    for (const auto& args : spy) {
        seen.push_back(args.at(0).value<LayerId>());
    }
    QVERIFY(std::find(seen.begin(), seen.end(), a) != seen.end());
    QVERIFY(std::find(seen.begin(), seen.end(), b) != seen.end());
    QVERIFY(std::find(seen.begin(), seen.end(), c) != seen.end());
}

void PaintControllerTest::endStrokeWithAnInstrumentToolBoundToATremoloMindWaveResolvesItAgainstTheProject() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto tremoloId = project.addMindWave("Tremolo", alwaysBaselineWave());

    PaintController controller;
    controller.setProject(&project);
    auto config = makeOpaqueInstrumentTool();
    config->setTremoloDepth(1.0);  // Full depth - baseline dips strength all the way to 0.
    config->setTremoloMindWave(tremoloId);
    controller.setToolConfiguration(std::move(config));

    controller.beginStroke(layerId, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    // The controller resolved tremoloId against the live Project's own
    // MindWave library (rather than leaving the stroke unmodulated because
    // no resolver was ever wired up) - the baseline wave silences the
    // stroke completely.
    const auto& content = *project.layerById(layerId)->content();
    const bool anyPainted = std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                                         [](float value) { return value != 0.0f; });
    QVERIFY(!anyPainted);
}

void PaintControllerTest::rebuildLayerContentRereadsTheTremoloMindWaveLibraryAfterInvalidateContentCaches() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto tremoloId = project.addMindWave("Tremolo", alwaysBaselineWave());

    PaintController controller;
    controller.setProject(&project);
    auto config = makeOpaqueInstrumentTool();
    config->setTremoloDepth(1.0);
    config->setTremoloMindWave(tremoloId);
    controller.setToolConfiguration(std::move(config));

    controller.beginStroke(layerId, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    // Still silent - the bound wave is still at baseline.
    {
        const auto& content = *project.layerById(layerId)->content();
        const bool anyPainted = std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                                             [](float value) { return value != 0.0f; });
        QVERIFY(!anyPainted);
    }

    // Edit the *same* library entry directly (as if the user had just
    // reshaped it in the MindWaves panel) - no new stroke is drawn, just a
    // fresh rebuild of the existing one, per the "live reference, not a
    // snapshot" contract MindWaveResolver's own docs establish.
    // v0.1.8.1's own PaintContentCache needs an explicit
    // invalidateContentCaches() call told about this, the same as
    // MainWindow's own mindWavesChanged handler already does in
    // production (PaintController alone, as this test uses it, has no
    // way to find out on its own that a MindWave somewhere changed).
    project.mindWaveById(tremoloId)->wave.setPhaseRadians(0.0);  // Baseline -> ceiling.
    controller.invalidateContentCaches();
    controller.rebuildLayerContent(layerId);

    const auto& content = *project.layerById(layerId)->content();
    const bool anyPainted = std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                                         [](float value) { return value != 0.0f; });
    QVERIFY(anyPainted);
}

void PaintControllerTest::rebuildLayerContentDoesNotRereadTheTremoloMindWaveLibraryWithoutInvalidateContentCaches() {
    // The deliberate narrowing the test above's own comment describes -
    // a regression guard, not just documentation: if this starts
    // failing, PaintContentCache's own fast path stopped being a cache
    // and became correctness-breaking staleness instead.
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto tremoloId = project.addMindWave("Tremolo", alwaysBaselineWave());

    PaintController controller;
    controller.setProject(&project);
    auto config = makeOpaqueInstrumentTool();
    config->setTremoloDepth(1.0);
    config->setTremoloMindWave(tremoloId);
    controller.setToolConfiguration(std::move(config));

    controller.beginStroke(layerId, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    project.mindWaveById(tremoloId)->wave.setPhaseRadians(0.0);  // Baseline -> ceiling.
    // No invalidateContentCaches() call this time.
    controller.rebuildLayerContent(layerId);

    const auto& content = *project.layerById(layerId)->content();
    const bool anyPainted = std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                                         [](float value) { return value != 0.0f; });
    QVERIFY(!anyPainted);
}
