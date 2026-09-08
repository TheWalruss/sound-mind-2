#pragma once

#include <QObject>

class RecentProjectsTest : public QObject {
    Q_OBJECT

private slots:
    void listIsEmptyForFreshSettings();
    void addThenListReturnsThatPath();
    void listFiltersOutPathsThatNoLongerExist();
    void addMovesAnAlreadyPresentPathToTheFront();
    void addTrimsToMaxEntries();
};
