#include "sound_mind/studio/checklist_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace sound_mind::studio {

CheckListDialog::CheckListDialog(const QString& title, const QStringList& labels,
                                   const std::vector<bool>& initiallyChecked, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(title);

    auto* root = new QVBoxLayout(this);

    auto* selectAllButton = new QPushButton(tr("Select All"), this);
    auto* deselectAllButton = new QPushButton(tr("Deselect All"), this);
    auto* buttonsRow = new QHBoxLayout();
    buttonsRow->addWidget(selectAllButton);
    buttonsRow->addWidget(deselectAllButton);
    buttonsRow->addStretch();
    root->addLayout(buttonsRow);

    auto* listContainer = new QWidget(this);
    auto* listLayout = new QVBoxLayout(listContainer);
    checkBoxes_.reserve(static_cast<std::size_t>(labels.size()));
    for (int i = 0; i < labels.size(); ++i) {
        auto* checkBox = new QCheckBox(labels.at(i), listContainer);
        checkBox->setObjectName(QStringLiteral("checkListCheckBox%1").arg(i));
        checkBox->setChecked(static_cast<std::size_t>(i) < initiallyChecked.size() && initiallyChecked[static_cast<std::size_t>(i)]);
        listLayout->addWidget(checkBox);
        checkBoxes_.push_back(checkBox);
    }
    listLayout->addStretch();

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(listContainer);
    scrollArea->setWidgetResizable(true);
    root->addWidget(scrollArea);

    connect(selectAllButton, &QPushButton::clicked, this, [this]() {
        for (QCheckBox* checkBox : checkBoxes_) {
            checkBox->setChecked(true);
        }
    });
    connect(deselectAllButton, &QPushButton::clicked, this, [this]() {
        for (QCheckBox* checkBox : checkBoxes_) {
            checkBox->setChecked(false);
        }
    });

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox);

    resize(300, 400);
}

std::vector<bool> CheckListDialog::checkedStates() const {
    std::vector<bool> states;
    states.reserve(checkBoxes_.size());
    for (const QCheckBox* checkBox : checkBoxes_) {
        states.push_back(checkBox->isChecked());
    }
    return states;
}

}  // namespace sound_mind::studio
