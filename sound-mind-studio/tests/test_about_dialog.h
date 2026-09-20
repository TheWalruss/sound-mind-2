#pragma once

#include <QObject>

class AboutDialogTest : public QObject {
    Q_OBJECT

private slots:
    void showsTheAppNameAndVersion();
    void hasACloseButtonThatClosesTheDialog();
};
