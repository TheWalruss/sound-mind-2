#include "test_layers_panel.h"

#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
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
    QVERIFY(panel.findChild<QLabel*>(QStringLiteral("typeTagLabel")) == nullptr);
}
