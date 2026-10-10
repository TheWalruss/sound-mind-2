#include "sound_mind/studio/landing_page.h"

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "sound_mind/studio/device_configuration_widget.h"

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
    // Packs the logo+title to the left rather than letting them stretch
    // to fill the row - see this class's own docs on the header being
    // left-aligned, not centered, as of the Header/Two-Column
    // Reorganization milestone.
    header->addStretch(1);
    return header;
}

/// @brief Wraps `content` in a borderless, resizable `QScrollArea` - the
/// Header/Two-Column Reorganization milestone's own "both columns get
/// scroll bars if the content doesn't fit" requirement, applied
/// identically to both the left and right columns.
/// @param objectName This scroll area's own object name, for tests to
///        find it by (`"leftColumnScrollArea"`/`"rightColumnScrollArea"`).
/// @param content The column's own content widget - already laid out,
///        just needs to become scrollable.
QScrollArea* makeColumnScrollArea(const QString& objectName, QWidget* content) {
    auto* scrollArea = new QScrollArea();
    scrollArea->setObjectName(objectName);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(content);
    return scrollArea;
}

}  // namespace

LandingPage::LandingPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);

    // Left-aligned header (see makeHeader()'s own docs on the trailing
    // stretch) - direct user feedback: "The header, left-aligned, has
    // the image glyph and 'SoundMind Studio'."
    root->addLayout(makeHeader(tr("Sound Mind Studio")));
    root->addSpacing(16);

    auto* columns = new QHBoxLayout();
    columns->setSpacing(24);
    root->addLayout(columns, 1);

    // --- Left column: New/Open Project + Recent Projects --------------
    auto* leftContent = new QWidget();
    auto* leftLayout = new QVBoxLayout(leftContent);

    auto* newButton = new QPushButton(tr("New Project..."));
    newButton->setObjectName(QStringLiteral("newProjectButton"));
    connect(newButton, &QPushButton::clicked, this, &LandingPage::newProjectRequested);
    leftLayout->addWidget(newButton, 0, Qt::AlignHCenter);

    auto* openButton = new QPushButton(tr("Open Project..."));
    openButton->setObjectName(QStringLiteral("openProjectButton"));
    connect(openButton, &QPushButton::clicked, this, &LandingPage::openProjectRequested);
    leftLayout->addWidget(openButton, 0, Qt::AlignHCenter);

    leftLayout->addSpacing(24);
    leftLayout->addWidget(makeHeading(tr("Recent Projects"), 2), 0, Qt::AlignHCenter);
    leftLayout->addWidget(makeSeparator());

    recentProjectsLayout_ = new QVBoxLayout();
    leftLayout->addLayout(recentProjectsLayout_);
    leftLayout->addStretch(1);

    columns->addWidget(makeColumnScrollArea(QStringLiteral("leftColumnScrollArea"), leftContent), 1);

    // --- Right column: Documentation + Device Configuration -----------
    auto* rightContent = new QWidget();
    auto* rightLayout = new QVBoxLayout(rightContent);

    rightLayout->addWidget(makeHeading(tr("Documentation"), 2), 0, Qt::AlignHCenter);
    rightLayout->addWidget(makeSeparator());

    auto* docsLayout = new QVBoxLayout();
    rightLayout->addLayout(docsLayout);

    auto addDocButton = [&](const QString& objectName, const QString& text, void (LandingPage::*signal)()) {
        auto* button = new QPushButton(text, this);
        button->setObjectName(objectName);
        button->setFlat(true);
        connect(button, &QPushButton::clicked, this, signal);
        docsLayout->addWidget(button, 0, Qt::AlignHCenter);
    };
    addDocButton(QStringLiteral("quickStartButton"), tr("Quick Start"), &LandingPage::quickStartRequested);
    addDocButton(QStringLiteral("readmeButton"), tr("Readme"), &LandingPage::readmeRequested);
    addDocButton(QStringLiteral("userGuideButton"), tr("User Guide"), &LandingPage::userGuideRequested);
    addDocButton(QStringLiteral("changelogButton"), tr("Changelog"), &LandingPage::changelogRequested);
    addDocButton(QStringLiteral("aboutButton"), tr("About"), &LandingPage::aboutRequested);

    // v0.Y.62.1 Installment H - lets a user pick/test audio devices before
    // even creating or opening a project. See the class's own docs on why
    // this is a second, independent DeviceConfigurationWidget instance,
    // not the dock's own.
    rightLayout->addSpacing(24);
    rightLayout->addWidget(makeHeading(tr("Device Configuration"), 2), 0, Qt::AlignHCenter);
    rightLayout->addWidget(makeSeparator());

    deviceConfiguration_ = new DeviceConfigurationWidget(rightContent);
    rightLayout->addWidget(deviceConfiguration_, 0, Qt::AlignHCenter);
    rightLayout->addStretch(1);

    columns->addWidget(makeColumnScrollArea(QStringLiteral("rightColumnScrollArea"), rightContent), 1);
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
