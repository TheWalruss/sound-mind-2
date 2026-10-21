#include "sound_mind/studio/about_dialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace sound_mind::studio {

#ifndef SOUND_MIND_VERSION
#define SOUND_MIND_VERSION "unknown"
#endif

namespace {

QString readResource(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

}  // namespace

QString AboutDialog::licenceText() { return readResource(QStringLiteral(":/legal/LICENSE.md")); }

QString AboutDialog::noticesText() { return readResource(QStringLiteral(":/legal/THIRD_PARTY_NOTICES.md")); }

QString AboutDialog::copyrightLine() { return tr("Copyright \u00A9 2026 Andreas Lundberg. All rights reserved."); }

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("About Sound Mind Studio"));

    auto* root = new QVBoxLayout(this);

    auto* versionLabel = new QLabel(QStringLiteral("<b>Sound Mind Studio</b> v" SOUND_MIND_VERSION), this);
    versionLabel->setObjectName(QStringLiteral("versionLabel"));
    versionLabel->setTextFormat(Qt::RichText);
    versionLabel->setAlignment(Qt::AlignHCenter);
    root->addWidget(versionLabel);

    auto* descriptionLabel = new QLabel(tr("A spectrogram-as-image DAW and graphics studio."), this);
    descriptionLabel->setAlignment(Qt::AlignHCenter);
    descriptionLabel->setWordWrap(true);
    root->addWidget(descriptionLabel);

    auto* copyrightLabel = new QLabel(copyrightLine(), this);
    copyrightLabel->setObjectName(QStringLiteral("copyrightLabel"));
    copyrightLabel->setAlignment(Qt::AlignHCenter);
    root->addWidget(copyrightLabel);

    auto* licenceLabel = new QLabel(
        tr("Free to use, including for commercial work - what you make with it is yours. Pre-release software, "
           "provided as is. Includes open-source components under their own licences."),
        this);
    licenceLabel->setObjectName(QStringLiteral("licenceSummaryLabel"));
    licenceLabel->setAlignment(Qt::AlignHCenter);
    licenceLabel->setWordWrap(true);
    root->addWidget(licenceLabel);

    auto* documents = new QHBoxLayout();
    auto* licenceButton = new QPushButton(tr("Licence..."), this);
    licenceButton->setObjectName(QStringLiteral("licenceButton"));
    connect(licenceButton, &QPushButton::clicked, this, [this]() {
        showDocument(QStringLiteral("licenceViewer"), tr("Sound Mind Studio Licence"), licenceText());
    });
    auto* noticesButton = new QPushButton(tr("Open-source notices..."), this);
    noticesButton->setObjectName(QStringLiteral("noticesButton"));
    connect(noticesButton, &QPushButton::clicked, this, [this]() {
        showDocument(QStringLiteral("noticesViewer"), tr("Open-Source Notices"), noticesText());
    });
    documents->addStretch();
    documents->addWidget(licenceButton);
    documents->addWidget(noticesButton);
    documents->addStretch();
    root->addLayout(documents);

    // Close is a RejectRole button (QDialogButtonBox's own convention), so
    // clicking it emits rejected(), not accepted() - wired to reject()
    // accordingly; MainWindow::showAboutDialog() ignores exec()'s result.
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttonBox->setObjectName(QStringLiteral("buttonBox"));
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox);
}

void AboutDialog::showDocument(const QString& objectName, const QString& title, const QString& markdown) {
    if (auto* existing = findChild<QDialog*>(objectName)) {
        existing->raise();
        existing->activateWindow();
        return;
    }
    auto* viewer = new QDialog(this);
    viewer->setObjectName(objectName);
    viewer->setWindowTitle(title);
    viewer->setAttribute(Qt::WA_DeleteOnClose);
    viewer->resize(720, 600);
    auto* layout = new QVBoxLayout(viewer);
    auto* browser = new QTextBrowser(viewer);
    browser->setOpenExternalLinks(true);
    browser->setMarkdown(markdown.isEmpty() ? tr("This document is missing from this copy of the program.")
                                            : markdown);
    layout->addWidget(browser);
    auto* close = new QDialogButtonBox(QDialogButtonBox::Close, viewer);
    connect(close, &QDialogButtonBox::rejected, viewer, &QDialog::close);
    layout->addWidget(close);
    viewer->show();
}

}  // namespace sound_mind::studio
