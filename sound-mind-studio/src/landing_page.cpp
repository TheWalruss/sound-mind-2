#include "sound_mind/studio/landing_page.h"

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace sound_mind::studio {

namespace {

/// @brief A bold, slightly larger label - used for the title and the
/// "Recent Projects" section heading.
QLabel* makeHeading(const QString& text, int extraPointSize) {
    auto* label = new QLabel(text);
    QFont font = label->font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + extraPointSize);
    label->setFont(font);
    return label;
}

QFrame* makeSeparator() {
    auto* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

/// @brief The logo + title row - see the class docs' `v0.Y.14.1` note.
/// Falls back to just the title (no logo) if the embedded resource
/// somehow failed to load, rather than showing a broken-image icon.
/// @param titleText Already-translated title text - a free function (no
///        `QObject` base) can't call `tr()` itself, so the constructor
///        does that and passes the result in.
QHBoxLayout* makeHeader(const QString& titleText) {
    auto* header = new QHBoxLayout();
    header->setSpacing(16);

    const QPixmap logo(QStringLiteral(":/ChooseAgainLarge.png"));
    if (!logo.isNull()) {
        auto* logoLabel = new QLabel();
        logoLabel->setObjectName(QStringLiteral("logoLabel"));
        logoLabel->setPixmap(logo.scaledToHeight(64, Qt::SmoothTransformation));
        header->addWidget(logoLabel, 0, Qt::AlignVCenter);
    }

    header->addWidget(makeHeading(titleText, 10), 0, Qt::AlignVCenter);
    return header;
}

}  // namespace

LandingPage::LandingPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(48, 40, 48, 40);
    root->addStretch(1);

    auto* header = makeHeader(tr("Sound Mind Studio"));
    root->addLayout(header);
    root->setAlignment(header, Qt::AlignHCenter);
    root->addSpacing(24);

    auto* newButton = new QPushButton(tr("New Project..."));
    newButton->setObjectName(QStringLiteral("newProjectButton"));
    connect(newButton, &QPushButton::clicked, this, &LandingPage::newProjectRequested);
    root->addWidget(newButton, 0, Qt::AlignHCenter);

    auto* openButton = new QPushButton(tr("Open Project..."));
    openButton->setObjectName(QStringLiteral("openProjectButton"));
    connect(openButton, &QPushButton::clicked, this, &LandingPage::openProjectRequested);
    root->addWidget(openButton, 0, Qt::AlignHCenter);

    root->addSpacing(24);
    root->addWidget(makeHeading(tr("Recent Projects"), 2), 0, Qt::AlignHCenter);
    root->addWidget(makeSeparator());

    recentProjectsLayout_ = new QVBoxLayout();
    root->addLayout(recentProjectsLayout_);

    root->addStretch(2);
}

void LandingPage::setRecentProjects(const std::vector<std::filesystem::path>& paths) {
    QLayoutItem* item = nullptr;
    while ((item = recentProjectsLayout_->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    for (const std::filesystem::path& path : paths) {
        const QString fullPath = QString::fromStdString(path.string());
        const QString displayName = QString::fromStdString(path.filename().string());

        auto* button = new QPushButton(displayName);
        button->setObjectName(QStringLiteral("recentProjectButton"));
        button->setFlat(true);
        button->setToolTip(fullPath);
        connect(button, &QPushButton::clicked, this,
                [this, fullPath]() { emit recentProjectRequested(fullPath); });
        recentProjectsLayout_->addWidget(button, 0, Qt::AlignHCenter);
    }
}

}  // namespace sound_mind::studio
