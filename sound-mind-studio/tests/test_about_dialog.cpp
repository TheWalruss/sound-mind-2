#include "test_about_dialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QtTest/QtTest>

#include "sound_mind/studio/about_dialog.h"

using sound_mind::studio::AboutDialog;

#ifndef SOUND_MIND_VERSION
#define SOUND_MIND_VERSION "unknown"
#endif

void AboutDialogTest::showsTheAppNameAndVersion() {
    const AboutDialog dialog;
    auto* versionLabel = dialog.findChild<QLabel*>(QStringLiteral("versionLabel"));
    QVERIFY(versionLabel != nullptr);
    QVERIFY(versionLabel->text().contains(QStringLiteral("Sound Mind Studio")));
    QVERIFY(versionLabel->text().contains(QStringLiteral(SOUND_MIND_VERSION)));
}

void AboutDialogTest::hasACloseButtonThatClosesTheDialog() {
    AboutDialog dialog;
    auto* buttonBox = dialog.findChild<QDialogButtonBox*>(QStringLiteral("buttonBox"));
    QVERIFY(buttonBox != nullptr);
    auto* closeButton = buttonBox->button(QDialogButtonBox::Close);
    QVERIFY(closeButton != nullptr);

    closeButton->click();

    // Close is a RejectRole button (see AboutDialog's own docs) - the
    // dialog closes either way, which is all MainWindow::showAboutDialog()
    // itself cares about.
    QCOMPARE(dialog.result(), QDialog::Rejected);
}
