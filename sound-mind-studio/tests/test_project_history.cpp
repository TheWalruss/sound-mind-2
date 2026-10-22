#include "test_project_history.h"

#include <memory>
#include <vector>

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/project_history.h"

using sound_mind::core::HistoryChange;
using sound_mind::core::Layer;
using sound_mind::core::LayerBase;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::studio::ProjectHistory;

namespace {

Project smallProject() {
    ProjectSettings settings;
    settings.canvasWidth = 16;
    settings.canvasHeight = 8;
    return Project::createNew(settings);
}

sound_mind::codec::StreamImage marked(const Project& project, float marker) {
    auto image = sound_mind::core::silentContentFor(project.settings());
    image.leftMagnitudeDb.at(0) = marker;
    return image;
}

}  // namespace

// v0.1.43.1 - the Studio's handle on a project's History.
void ProjectHistoryTest::startsEmptyWithoutAProject() {
    ProjectHistory history;
    QVERIFY(!history.canUndo());
    QVERIFY(!history.canRedo());
    QCOMPARE(history.count(), std::size_t{0});
    QCOMPARE(history.currentIndex(), std::size_t{0});
    QVERIFY(!history.undo());
}

void ProjectHistoryTest::commitRecordsAnEntryAndSignals() {
    Project project = smallProject();
    ProjectHistory history;
    QSignalSpy committed(&history, &ProjectHistory::entryCommitted);
    QSignalSpy historyChanged(&history, &ProjectHistory::historyChanged);

    project.layers().front().setOpacity(0.5f);
    const auto id = history.commit(project, QStringLiteral("Changed opacity"));

    QVERIFY(id.has_value());
    QCOMPARE(history.project(), &project);  // adopted on first commit.
    QCOMPARE(history.count(), std::size_t{1});
    QCOMPARE(history.descriptionAt(0), QStringLiteral("Changed opacity"));
    QCOMPARE(committed.count(), 1);
    QCOMPARE(historyChanged.count(), 1);
    QVERIFY(history.canUndo());
}

void ProjectHistoryTest::commitWithNoChangeRecordsNothing() {
    Project project = smallProject();
    ProjectHistory history;
    QSignalSpy committed(&history, &ProjectHistory::entryCommitted);
    QVERIFY(!history.commit(project, QStringLiteral("Nothing")).has_value());
    QCOMPARE(committed.count(), 0);
    QCOMPARE(history.count(), std::size_t{0});
}

void ProjectHistoryTest::undoRedoRebuildContentBeforeSignalling() {
    Project project = smallProject();
    ProjectHistory history;
    history.setProject(&project);
    Layer layer(0, "Imported", LayerType::Normal);
    layer.setBaseContent(marked(project, -3.0f));
    const LayerId id = project.addLayer(std::move(layer));
    history.commit(project, QStringLiteral("Imported"));
    project.layerById(id)->setBaseContent(marked(project, -8.0f));
    history.commit(project, QStringLiteral("Recorded over"));

    std::vector<QString> order;
    history.addContentRebuilder([&](const HistoryChange& change) {
        QVERIFY(change.contentLayers.contains(id));
        order.push_back(QStringLiteral("rebuild"));
    });
    connect(&history, &ProjectHistory::changed, this, [&](const HistoryChange&) {
        order.push_back(QStringLiteral("changed"));
    });

    QVERIFY(history.undo());
    QCOMPARE(project.layerById(id)->content()->leftMagnitudeDb.at(0), -3.0f);
    QVERIFY(history.redo());
    QCOMPARE(project.layerById(id)->content()->leftMagnitudeDb.at(0), -8.0f);
    QCOMPARE(order, (std::vector<QString>{QStringLiteral("rebuild"), QStringLiteral("changed"),
                                          QStringLiteral("rebuild"), QStringLiteral("changed")}));
}

void ProjectHistoryTest::jumpToReachesThePositionAndSignalsOnce() {
    Project project = smallProject();
    ProjectHistory history;
    history.setProject(&project);
    for (const float opacity : {0.9f, 0.8f, 0.7f}) {
        project.layers().front().setOpacity(opacity);
        history.commit(project, QStringLiteral("Opacity"));
    }
    QSignalSpy changed(&history, &ProjectHistory::changed);
    QVERIFY(history.jumpTo(0));
    QCOMPARE(history.currentIndex(), std::size_t{0});
    QCOMPARE(project.layers().front().opacity(), 1.0f);
    QCOMPARE(changed.count(), 1);
    QVERIFY(history.jumpTo(2));
    QCOMPARE(project.layers().front().opacity(), 0.8f);
}

void ProjectHistoryTest::undoWithoutARecoverableBaseFailsCleanly() {
    Project project = smallProject();
    ProjectHistory history;
    history.setProject(&project);
    Layer layer(0, "Imported", LayerType::Normal);
    layer.setBaseContent(marked(project, -1.0f));
    const LayerId id = project.addLayer(std::move(layer));
    history.commit(project, QStringLiteral("Imported"));
    project.layerById(id)->setBaseContent(marked(project, -2.0f));
    history.commit(project, QStringLiteral("Replaced"));
    project.history().setBaseBudgetBytes(0);  // forget every kept base.

    QSignalSpy changed(&history, &ProjectHistory::changed);
    QVERIFY(!history.undo());
    QCOMPARE(changed.count(), 0);
    QCOMPARE(history.currentIndex(), std::size_t{2});
    QCOMPARE(project.layerById(id)->content()->leftMagnitudeDb.at(0), -2.0f);
}

// v0.1.43.3 - entries that don't rebuild the project from its creation
// must say so, or a replay (Remaster) would silently produce something else.
void ProjectHistoryTest::aChangeMadeOutsideTheHistoryMarksItIncomplete() {
    Project project = smallProject();
    QVERIFY(project.history().startsAtCreation());
    ProjectHistory history;
    history.setProject(&project);
    QVERIFY(project.history().startsAtCreation());  // nothing done outside it.

    project.layers().front().setOpacity(0.5f);
    history.setProject(&project);  // attached again, with an uncommitted change.
    QVERIFY(!project.history().startsAtCreation());
    QVERIFY(!project.history().hasUncommittedChanges(project));
}

void ProjectHistoryTest::undoUsesTheBaseResolver() {
    Project project = smallProject();
    ProjectHistory history;
    history.setProject(&project);
    Layer layer(0, "Imported", LayerType::Normal);
    layer.setBaseContent(marked(project, -1.0f));
    const LayerId id = project.addLayer(std::move(layer));
    history.commit(project, QStringLiteral("Imported"));
    project.layerById(id)->setBaseContent(marked(project, -2.0f));
    history.commit(project, QStringLiteral("Replaced"));
    project.history().setBaseBudgetBytes(0);

    history.setBaseResolver([&](std::size_t position, LayerId layerId) -> std::optional<LayerBase> {
        if (position != 1 || layerId != id) {
            return std::nullopt;
        }
        return LayerBase{std::make_shared<const sound_mind::codec::StreamImage>(marked(project, -1.0f)), nullptr, 0};
    });
    QVERIFY(history.undo());
    QCOMPARE(project.layerById(id)->content()->leftMagnitudeDb.at(0), -1.0f);
}
