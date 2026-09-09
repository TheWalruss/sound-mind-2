#include "test_landing_page.h"

#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/landing_page.h"

using sound_mind::studio::LandingPage;

void LandingPageTest::showsTheEmbeddedLogo() {
    // Per the Visual Identity milestone (v0.Y.14.1): the header shows the
    // legacy Studio's logo, embedded via assets/app.qrc - see theme.h.
    LandingPage page;
    auto* logoLabel = page.findChild<QLabel*>(QStringLiteral("logoLabel"));
    QVERIFY(logoLabel != nullptr);
    QVERIFY(logoLabel->pixmap().isNull() == false);
}

void LandingPageTest::newProjectButtonEmitsNewProjectRequested() {
    LandingPage page;
    QSignalSpy spy(&page, &LandingPage::newProjectRequested);

    auto* button = page.findChild<QPushButton*>(QStringLiteral("newProjectButton"));
    QVERIFY(button != nullptr);
    button->click();

    QCOMPARE(spy.count(), 1);
}

void LandingPageTest::openProjectButtonEmitsOpenProjectRequested() {
    LandingPage page;
    QSignalSpy spy(&page, &LandingPage::openProjectRequested);

    auto* button = page.findChild<QPushButton*>(QStringLiteral("openProjectButton"));
    QVERIFY(button != nullptr);
    button->click();

    QCOMPARE(spy.count(), 1);
}

void LandingPageTest::setRecentProjectsWithNoPathsShowsNoClickableEntries() {
    LandingPage page;
    page.setRecentProjects({});

    const auto entries = page.findChildren<QPushButton*>(QStringLiteral("recentProjectButton"));
    QVERIFY(entries.isEmpty());
}

void LandingPageTest::setRecentProjectsCreatesEntriesThatEmitTheirPath() {
    LandingPage page;
    const std::filesystem::path pathA("C:/projects/a.smproj");
    const std::filesystem::path pathB("C:/projects/b.smproj");
    page.setRecentProjects({pathA, pathB});

    const auto entries = page.findChildren<QPushButton*>(QStringLiteral("recentProjectButton"));
    QCOMPARE(entries.size(), 2);

    QSignalSpy spy(&page, &LandingPage::recentProjectRequested);
    entries.first()->click();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString::fromStdString(pathA.string()));
}

void LandingPageTest::setRecentProjectsReplacesThePreviousEntries() {
    LandingPage page;
    page.setRecentProjects({std::filesystem::path("C:/projects/a.smproj")});
    QCOMPARE(page.findChildren<QPushButton*>(QStringLiteral("recentProjectButton")).size(), 1);

    page.setRecentProjects(
        {std::filesystem::path("C:/projects/b.smproj"), std::filesystem::path("C:/projects/c.smproj")});

    QCOMPARE(page.findChildren<QPushButton*>(QStringLiteral("recentProjectButton")).size(), 2);
}
