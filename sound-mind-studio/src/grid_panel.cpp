#include "sound_mind/studio/grid_panel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QVariant>
#include <QWidget>

namespace sound_mind::studio {

GridPanel::GridPanel(QWidget* parent) : QDockWidget(tr("Grid"), parent) {
    auto* container = new QWidget(this);
    auto* form = new QFormLayout(container);

    verticalAxisCombo_ = new QComboBox(container);
    verticalAxisCombo_->setObjectName(QStringLiteral("verticalAxisCombo"));
    verticalAxisCombo_->addItem(tr("Off"), QVariant::fromValue(static_cast<int>(VerticalAxisLabelMode::Off)));
    verticalAxisCombo_->addItem(tr("Hz"), QVariant::fromValue(static_cast<int>(VerticalAxisLabelMode::Hertz)));
    verticalAxisCombo_->addItem(tr("Notes"), QVariant::fromValue(static_cast<int>(VerticalAxisLabelMode::Notes)));
    verticalAxisCombo_->addItem(tr("Bin index"),
                                 QVariant::fromValue(static_cast<int>(VerticalAxisLabelMode::BinIndex)));
    connect(verticalAxisCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        verticalAxisLabelMode_ = static_cast<VerticalAxisLabelMode>(verticalAxisCombo_->itemData(index).toInt());
        emit verticalAxisLabelModeChanged(verticalAxisLabelMode_);
    });
    form->addRow(tr("Vertical axis (frequency):"), verticalAxisCombo_);

    horizontalAxisCombo_ = new QComboBox(container);
    horizontalAxisCombo_->setObjectName(QStringLiteral("horizontalAxisCombo"));
    horizontalAxisCombo_->addItem(tr("Off"), QVariant::fromValue(static_cast<int>(HorizontalAxisLabelMode::Off)));
    horizontalAxisCombo_->addItem(tr("Seconds"),
                                   QVariant::fromValue(static_cast<int>(HorizontalAxisLabelMode::Seconds)));
    horizontalAxisCombo_->addItem(
        tr("Milliseconds"), QVariant::fromValue(static_cast<int>(HorizontalAxisLabelMode::Milliseconds)));
    horizontalAxisCombo_->addItem(tr("Frame index"),
                                   QVariant::fromValue(static_cast<int>(HorizontalAxisLabelMode::FrameIndex)));
    connect(horizontalAxisCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        horizontalAxisLabelMode_ =
            static_cast<HorizontalAxisLabelMode>(horizontalAxisCombo_->itemData(index).toInt());
        emit horizontalAxisLabelModeChanged(horizontalAxisLabelMode_);
    });
    form->addRow(tr("Horizontal axis (time):"), horizontalAxisCombo_);

    setWidget(container);
}

}  // namespace sound_mind::studio
