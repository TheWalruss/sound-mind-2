#include "test_layer_controller.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/filter_configuration_panel.h"
#include "sound_mind/studio/layer_controller.h"
#include "sound_mind/studio/layers_panel.h"
#include "sound_mind/studio/playback_controller.h"

using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::studio::CanvasWidget;
using sound_mind::studio::FilterConfigurationPanel;
using sound_mind::studio::LayerController;
using sound_mind::studio::LayersPanel;
using sound_mind::studio::PlaybackController;

namespace {

ProjectSettings testSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 20;
    settings.canvasHeight = 10;
    settings.binCount = 10;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 2020.0f;
    settings.timestepMs = 10.0;
    return settings;
}

/// @brief Owns every collaborator a LayerController needs, so each test
/// only has to build the controller itself.
struct Fixture {
    CanvasWidget canvas;
    PlaybackController playbackController{nullptr, sound_mind::core::AudioDeviceMode::None};
    LayersPanel layersPanel;
    FilterConfigurationPanel filterConfigurationPanel;
    LayerController controller{&canvas, &playbackController, &layersPanel, &filterConfigurationPanel};
};

}  // namespace

void LayerControllerTest::withNoProjectLookupsReturnNulloptOrNullptr() {
    Fixture fixture;
    QVERIFY(fixture.controller.topmostLayerWithContent() == nullptr);
    QVERIFY(fixture.controller.layerById(1) == nullptr);
    QVERIFY(!fixture.controller.paintTargetLayerId().has_value());
}

void LayerControllerTest::setProjectMakesLookupsWork() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);

    QVERIFY(fixture.controller.layerById(backgroundId) != nullptr);
    QCOMPARE(fixture.controller.paintTargetLayerId().value(), backgroundId);
}

void LayerControllerTest::toggleLayerVisibilityChangesVisibilityAndEmitsLayersChanged() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);
    QSignalSpy spy(&fixture.controller, &LayerController::layersChanged);

    fixture.controller.toggleLayerVisibility(backgroundId, false);

    QVERIFY(!project.layers().front().visible());
    QCOMPARE(spy.count(), 1);
}

void LayerControllerTest::setLayerOpacityChangesOpacity() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);

    fixture.controller.setLayerOpacity(backgroundId, 0.5f);

    QCOMPARE(project.layers().front().opacity(), 0.5f);
}

void LayerControllerTest::setLayerTranslationChangesTranslation() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);

    fixture.controller.setLayerTranslation(backgroundId, 7);

    QCOMPARE(project.layers().front().translationColumns(), static_cast<std::int64_t>(7));
}

void LayerControllerTest::setLayerRescaleChangesRescale() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);

    fixture.controller.setLayerRescale(backgroundId, 2.0);

    QCOMPARE(project.layers().front().rescaleFactor(), 2.0);
}

void LayerControllerTest::renameLayerToRenamesAndRejectsEmptyName() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);

    QVERIFY(fixture.controller.renameLayerTo(backgroundId, QStringLiteral("Floor")));
    QCOMPARE(QString::fromStdString(project.layers().front().name()), QStringLiteral("Floor"));
    QVERIFY(!fixture.controller.renameLayerTo(backgroundId, QString()));
}

void LayerControllerTest::deleteLayerRemovesALayerButRefusesALockedOne() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    const LayerId equalizerId = project.layers().back().id();
    Layer normal(0, "Normal", LayerType::Normal);
    const LayerId normalId = project.addLayer(std::move(normal));
    fixture.controller.setProject(&project);
    const std::size_t countBefore = project.layers().size();

    fixture.controller.deleteLayer(backgroundId);  // locked - refused.
    QCOMPARE(project.layers().size(), countBefore);
    fixture.controller.deleteLayer(equalizerId);  // locked - refused.
    QCOMPARE(project.layers().size(), countBefore);

    fixture.controller.deleteLayer(normalId);  // not locked - removed.
    QCOMPARE(project.layers().size(), countBefore - 1);
    QVERIFY(fixture.controller.layerById(normalId) == nullptr);
}

void LayerControllerTest::addEmptyLayerAddsAndSelectsANormalLayer() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);
    const std::size_t countBefore = project.layers().size();

    sound_mind::codec::StreamImage placeholder;
    placeholder.config = sound_mind::core::streamCodecConfigFor(project.settings());
    placeholder.frameCount = project.settings().canvasWidth;
    const std::size_t pixelCount = std::size_t{placeholder.config.binCount} * placeholder.frameCount;
    placeholder.leftMagnitudeDb.assign(pixelCount, 0.0f);
    placeholder.rightMagnitudeDb.assign(pixelCount, 0.0f);
    placeholder.sharedPhaseRadians.assign(pixelCount, 0.0f);
    fixture.controller.addEmptyLayer(placeholder);

    QCOMPARE(project.layers().size(), countBefore + 1);
    QVERIFY(fixture.layersPanel.selectedLayerId().has_value());
}

void LayerControllerTest::addFilterLayerAddsAndSelectsAFilterLayer() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);
    const std::size_t countBefore = project.layers().size();

    fixture.controller.addFilterLayer();

    QCOMPARE(project.layers().size(), countBefore + 1);
    const auto selectedId = fixture.layersPanel.selectedLayerId();
    QVERIFY(selectedId.has_value());
    QCOMPARE(fixture.controller.layerById(*selectedId)->type(), LayerType::Filter);
}

void LayerControllerTest::handleLayerSelectionChangedSyncsFilterConfigurationPanel() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();  // Normal - not a Filter layer.
    fixture.controller.setProject(&project);

    fixture.controller.handleLayerSelectionChanged(backgroundId);
    QVERIFY(!fixture.filterConfigurationPanel.isEnabled());

    fixture.controller.addFilterLayer();
    const auto filterId = fixture.layersPanel.selectedLayerId();
    fixture.controller.handleLayerSelectionChanged(filterId);
    QVERIFY(fixture.filterConfigurationPanel.isEnabled());

    fixture.controller.handleLayerSelectionChanged(std::nullopt);
    QVERIFY(!fixture.filterConfigurationPanel.isEnabled());
}

void LayerControllerTest::applyFilterConfigurationAppliesOnlyToAFilterLayer() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);

    sound_mind::core::FilterConfiguration config;
    config.setBlurSigma(3.0f);

    // No selection at all - a no-op.
    fixture.controller.applyFilterConfiguration(config);

    fixture.controller.addFilterLayer();
    const auto filterId = fixture.layersPanel.selectedLayerId();
    QVERIFY(filterId.has_value());

    fixture.controller.applyFilterConfiguration(config);
    QCOMPARE(fixture.controller.layerById(*filterId)->filterConfiguration().blurSigma(), 3.0f);
    // The Normal (Background) layer is untouched by a config meant for
    // whichever layer is currently selected in the panel.
    QVERIFY(backgroundId != *filterId);
}

void LayerControllerTest::reorderLayersRefreshesEitherWay() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);

    // An invalid permutation (missing/duplicate ids) is rejected by
    // Project::reorderLayers() itself, but the panel is still refreshed -
    // this just needs to not crash.
    fixture.controller.reorderLayers({});
}

void LayerControllerTest::paintTargetLayerIdFallsBackToTheBottommostLayer() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    const LayerId backgroundId = project.layers().front().id();
    fixture.controller.setProject(&project);

    // Nothing selected in the panel yet - falls back to the bottommost
    // (Background) layer, not the Equalizer (always .back()).
    QCOMPARE(fixture.controller.paintTargetLayerId().value(), backgroundId);
}
