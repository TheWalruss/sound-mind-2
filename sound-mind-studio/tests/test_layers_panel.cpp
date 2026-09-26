#include "test_layers_panel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QStackedLayout>
#include <QtTest/QtTest>

#include "sound_mind/studio/layers_panel.h"

using sound_mind::core::BlendMode;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::MindWaveId;
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

void LayersPanelTest::visibilityButtonEmitsVisibilityCycleRequestedAndShowsTheCorrectGlyphPerState() {
    // v0.Y.46.1 Installment B ("Layers Panel & Editing Enhancements v2") -
    // replaced the old plain on/off visibilityToggled() signal with a
    // 3-way cycle (Visible -> Muted -> Invisible); the button itself
    // doesn't compute the next state (LayerController does) - it just
    // requests a cycle and displays whatever RowData it's next given.
    // A single, purpose-built row - twoNormalLayers()'s own "Top" starts
    // invisible, which would make the first assertion below misleading.
    LayersPanel::RowData row;
    row.id = 1;
    row.name = QStringLiteral("Layer");
    row.type = LayerType::Normal;
    row.visible = true;
    row.muted = false;

    LayersPanel panel;
    panel.setLayers({row});
    QSignalSpy spy(&panel, &LayersPanel::visibilityCycleRequested);

    auto buttons = panel.findChildren<QPushButton*>(QStringLiteral("visibilityButton"));
    QCOMPARE(buttons.size(), 1);
    QCOMPARE(buttons.at(0)->text(), QStringLiteral("●"));  // Visible, unmuted.
    buttons.at(0)->click();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), static_cast<LayerId>(1));

    row.muted = true;  // Simulates the controller's own Visible -> Muted transition.
    panel.setLayers({row});
    QTest::qWait(0);  // rebuildRows() rebuilds via deleteLater() - see setLayersReplacesThePreviousRows().
    buttons = panel.findChildren<QPushButton*>(QStringLiteral("visibilityButton"));
    QCOMPARE(buttons.at(0)->text(), QStringLiteral("◐"));

    row.visible = false;
    row.muted = false;  // Simulates Muted -> Invisible.
    panel.setLayers({row});
    QTest::qWait(0);
    buttons = panel.findChildren<QPushButton*>(QStringLiteral("visibilityButton"));
    QCOMPARE(buttons.at(0)->text(), QStringLiteral("○"));
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
    QVERIFY(panel.findChild<QSlider*>(QStringLiteral("balanceSlider")) == nullptr);
    QVERIFY(panel.findChild<QSpinBox*>(QStringLiteral("translationSpinBox")) == nullptr);
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("rescaleSpinBox")) == nullptr);
}

void LayersPanelTest::opacitySliderEmitsOpacityChanged() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    panel.selectLayer(static_cast<LayerId>(1));  // "Bottom" - the opacity slider is one of the
                                                  // redesign's own "revealed once selected" controls.
    QSignalSpy spy(&panel, &LayersPanel::opacityChanged);

    const auto sliders = panel.findChildren<QSlider*>(QStringLiteral("opacitySlider"));
    QCOMPARE(sliders.size(), 1);  // only the selected row's own.
    sliders.at(0)->setValue(25);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), static_cast<LayerId>(1));
    QCOMPARE(spy.at(0).at(1).toFloat(), 0.25f);
}

void LayersPanelTest::balanceSliderEmitsBalanceChanged() {
    // v0.Y.46.1 Installment C ("Per-layer balance").
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    panel.selectLayer(static_cast<LayerId>(1));  // "Bottom".
    QSignalSpy spy(&panel, &LayersPanel::balanceChanged);

    const auto sliders = panel.findChildren<QSlider*>(QStringLiteral("balanceSlider"));
    QCOMPARE(sliders.size(), 1);
    sliders.at(0)->setValue(25);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), static_cast<LayerId>(1));
    QCOMPARE(spy.at(0).at(1).toFloat(), 0.25f);
}

void LayersPanelTest::translationSpinBoxEmitsTranslationChanged() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    panel.selectLayer(static_cast<LayerId>(1));  // "Bottom".
    QSignalSpy spy(&panel, &LayersPanel::translationChanged);

    const auto spinBoxes = panel.findChildren<QSpinBox*>(QStringLiteral("translationSpinBox"));
    QCOMPARE(spinBoxes.size(), 1);
    spinBoxes.at(0)->setValue(150);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), static_cast<LayerId>(1));
    QCOMPARE(spy.at(0).at(1).value<qint64>(), static_cast<qint64>(150));
}

