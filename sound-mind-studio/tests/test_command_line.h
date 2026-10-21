#pragma once

#include <QObject>

class CommandLineTest : public QObject {
    Q_OBJECT

private slots:
    void findsTheProjectArgument();
    void ignoresTheProgramNameAndOtherArguments();
    void keepsNonAsciiPaths();
};
