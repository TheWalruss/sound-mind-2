#include "test_about_dialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QFile>
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

// v0.1.42.1 - licence and notices.
void AboutDialogTest::showsTheCopyrightHolderAndLicenceSummary() {
    const AboutDialog dialog;
    auto* copyright = dialog.findChild<QLabel*>(QStringLiteral("copyrightLabel"));
    QVERIFY(copyright != nullptr);
    QVERIFY(copyright->text().contains(QStringLiteral("Andreas Lundberg")));
    QVERIFY(copyright->text().contains(QStringLiteral("2026")));
    QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("licenceSummaryLabel")) != nullptr);
}

void AboutDialogTest::theEmbeddedLicenceAndNoticesAreTheShippedFiles() {
    // The resources must be exactly the repository's LICENSE.md and
    // THIRD_PARTY_NOTICES.md, not stale copies.
    const auto readFile = [](const QString& path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    };
    const QString root = QStringLiteral(SOUND_MIND_SOURCE_DIR);
    QVERIFY(!AboutDialog::licenceText().isEmpty());
    QVERIFY(!AboutDialog::noticesText().isEmpty());
    QCOMPARE(AboutDialog::licenceText(), readFile(root + QStringLiteral("/LICENSE.md")));
    QCOMPARE(AboutDialog::noticesText(), readFile(root + QStringLiteral("/THIRD_PARTY_NOTICES.md")));
    QVERIFY(AboutDialog::licenceText().contains(QStringLiteral("Andreas Lundberg")));
    for (const char* component : {"Qt", "FFmpeg", "LAME", "JUCE", "PocketFFT", "Eigen", "FLAC"}) {
        QVERIFY2(AboutDialog::noticesText().contains(QString::fromLatin1(component)), component);
    }
}

void AboutDialogTest::licenceAndNoticesButtonsOpenReadableViewers() {
    AboutDialog dialog;
    dialog.findChild<QPushButton*>(QStringLiteral("licenceButton"))->click();
    auto* licence = dialog.findChild<QDialog*>(QStringLiteral("licenceViewer"));
    QVERIFY(licence != nullptr);
    QVERIFY(licence->findChild<QTextBrowser*>()->toPlainText().contains(QStringLiteral("Andreas Lundberg")));

    dialog.findChild<QPushButton*>(QStringLiteral("noticesButton"))->click();
    auto* notices = dialog.findChild<QDialog*>(QStringLiteral("noticesViewer"));
    QVERIFY(notices != nullptr);
    QVERIFY(notices->findChild<QTextBrowser*>()->toPlainText().contains(QStringLiteral("FFmpeg")));

    // A second click raises the open viewer rather than stacking another.
    dialog.findChild<QPushButton*>(QStringLiteral("licenceButton"))->click();
    QCOMPARE(dialog.findChildren<QDialog*>(QStringLiteral("licenceViewer")).size(), 1);
}