void LayersPanelTest::rescaleSpinBoxEmitsRescaleChanged() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    panel.selectLayer(static_cast<LayerId>(1));  // "Bottom".
    QSignalSpy spy(&panel, &LayersPanel::rescaleChanged);

    const auto spinBoxes = panel.findChildren<QDoubleSpinBox*>(QStringLiteral("rescaleSpinBox"));
    QCOMPARE(spinBoxes.size(), 1);
    spinBoxes.at(0)->setValue(2.0);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), static_cast<LayerId>(1));
    QCOMPARE(spy.at(0).at(1).toDouble(), 2.0);
}

void LayersPanelTest::unselectedRowsShowNoOpacityOrTransformOrBlendModeOrDeleteControls() {
    // The Layers Panel Redesign's own central premise: an unselected row
    // shows only its visibility eye alongside its name - everything else
    // (opacity, MindWave combo, transform controls, blend mode, delete,
    // drag handle) waits for selection.
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());  // nothing selected.

    QVERIFY(panel.findChild<QSlider*>(QStringLiteral("opacitySlider")) == nullptr);
    QVERIFY(panel.findChild<QSlider*>(QStringLiteral("balanceSlider")) == nullptr);
    QVERIFY(panel.findChild<QComboBox*>(QStringLiteral("opacityMindWaveCombo")) == nullptr);
    QVERIFY(panel.findChild<QSpinBox*>(QStringLiteral("translationSpinBox")) == nullptr);
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("rescaleSpinBox")) == nullptr);
    QVERIFY(panel.findChild<QComboBox*>(QStringLiteral("blendModeCombo")) == nullptr);
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("deleteButton")) == nullptr);
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("duplicateButton")) == nullptr);
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("cleanUpPhaseButton")) == nullptr);
    QVERIFY(panel.findChild<QWidget*>(QStringLiteral("dragHandle")) == nullptr);
    // The visibility eye is the one named exception - always present.
    QCOMPARE(panel.findChildren<QPushButton*>(QStringLiteral("visibilityButton")).size(), 2);
}

void LayersPanelTest::selectingARowRevealsItsOwnControlsAndDeselectingHidesThemAgain() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());

    panel.selectLayer(static_cast<LayerId>(1));  // "Bottom".
    QCOMPARE(panel.findChildren<QSlider*>(QStringLiteral("opacitySlider")).size(), 1);
    QCOMPARE(panel.findChildren<QSlider*>(QStringLiteral("balanceSlider")).size(), 1);
    QCOMPARE(panel.findChildren<QComboBox*>(QStringLiteral("opacityMindWaveCombo")).size(), 1);
    QCOMPARE(panel.findChildren<QSpinBox*>(QStringLiteral("translationSpinBox")).size(), 1);
    QCOMPARE(panel.findChildren<QDoubleSpinBox*>(QStringLiteral("rescaleSpinBox")).size(), 1);
    QCOMPARE(panel.findChildren<QComboBox*>(QStringLiteral("blendModeCombo")).size(), 1);
    QCOMPARE(panel.findChildren<QPushButton*>(QStringLiteral("deleteButton")).size(), 1);
    QCOMPARE(panel.findChildren<QPushButton*>(QStringLiteral("duplicateButton")).size(), 1);
    QCOMPARE(panel.findChildren<QWidget*>(QStringLiteral("dragHandle")).size(), 1);
    // twoNormalLayers() rows have no thumbnail (no content yet) - the
    // phase-cleanup button stays absent even once selected, unlike
    // duplicate/delete, which only gate on selection - real-world testing
    // pass finding #24.
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("cleanUpPhaseButton")) == nullptr);

    panel.clearSelection();
    QTest::qWait(0);  // rebuildRows() rebuilds via deleteLater() - see setLayersReplacesThePreviousRows().

    QVERIFY(panel.findChild<QSlider*>(QStringLiteral("opacitySlider")) == nullptr);
    QVERIFY(panel.findChild<QSlider*>(QStringLiteral("balanceSlider")) == nullptr);
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("deleteButton")) == nullptr);
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("duplicateButton")) == nullptr);
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
    panel.selectLayer(static_cast<LayerId>(2));  // "Top" - delete is one of the redesign's own
                                                  // "revealed once selected" controls.
    QSignalSpy spy(&panel, &LayersPanel::deleteRequested);

    const auto buttons = panel.findChildren<QPushButton*>(QStringLiteral("deleteButton"));
    QCOMPARE(buttons.size(), 1);
    buttons.at(0)->click();

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
    QVERIFY(panel.findChildren<QPushButton*>(QStringLiteral("duplicateButton")).isEmpty());
    QVERIFY(panel.findChildren<QPushButton*>(QStringLiteral("cleanUpPhaseButton")).isEmpty());

    // Still none once selected - locked stays locked regardless of the
    // redesign's own selection-gated controls.
    panel.selectLayer(static_cast<LayerId>(1));
    QVERIFY(panel.findChildren<QPushButton*>(QStringLiteral("deleteButton")).isEmpty());
    QVERIFY(panel.findChildren<QPushButton*>(QStringLiteral("duplicateButton")).isEmpty());
    QVERIFY(panel.findChildren<QPushButton*>(QStringLiteral("cleanUpPhaseButton")).isEmpty());
}

