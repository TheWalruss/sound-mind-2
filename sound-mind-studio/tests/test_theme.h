#pragma once

#include <QObject>

class ThemeTest : public QObject {
    Q_OBJECT

private slots:
    void studioStyleSheetIsNotEmpty();
    void studioStyleSheetContainsBothBrandColors();
    void studioWindowIconIsNotNull();
};
