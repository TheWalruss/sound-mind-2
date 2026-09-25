#include "sound_mind/studio/loop_panel.h"

#include <QCheckBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>

namespace sound_mind::studio {

LoopPanel::LoopPanel(QWidget* parent) : QDockWidget(tr("Loop"), parent) {
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    toggleButton_ = new QPushButton(tr("Start Loop"), container);
    toggleButton_->setObjectName(QStringLiteral("loopToggleButton"));
    toggleButton_->setCheckable(true);
    connect(toggleButton_, &QPushButton::clicked, this, &LoopPanel::toggleRequested);
    root->addWidget(toggleButton_);

    // Labeled "Freeze Loop" (not "Keep Looping" - confirmed with the user
    // as not descriptive of what it actually does): the standard
    // loop-pedal term for freezing whichever take is currently playing,
    // rather than recording over it every cycle. The internal name and API
    // (keepLoopingCheckBox, setKeepLooping()/keepLoopingChanged()) are
    // unchanged - only the visible label.
    keepLoopingCheckBox_ = new QCheckBox(tr("Freeze Loop"), container);
    keepLoopingCheckBox_->setObjectName(QStringLiteral("keepLoopingCheckBox"));
    connect(keepLoopingCheckBox_, &QCheckBox::toggled, this, &LoopPanel::keepLoopingChanged);
    root->addWidget(keepLoopingCheckBox_);

    root->addStretch();

    // Wrapped in a QScrollArea - per the confirmed scope for this
    // milestone - so this panel's content is never clipped, and never
    // forces the dock wider than the window, if it doesn't fit the
    // available height.
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);
}

void LoopPanel::setRunning(bool running) {
    toggleButton_->setChecked(running);
    toggleButton_->setText(running ? tr("Stop Loop") : tr("Start Loop"));
}

void LoopPanel::setKeepLoopingChecked(bool checked) {
    const QSignalBlocker blocker(keepLoopingCheckBox_);
    keepLoopingCheckBox_->setChecked(checked);
}

}  // namespace sound_mind::studio