void LayersPanelTest::duplicateButtonEmitsDuplicateRequestedForNormalLayers() {
    // Real-world testing pass finding #22.
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    panel.selectLayer(static_cast<LayerId>(2));  // "Top" - duplicate is one of the redesign's own
                                                  // "revealed once selected" controls.
    QSignalSpy spy(&panel, &LayersPanel::duplicateRequested);

    const auto buttons = panel.findChildren<QPushButton*>(QStringLiteral("duplicateButton"));
    QCOMPARE(buttons.size(), 1);
    buttons.at(0)->click();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), static_cast<LayerId>(2));
}

void LayersPanelTest::cleanUpPhaseButtonOnlyAppearsForASelectedRowWithContentAndEmitsCleanUpPhaseRequested() {
    // Real-world testing pass finding #24 - unlike duplicate/delete, this
    // button also requires the row to actually have content (a non-null
    // thumbnail - see RowData::thumbnail's own docs).
    auto rows = twoNormalLayers();
    rows[1].thumbnail = QImage(4, 4, QImage::Format_RGB32);  // "Top" (id 2) has content.

    LayersPanel panel;
    panel.setLayers(rows);
    panel.selectLayer(static_cast<LayerId>(2));
    QSignalSpy spy(&panel, &LayersPanel::cleanUpPhaseRequested);

    const auto buttons = panel.findChildren<QPushButton*>(QStringLiteral("cleanUpPhaseButton"));
    QCOMPARE(buttons.size(), 1);
    buttons.at(0)->click();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), static_cast<LayerId>(2));

    panel.selectLayer(static_cast<LayerId>(1));  // "Bottom" - no thumbnail, no button.
    QTest::qWait(0);  // rebuildRows() rebuilds via deleteLater() - see setLayersReplacesThePreviousRows().
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("cleanUpPhaseButton")) == nullptr);
}

void LayersPanelTest::lockedLayersHaveALockIconInsteadOfADragHandle() {
    LayersPanel::RowData background;
    background.id = 1;
    background.type = LayerType::Background;

    LayersPanel panel;
    panel.setLayers({background});

    QVERIFY(panel.findChild<QLabel*>(QStringLiteral("lockLabel")) != nullptr);
    QVERIFY(panel.findChild<QWidget*>(QStringLiteral("dragHandle")) == nullptr);

    // Still no drag handle once selected - a locked row's own lock icon
    // always wins over the redesign's own selection-revealed handle.
    panel.selectLayer(static_cast<LayerId>(1));
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

void LayersPanelTest::addLayerButtonEmitsAddLayerRequested() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    QSignalSpy spy(&panel, &LayersPanel::addLayerRequested);

    auto* button = panel.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    QVERIFY(button != nullptr);
    button->click();

    QCOMPARE(spy.count(), 1);
}

void LayersPanelTest::addFilterLayerButtonEmitsAddFilterLayerRequested() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    QSignalSpy spy(&panel, &LayersPanel::addFilterLayerRequested);

    auto* button = panel.findChild<QPushButton*>(QStringLiteral("addFilterLayerButton"));
    QVERIFY(button != nullptr);
    button->click();

    QCOMPARE(spy.count(), 1);
}

