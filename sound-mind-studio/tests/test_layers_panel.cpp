#include "test_layers_panel.h"

#include <QDoubleSpinBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QtTest/QtTest>

#include "sound_mind/studio/layers_panel.h"

using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::studio::LayersPanel;

namespace {

std::vector<LayersPanel::RowData> twoNormalLayers() {
    // Bottom-to-top, matching Project::layers()' own convention -
    // "Top" ends up displayed first (index 0), "Bottom" second.
    LayersPanel::RowData bottom;
    bottom.id = 1;
    bottom.name = QStringLiteral("Bottom");
    bottom.type = LayerType::Normal;
    bottom.opacity = 1.0f;
    bottom.visible = true;

    LayersPanel::RowData top;
    top.id = 2;
    top.name = QStringLiteral("Top");
    top.type = LayerType::Normal;
    top.opacity = 0.5f;
    top.visible = false;

    return {bottom, top};
}

}  // namespace

void LayersPanelTest::setLayersCreatesOneRowPerLayerTopFirst() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());

    const auto nameLabels = panel.findChildren<QLabel*>(QStringLiteral("nameLabel"));
    QCOMPARE(nameLabels.size(), 2);
    QCOMPARE(nameLabels.at(0)->text(), QStringLiteral("Top"));
    QCOMPARE(nameLabels.at(1)->text(), QStringLiteral("Bottom"));
}

void LayersPanelTest::setLayersReplacesThePreviousRows() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    QCOMPARE(panel.findChildren<QLabel*>(QStringLiteral("nameLabel")).size(), 2);

    LayersPanel::RowData single;
    single.id = 3;
    single.name = QStringLiteral("Only");
    panel.setLayers({single});
    // setLayers() deletes the previous rows via deleteLater() (see its
    // own docs for why) - QTest::qWait(0) spins the event loop once so
    // that's actually happened before counting children below, the same
    // as it would have by the time a real user's next interaction runs.
    QTest::qWait(0);

    const auto nameLabels = panel.findChildren<QLabel*>(QStringLiteral("nameLabel"));
    QCOMPARE(nameLabels.size(), 1);
    QCOMPARE(nameLabels.at(0)->text(), QStringLiteral("Only"));
}

void LayersPanelTest::visibilityButtonEmitsVisibilityToggled() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    QSignalSpy spy(&panel, &LayersPanel::visibilityToggled);

    const auto buttons = panel.findChildren<QPushButton*>(QStringLiteral("visibilityButton"));
    QCOMPARE(buttons.size(), 2);
    buttons.at(0)->click();  // "Top" (id 2), currently invisible - toggling on.

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), static_cast<LayerId>(2));
    QCOMPARE(spy.at(0).at(1).toBool(), true);
}

void LayersPanelTest::backgroundVisibilityButtonIsDisabled() {
    LayersPanel::RowData background;
    background.id = 1;
    background.name = QStringLiteral("Background");
    background.type = LayerType::Background;

    LayersPanel panel;
    panel.setLayers({background});

    const auto buttons = panel.findChildren<QPushButton*>(QStringLiteral("visibilityButton"));
    QCOMPARE(buttons.size(), 1);
    QVERIFY(!buttons.at(0)->isEnabled());
}

void LayersPanelTest::backgroundLayerHasNoOpacityOrTransformControls() {
    // Confirmed with the user: opacity and the Layer Time Alignment
    // controls (translation/rescale) don't make sense for the Background
    // layer - it's always the floor of the stack, always fully opaque,
    // with nothing else beneath it to line up against in time.
    LayersPanel::RowData background;
    background.id = 1;
    background.name = QStringLiteral("Background");
    background.type = LayerType::Background;

    LayersPanel panel;
    panel.setLayers({background});

    QVERIFY(panel.findChild<QSlider*>(QStringLiteral("opacitySlider")) == nullptr);
    QVERIFY(panel.findChild<QSpinBox*>(QStringLiteral("translationSpinBox")) == nullptr);
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("rescaleSpinBox")) == nullptr);
}

void LayersPanelTest::opacitySliderEmitsOpacityChanged() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    QSignalSpy spy(&panel, &LayersPanel::opacityChanged);

    const auto sliders = panel.findChildren<QSlider*>(QStringLiteral("opacitySlider"));
    QCOMPARE(sliders.size(), 2);
    sliders.at(1)->setValue(25);  // "Bottom" (id 1).

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), static_cast<LayerId>(1));
    QCOMPARE(spy.at(0).at(1).toFloat(), 0.25f);
}

void LayersPanelTest::translationSpinBoxEmitsTranslationChanged() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    QSignalSpy spy(&panel, &LayersPanel::translationChanged);

    const auto spinBoxes = panel.findChildren<QSpinBox*>(QStringLiteral("translationSpinBox"));
    QCOMPARE(spinBoxes.size(), 2);
    spinBoxes.at(1)->setValue(150);  // "Bottom" (id 1).

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), static_cast<LayerId>(1));
    QCOMPARE(spy.at(0).at(1).value<qint64>(), static_cast<qint64>(150));
}

