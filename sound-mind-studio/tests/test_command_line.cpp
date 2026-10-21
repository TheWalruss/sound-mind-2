#include "test_command_line.h"

#include <QtTest/QtTest>

#include "sound_mind/studio/command_line.h"

using sound_mind::studio::projectToOpenFromArguments;

// v0.1.42.2 - opening a project passed on the command line (file association).
void CommandLineTest::findsTheProjectArgument() {
    const auto path = projectToOpenFromArguments(
        {QStringLiteral("sound-mind-studio.exe"), QStringLiteral("C:/Music/My Song.smproj")});
    QVERIFY(path.has_value());
    QCOMPARE(QString::fromStdWString(path->wstring()), QStringLiteral("C:/Music/My Song.smproj"));
    QVERIFY(projectToOpenFromArguments({QStringLiteral("app"), QStringLiteral("D:/A.SMPROJ")}).has_value());
}

void CommandLineTest::ignoresTheProgramNameAndOtherArguments() {
    QVERIFY(!projectToOpenFromArguments({}).has_value());
    QVERIFY(!projectToOpenFromArguments({QStringLiteral("C:/odd/name.smproj")}).has_value());  // the program itself.
    QVERIFY(!projectToOpenFromArguments({QStringLiteral("app"), QStringLiteral("-style"), QStringLiteral("fusion")})
                 .has_value());
    const auto path = projectToOpenFromArguments(
        {QStringLiteral("app"), QStringLiteral("song.wav"), QStringLiteral("b.smproj"), QStringLiteral("c.smproj")});
    QCOMPARE(QString::fromStdWString(path->wstring()), QStringLiteral("b.smproj"));  // the first one.
}

void CommandLineTest::keepsNonAsciiPaths() {
    const QString project = QStringLiteral("C:/Musik/L\u00e5t \u00f6ver \u00e4ng.smproj");
    const auto path = projectToOpenFromArguments({QStringLiteral("app"), project});
    QVERIFY(path.has_value());
    QCOMPARE(QString::fromStdWString(path->wstring()), project);
}