void LayersPanelTest::selectLayerSelectsAMatchingRow() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());

    panel.selectLayer(static_cast<LayerId>(1));  // "Bottom".

    QVERIFY(panel.selectedLayerId().has_value());
    QCOMPARE(*panel.selectedLayerId(), static_cast<LayerId>(1));
    auto* list = panel.findChild<QListWidget*>(QStringLiteral("layersList"));
    QVERIFY(list != nullptr);
    QCOMPARE(list->currentRow(), 1);  // "Bottom" is displayed second.
}

void LayersPanelTest::selectLayerIsANoOpForAnUnknownId() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());

    panel.selectLayer(static_cast<LayerId>(999));

    QVERIFY(!panel.selectedLayerId().has_value());
}

void LayersPanelTest::selectLayerEmitsSelectionChanged() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    int emitCount = 0;
    std::optional<LayerId> received;
    connect(&panel, &LayersPanel::selectionChanged, [&](std::optional<LayerId> id) {
        ++emitCount;
        received = id;
    });

    panel.selectLayer(static_cast<LayerId>(1));

    QCOMPARE(emitCount, 1);
    QVERIFY(received.has_value());
    QCOMPARE(*received, static_cast<LayerId>(1));
}

void LayersPanelTest::clearSelectionEmitsSelectionChangedWithNullopt() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    panel.selectLayer(static_cast<LayerId>(1));
    int emitCount = 0;
    std::optional<LayerId> received = static_cast<LayerId>(1);
    connect(&panel, &LayersPanel::selectionChanged, [&](std::optional<LayerId> id) {
        ++emitCount;
        received = id;
    });

    panel.clearSelection();

    QCOMPARE(emitCount, 1);
    QVERIFY(!received.has_value());
}

void LayersPanelTest::setLayersEmitsSelectionChangedWhenTheSelectedLayerIsGone() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    panel.selectLayer(static_cast<LayerId>(1));
    int emitCount = 0;
    std::optional<LayerId> received = static_cast<LayerId>(1);
    connect(&panel, &LayersPanel::selectionChanged, [&](std::optional<LayerId> id) {
        ++emitCount;
        received = id;
    });

    std::vector<LayersPanel::RowData> rows = twoNormalLayers();
    rows.erase(rows.begin());  // Drops layer id 1 - the currently selected one.
    panel.setLayers(rows);

    QCOMPARE(emitCount, 1);
    QVERIFY(!received.has_value());
}

void LayersPanelTest::freshRowsOfferOnlyNoneUntilSetAvailableMindWavesIsCalled() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    panel.selectLayer(static_cast<LayerId>(1));  // the combo is a selection-revealed control.

    const auto combos = panel.findChildren<QComboBox*>(QStringLiteral("opacityMindWaveCombo"));
    QCOMPARE(combos.size(), 1);
    QCOMPARE(combos.at(0)->count(), 1);
    QCOMPARE(combos.at(0)->currentText(), QStringLiteral("None"));
}

void LayersPanelTest::setAvailableMindWavesPopulatesEveryRowsComboImmediately() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    panel.selectLayer(static_cast<LayerId>(1));

    // Called with no further setLayers() in between - see
    // setAvailableMindWaves()'s own docs on rebuilding rows immediately,
    // not waiting for the next unrelated layer refresh.
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")},
                                  {MindWaveId{6}, QStringLiteral("Fast Pulse")}});
    // rebuildRows()'s own row widgets are only scheduled via deleteLater()
    // (see rebuildRows()'s own docs) - the setLayers() call above and this
    // setAvailableMindWaves() call each rebuild the row widgets, leaving
    // the first pass's own combo still alive (pending deletion) unless the
    // event loop gets a chance to actually run it - QTest::qWait(0) does
    // that, matching this file's own established precedent elsewhere (see
    // setLayersReplacesThePreviousRows()).
    QTest::qWait(0);

    const auto combos = panel.findChildren<QComboBox*>(QStringLiteral("opacityMindWaveCombo"));
    QCOMPARE(combos.size(), 1);
    QCOMPARE(combos.at(0)->count(), 3);  // None + two MindWaves.
    QCOMPARE(combos.at(0)->itemText(1), QStringLiteral("Slow Pulse"));
    QCOMPARE(combos.at(0)->itemText(2), QStringLiteral("Fast Pulse"));
}

