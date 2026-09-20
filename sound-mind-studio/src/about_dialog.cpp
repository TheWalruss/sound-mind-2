#include "sound_mind/studio/about_dialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

namespace sound_mind::studio {

#ifndef SOUND_MIND_VERSION
#define SOUND_MIND_VERSION "unknown"
#endif

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("About Sound Mind Studio"));

    auto* root = new QVBoxLayout(this);

    auto* versionLabel = new QLabel(
        QStringLiteral("<b>Sound Mind Studio</b> v" SOUND_MIND_VERSION), this);
    versionLabel->setObjectName(QStringLiteral("versionLabel"));
    versionLabel->setTextFormat(Qt::RichText);
    versionLabel->setAlignment(Qt::AlignHCenter);
    root->addWidget(versionLabel);

    auto* descriptionLabel =
        new QLabel(tr("A spectrogram-as-image DAW and graphics studio."), this);
    descriptionLabel->setAlignment(Qt::AlignHCenter);
    descriptionLabel->setWordWrap(true);
    root->addWidget(descriptionLabel);

    auto* copyrightLabel = new QLabel(tr("© 2026 Sound Mind Project"), this);
    copyrightLabel->setAlignment(Qt::AlignHCenter);
    root->addWidget(copyrightLabel);

    // Close is a RejectRole button (QDialogButtonBox's own convention), so
    // clicking it emits rejected(), not accepted() - wired to reject()
    // accordingly, even though there's nothing here to actually "reject"
    // (a plain informational dialog, not a form) - MainWindow::
    // showAboutDialog() ignores exec()'s own return value either way.
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttonBox->setObjectName(QStringLiteral("buttonBox"));
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox);
}

}  // namespace sound_mind::studio
