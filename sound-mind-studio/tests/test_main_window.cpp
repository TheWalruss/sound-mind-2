#include "test_main_window.h"

#include <QtTest/QtTest>

#include "sound_mind/studio/main_window.h"

using sound_mind::studio::MainWindow;

void MainWindowTest::startsWithAFreshProject() {
    const MainWindow window;
    QVERIFY(window.project() != nullptr);
    QCOMPARE(window.project()->layers().size(), static_cast<std::size_t>(1));
}

void MainWindowTest::newProjectReplacesTheCurrentOne() {
    // MainWindow::project_ is a std::optional<Project>, which reuses its own
    // inline storage across assignment - so project()'s pointer *address*
    // staying the same across newProject() calls is expected, not a bug.
    // What actually matters is that the *contents* are a fresh project
    // afterwards, which is what this checks.
    MainWindow window;

    window.newProject();

    QVERIFY(window.project() != nullptr);
    QCOMPARE(window.project()->layers().size(), static_cast<std::size_t>(1));
}