void LayersPanelTest::aRowsComboPreselectsItsOwnCurrentBinding() {
    LayersPanel panel;
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    auto rows = twoNormalLayers();
    rows[1].opacityMindWaveId = MindWaveId{5};  // "Top" (id 2).
    panel.setLayers(rows);

    panel.selectLayer(static_cast<LayerId>(2));  // "Top" - bound.
    QCOMPARE(panel.findChildren<QComboBox*>(QStringLiteral("opacityMindWaveCombo")).at(0)->currentText(),
             QStringLiteral("Slow Pulse"));

    panel.selectLayer(static_cast<LayerId>(1));  // "Bottom" - unbound.
    // rebuildRows() deletes the previous selection's own row widgets via
    // deleteLater() (see its own docs) - without waiting a tick, the
    // "Top" row's own now-stale combo would still be findable alongside
    // "Bottom"'s new one, at index 0 - see
    // setLayersReplacesThePreviousRows()'s own comment for the identical
    // timing issue.
    QTest::qWait(0);
    QCOMPARE(panel.findChildren<QComboBox*>(QStringLiteral("opacityMindWaveCombo")).at(0)->currentText(),
             QStringLiteral("None"));
}

void LayersPanelTest::changingARowsMindWaveComboEmitsOpacityMindWaveChanged() {
    LayersPanel panel;
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    panel.setLayers(twoNormalLayers());
    panel.selectLayer(static_cast<LayerId>(2));  // "Top".
    int emitCount = 0;
    std::optional<LayerId> receivedId;
    std::optional<MindWaveId> receivedMindWaveId;
    connect(&panel, &LayersPanel::opacityMindWaveChanged, [&](LayerId id, std::optional<MindWaveId> mindWaveId) {
        ++emitCount;
        receivedId = id;
        receivedMindWaveId = mindWaveId;
    });

    const auto combos = panel.findChildren<QComboBox*>(QStringLiteral("opacityMindWaveCombo"));
    combos.at(0)->setCurrentIndex(1);  // "Slow Pulse".

    QCOMPARE(emitCount, 1);
    QCOMPARE(receivedId, std::optional<LayerId>(static_cast<LayerId>(2)));
    QCOMPARE(receivedMindWaveId, std::optional<MindWaveId>(MindWaveId{5}));
}

void LayersPanelTest::selectingNoneEmitsOpacityMindWaveChangedWithNullopt() {
    LayersPanel panel;
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    auto rows = twoNormalLayers();
    rows[1].opacityMindWaveId = MindWaveId{5};  // "Top" (id 2), already bound.
    panel.setLayers(rows);
    panel.selectLayer(static_cast<LayerId>(2));
    int emitCount = 0;
    std::optional<MindWaveId> receivedMindWaveId = MindWaveId{5};
    connect(&panel, &LayersPanel::opacityMindWaveChanged, [&](LayerId, std::optional<MindWaveId> mindWaveId) {
        ++emitCount;
        receivedMindWaveId = mindWaveId;
    });

    const auto combos = panel.findChildren<QComboBox*>(QStringLiteral("opacityMindWaveCombo"));
    combos.at(0)->setCurrentIndex(0);  // "None".

    QCOMPARE(emitCount, 1);
    QVERIFY(!receivedMindWaveId.has_value());
}

void LayersPanelTest::contentIsInAResizableScrollAreaSoThePanelCanShrinkBelowItsFullHeight() {
    LayersPanel panel;

    // Matches every other dock panel's own established convention (see
    // e.g. FilterConfigurationPanel/ToolConfigurationPanel) - `widget()`
    // is a `QScrollArea` wrapping the panel's real content, not the
    // content widget directly, so the dock can be resized freely and a
    // scrollbar appears for whatever doesn't fit instead of the panel's
    // own layout forcing a tall minimum size.
    auto* scrollArea = qobject_cast<QScrollArea*>(panel.widget());
    QVERIFY(scrollArea != nullptr);
    QVERIFY(scrollArea->widgetResizable());
}

