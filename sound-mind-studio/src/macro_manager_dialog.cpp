#include "sound_mind/studio/macro_manager_dialog.h"

#include <algorithm>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace sound_mind::studio {

namespace {

constexpr int kMacroIdRole = Qt::UserRole + 1;

}  // namespace

MacroManagerDialog::MacroManagerDialog(sound_mind::core::Project& project, QWidget* parent)
    : QDialog(parent), project_(project) {
    setWindowTitle(tr("Macros"));
    auto* root = new QVBoxLayout(this);
    list_ = new QListWidget();
    list_->setObjectName(QStringLiteral("macroList"));
    root->addWidget(list_, 1);

    auto* buttons = new QHBoxLayout();
    renameButton_ = new QPushButton(tr("Rename..."));
    renameButton_->setObjectName(QStringLiteral("renameMacroButton"));
    deleteButton_ = new QPushButton(tr("Delete"));
    deleteButton_->setObjectName(QStringLiteral("deleteMacroButton"));
    buttons->addWidget(renameButton_);
    buttons->addWidget(deleteButton_);
    buttons->addStretch(1);
    root->addLayout(buttons);

    auto* close = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::accept);
    root->addWidget(close);

    connect(renameButton_, &QPushButton::clicked, this, &MacroManagerDialog::renameSelected);
    connect(deleteButton_, &QPushButton::clicked, this, &MacroManagerDialog::deleteSelected);
    connect(list_, &QListWidget::currentRowChanged, this, &MacroManagerDialog::updateButtons);
    resize(420, 320);
    refresh();
}

void MacroManagerDialog::refresh() {
    list_->clear();
    for (const sound_mind::core::Macro& macro : project_.macros()) {
        QString text = tr("%1 - %2 s, %3 event(s)")
                           .arg(QString::fromStdString(macro.name))
                           .arg(macro.durationSeconds, 0, 'f', 1)
                           .arg(macro.events.size());
        if (macro.recording) {
            text += tr(" (recording)");
        }
        auto* item = new QListWidgetItem(text, list_);
        item->setData(kMacroIdRole, static_cast<qulonglong>(macro.id));
    }
    if (list_->count() > 0) {
        list_->setCurrentRow(0);
    }
    updateButtons();
}

std::optional<sound_mind::core::MacroId> MacroManagerDialog::selectedId() const {
    const QListWidgetItem* item = list_->currentItem();
    if (item == nullptr) {
        return std::nullopt;
    }
    return static_cast<sound_mind::core::MacroId>(item->data(kMacroIdRole).toULongLong());
}

void MacroManagerDialog::updateButtons() {
    const auto id = selectedId();
    const sound_mind::core::Macro* macro = id ? project_.macroById(*id) : nullptr;
    const bool editable = macro != nullptr && !macro->recording;
    renameButton_->setEnabled(editable);
    deleteButton_->setEnabled(editable);
}

bool MacroManagerDialog::renameMacro(sound_mind::core::MacroId id, const QString& name) {
    sound_mind::core::Macro* macro = project_.macroById(id);
    if (macro == nullptr || macro->recording || name.trimmed().isEmpty()) {
        return false;
    }
    macro->name = name.trimmed().toStdString();
    changed_ = true;
    refresh();
    return true;
}

bool MacroManagerDialog::deleteMacro(sound_mind::core::MacroId id) {
    auto& macros = project_.macros();
    const auto it = std::find_if(macros.begin(), macros.end(),
                                 [id](const sound_mind::core::Macro& macro) { return macro.id == id; });
    if (it == macros.end() || it->recording) {
        return false;
    }
    macros.erase(it);
    changed_ = true;
    refresh();
    return true;
}

void MacroManagerDialog::renameSelected() {
    const auto id = selectedId();
    const sound_mind::core::Macro* macro = id ? project_.macroById(*id) : nullptr;
    if (macro == nullptr) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename Macro"), tr("Name:"), QLineEdit::Normal,
                                               QString::fromStdString(macro->name), &ok);
    if (ok) {
        renameMacro(*id, name);
    }
}

void MacroManagerDialog::deleteSelected() {
    const auto id = selectedId();
    const sound_mind::core::Macro* macro = id ? project_.macroById(*id) : nullptr;
    if (macro == nullptr) {
        return;
    }
    if (QMessageBox::question(this, tr("Delete Macro"),
                              tr("Delete \"%1\"?").arg(QString::fromStdString(macro->name))) == QMessageBox::Yes) {
        deleteMacro(*id);
    }
}

}  // namespace sound_mind::studio