void LayersPanelTest::rescaleSpinBoxEmitsRescaleChanged() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    QSignalSpy spy(&panel, &LayersPanel::rescaleChanged);

    const auto spinBoxes = panel.findChildren<QDoubleSpinBox*>(QStringLiteral("rescaleSpinBox"));
    QCOMPARE(spinBoxes.size(), 2);
    spinBoxes.at(1)->setValue(2.0);  // "Bottom" (id 1).

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), static_cast<LayerId>(1));
    QCOMPARE(spy.at(0).at(1).toDouble(), 2.0);
}

void LayersPanelTest::doubleClickingNameEmitsRenameRequested() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    QSignalSpy spy(&panel, &LayersPanel::renameRequested);

    const auto nameLabels = panel.findChildren<QLabel*>(QStringLiteral("nameLabel"));
    QCOMPARE(nameLabels.size(), 2);
    QTest::mouseDClick(nameLabels.at(0), Qt::LeftButton);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), static_cast<LayerId>(2));
}

void LayersPanelTest::deleteButtonEmitsDeleteRequestedForNormalLayers() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    QSignalSpy spy(&panel, &LayersPanel::deleteRequested);

    const auto buttons = panel.findChildren<QPushButton*>(QStringLiteral("deleteButton"));
    QCOMPARE(buttons.size(), 2);
    buttons.at(0)->click();  // "Top" (id 2).

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), static_cast<LayerId>(2));
}

void LayersPanelTest::lockedLayersHaveNoDeleteButton() {
    LayersPanel::RowData background;
    background.id = 1;
    background.type = LayerType::Background;

    LayersPanel panel;
    panel.setLayers({background});

    QVERIFY(panel.findChildren<QPushButton*>(QStringLiteral("deleteButton")).isEmpty());
}

void LayersPanelTest::lockedLayersHaveALockIconInsteadOfADragHandle() {
    LayersPanel::RowData background;
    background.id = 1;
    background.type = LayerType::Background;

    LayersPanel panel;
    panel.setLayers({background});

    QVERIFY(panel.findChild<QLabel*>(QStringLiteral("lockLabel")) != nullptr);
    QVERIFY(panel.findChild<QWidget*>(QStringLiteral("dragHandle")) == nullptr);
}

void LayersPanelTest::nonNormalLayersShowATypeTag() {
    LayersPanel::RowData background;
    background.id = 1;
    background.type = LayerType::Background;

    LayersPanel panel;
    panel.setLayers({background});

    QVERIFY(panel.findChild<QLabel*>(QStringLiteral("typeTagLabel")) != nullptr);

    panel.setLayers(twoNormalLayers());
    QTest::qWait(0);  // let the Background row's deleteLater() actually happen - see setLayersReplacesThePreviousRows().
    QVERIFY(panel.findChild<QLabel*>(QStringLiteral("typeTagLabel")) == nullptr);
}

void LayersPanelTest::freshPanelHasNoSelection() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());

    QVERIFY(!panel.selectedLayerId().has_value());
}

void LayersPanelTest::clickingANameSelectsItsLayer() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());

    const auto nameLabels = panel.findChildren<QLabel*>(QStringLiteral("nameLabel"));
    QCOMPARE(nameLabels.size(), 2);
    QTest::mouseClick(nameLabels.at(0), Qt::LeftButton);  // "Top" (id 2).

    QVERIFY(panel.selectedLayerId().has_value());
    QCOMPARE(*panel.selectedLayerId(), static_cast<LayerId>(2));

    auto* list = panel.findChild<QListWidget*>(QStringLiteral("layersList"));
    QVERIFY(list != nullptr);
    QCOMPARE(list->currentRow(), 0);  // "Top" is displayed first.
}

void LayersPanelTest::selectionSurvivesASetLayersRefreshOfTheSameLayers() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    QTest::mouseClick(panel.findChildren<QLabel*>(QStringLiteral("nameLabel")).at(0), Qt::LeftButton);

    panel.setLayers(twoNormalLayers());  // e.g. an opacity change elsewhere triggering a refresh.

    QVERIFY(panel.selectedLayerId().has_value());
    QCOMPARE(*panel.selectedLayerId(), static_cast<LayerId>(2));
    auto* list = panel.findChild<QListWidget*>(QStringLiteral("layersList"));
    QCOMPARE(list->currentRow(), 0);
}

void LayersPanelTest::selectionIsDroppedWhenTheSelectedLayerIsGoneFromANewSetLayersCall() {
    LayersPanel panel;
    const auto layers = twoNormalLayers();
    panel.setLayers(layers);
    QTest::mouseClick(panel.findChildren<QLabel*>(QStringLiteral("nameLabel")).at(0), Qt::LeftButton);  // "Top" (id 2).
    QVERIFY(panel.selectedLayerId().has_value());

    panel.setLayers({layers.front()});  // "Top" (id 2) deleted - only "Bottom" (id 1) remains.

    QVERIFY(!panel.selectedLayerId().has_value());
}

void LayersPanelTest::clearSelectionDropsTheSelectionAndItsHighlight() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    QTest::mouseClick(panel.findChildren<QLabel*>(QStringLiteral("nameLabel")).at(0), Qt::LeftButton);
    QVERIFY(panel.selectedLayerId().has_value());

    panel.clearSelection();

    QVERIFY(!panel.selectedLayerId().has_value());
    auto* list = panel.findChild<QListWidget*>(QStringLiteral("layersList"));
    QCOMPARE(list->currentRow(), -1);
}