void LayersPanelTest::setDisallowedLayersMarksTheGivenRowsWithARedX() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());  // id 1 ("Bottom"), id 2 ("Top").

    panel.setDisallowedLayers({LayerId{1}});

    const auto marks = panel.findChildren<QLabel*>(QStringLiteral("mindGrainDisallowedLabel"));
    QCOMPARE(marks.size(), 1);
}

void LayersPanelTest::setDisallowedLayersLeavesOtherRowsUnmarked() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());

    panel.setDisallowedLayers({LayerId{1}});

    // "Top" (id 2) isn't in the disallowed list - its own row gets no mark
    // at all (not just a hidden one - see LayersPanel's own docs).
    const auto nameLabels = panel.findChildren<QLabel*>(QStringLiteral("nameLabel"));
    QCOMPARE(nameLabels.at(0)->text(), QStringLiteral("Top"));
    // "Top" is rendered first (top-first display order) - its own row
    // widget is the first LayerRowWidget child, which must contain no
    // mindGrainDisallowedLabel of its own.
    const auto marks = panel.findChildren<QLabel*>(QStringLiteral("mindGrainDisallowedLabel"));
    QCOMPARE(marks.size(), 1);  // Only "Bottom"'s row has one.
}

void LayersPanelTest::setDisallowedLayersWithAnEmptyListClearsEveryMark() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    panel.setDisallowedLayers({LayerId{1}, LayerId{2}});
    QCOMPARE(panel.findChildren<QLabel*>(QStringLiteral("mindGrainDisallowedLabel")).size(), 2);

    panel.setDisallowedLayers({});
    // setDisallowedLayers() rebuilds rows via deleteLater() (see
    // setLayersReplacesThePreviousRows()'s own docs) - spin the event loop
    // once so that's actually happened before counting children below.
    QTest::qWait(0);

    QVERIFY(panel.findChildren<QLabel*>(QStringLiteral("mindGrainDisallowedLabel")).empty());
}

void LayersPanelTest::backgroundLayerHasNoBlendModeCombo() {
    // Same reasoning as backgroundLayerHasNoOpacityOrTransformControls() -
    // the Background layer is always the floor of the stack, blended
    // against nothing beneath it, so a blend mode choice is meaningless
    // for it.
    LayersPanel::RowData background;
    background.id = 1;
    background.name = QStringLiteral("Background");
    background.type = LayerType::Background;

    LayersPanel panel;
    panel.setLayers({background});
    QVERIFY(panel.findChild<QComboBox*>(QStringLiteral("blendModeCombo")) == nullptr);

    panel.selectLayer(static_cast<LayerId>(1));
    QVERIFY(panel.findChild<QComboBox*>(QStringLiteral("blendModeCombo")) == nullptr);
}

void LayersPanelTest::aRowsBlendModeComboDefaultsToNormalAndPreselectsItsOwnValue() {
    LayersPanel panel;
    auto rows = twoNormalLayers();
    rows[1].blendMode = BlendMode::Multiply;  // "Top" (id 2).
    panel.setLayers(rows);

    panel.selectLayer(static_cast<LayerId>(2));  // "Top".
    QCOMPARE(panel.findChildren<QComboBox*>(QStringLiteral("blendModeCombo")).at(0)->currentText(),
             QStringLiteral("Multiply"));

    panel.selectLayer(static_cast<LayerId>(1));  // "Bottom" - default.
    // See aRowsComboPreselectsItsOwnCurrentBinding()'s own comment for why
    // this wait is needed - the "Top" row's own now-stale combo is only
    // actually gone once the event loop gets a chance to run.
    QTest::qWait(0);
    QCOMPARE(panel.findChildren<QComboBox*>(QStringLiteral("blendModeCombo")).at(0)->currentText(),
             QStringLiteral("Normal"));
}

void LayersPanelTest::changingARowsBlendModeComboEmitsBlendModeChanged() {
    LayersPanel panel;
    panel.setLayers(twoNormalLayers());
    panel.selectLayer(static_cast<LayerId>(1));  // "Bottom".
    int emitCount = 0;
    std::optional<LayerId> receivedId;
    std::optional<BlendMode> receivedMode;
    connect(&panel, &LayersPanel::blendModeChanged, [&](LayerId id, BlendMode mode) {
        ++emitCount;
        receivedId = id;
        receivedMode = mode;
    });

    const auto combos = panel.findChildren<QComboBox*>(QStringLiteral("blendModeCombo"));
    combos.at(0)->setCurrentIndex(combos.at(0)->findText(QStringLiteral("Screen")));

    QCOMPARE(emitCount, 1);
    QCOMPARE(receivedId, std::optional<LayerId>(static_cast<LayerId>(1)));
    QCOMPARE(receivedMode, std::optional<BlendMode>(BlendMode::Screen));
}

void LayersPanelTest::aRowsThumbnailIsShownWhenGivenAndAPlainBackgroundWhenNot() {
    auto rows = twoNormalLayers();
    rows[1].thumbnail = QImage(4, 4, QImage::Format_RGB888);  // "Top" (id 2) - any non-null image.
    rows[1].thumbnail.fill(Qt::red);
    // "Bottom" (id 1) keeps its own default-constructed, null thumbnail.

    LayersPanel panel;
    panel.setLayers(rows);

    const auto nameAreas = panel.findChildren<QWidget*>(QStringLiteral("nameArea"));
    QCOMPARE(nameAreas.size(), 2);
    // "Top" is displayed first (index 0) - see setLayersCreatesOneRowPerLayerTopFirst.
    QVERIFY(nameAreas.at(0)->findChild<QLabel*>(QStringLiteral("thumbnailLabel")) != nullptr);
    QVERIFY(nameAreas.at(1)->findChild<QLabel*>(QStringLiteral("thumbnailLabel")) == nullptr);

    // Regression test: QStackedLayout::StackAll still only raises its own
    // *current* widget (index 0 by default) in front of the rest - adding
    // thumbnailLabel at index 0 previously left it as the default-current,
    // and therefore frontmost, child, hiding nameLabel entirely behind it
    // for any row that actually has a thumbnail. nameLabel must be the
    // stack's own current widget regardless of whether a thumbnail exists.
    auto* stack = qobject_cast<QStackedLayout*>(nameAreas.at(0)->layout());
    QVERIFY(stack != nullptr);
    QCOMPARE(stack->currentWidget()->objectName(), QStringLiteral("nameLabel"));
}

void LayersPanelTest::mindWaveChildRowAppearsOnlyWhenBoundAndAPreviewImageExists() {
    auto rows = twoNormalLayers();
    rows[1].opacityMindWaveId = MindWaveId{5};  // "Top" (id 2) - bound, but no preview image set yet.

    LayersPanel panel;
    panel.setLayers(rows);

    // Bound, but setMindWavePreviewImages() was never called - no entry for
    // id 5 anywhere, so no child row yet (see setMindWavePreviewImages()'s
    // own docs on a missing entry).
    QVERIFY(panel.findChild<QLabel*>(QStringLiteral("mindWaveNameLabel")) == nullptr);

    QImage preview(4, 4, QImage::Format_RGB888);
    preview.fill(Qt::gray);
    panel.setMindWavePreviewImages({{MindWaveId{5}, preview}});

    auto* nameLabel = panel.findChild<QLabel*>(QStringLiteral("mindWaveNameLabel"));
    QVERIFY(nameLabel != nullptr);
    QVERIFY(panel.findChild<QLabel*>(QStringLiteral("mindWavePreviewLabel")) != nullptr);
}

void LayersPanelTest::mindWaveChildRowDisappearsWhenTheBindingIsCleared() {
    auto rows = twoNormalLayers();
    rows[1].opacityMindWaveId = MindWaveId{5};  // "Top" (id 2).

    LayersPanel panel;
    QImage preview(4, 4, QImage::Format_RGB888);
    preview.fill(Qt::gray);
    panel.setMindWavePreviewImages({{MindWaveId{5}, preview}});
    panel.setLayers(rows);
    QVERIFY(panel.findChild<QLabel*>(QStringLiteral("mindWaveNameLabel")) != nullptr);

    rows[1].opacityMindWaveId = std::nullopt;  // unbound.
    panel.setLayers(rows);
    QTest::qWait(0);  // rebuildRows() rebuilds via deleteLater() - see setLayersReplacesThePreviousRows().

    QVERIFY(panel.findChild<QLabel*>(QStringLiteral("mindWaveNameLabel")) == nullptr);
}
