#include "sound_mind/studio/name_collision_dialog.h"

#include <optional>

#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

namespace sound_mind::studio {

namespace {

/// @brief Shared by promptSaveName()'s Rename loop and the initial entry -
/// `std::nullopt` on Cancel/dismiss.
std::optional<QString> askForName(QWidget* parent, const QString& dialogTitle, const QString& promptLabel,
                                   const QString& startingText) {
    bool ok = false;
    const QString name = QInputDialog::getText(parent, dialogTitle, promptLabel, QLineEdit::Normal, startingText, &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return std::nullopt;
    }
    return name.trimmed();
}

}  // namespace

SaveNameResult promptSaveName(QWidget* parent, const QString& dialogTitle, const QString& promptLabel,
                               const QString& initialName,
                               const std::function<bool(const std::string&)>& nameExists) {
    const std::optional<QString> candidate = askForName(parent, dialogTitle, promptLabel, initialName);
    if (!candidate.has_value()) {
        return SaveNameResult{SaveNameOutcome::Cancelled, {}, false};
    }
    return resolveNameCollision(parent, dialogTitle, promptLabel, *candidate, nameExists);
}

SaveNameResult resolveNameCollision(QWidget* parent, const QString& dialogTitle, const QString& promptLabel,
                                     const QString& candidateName,
                                     const std::function<bool(const std::string&)>& nameExists) {
    std::optional<QString> candidate = candidateName;

    for (;;) {
        const std::string candidateStd = candidate->toStdString();
        if (!nameExists(candidateStd)) {
            return SaveNameResult{SaveNameOutcome::Saved, candidateStd, false};
        }

        QMessageBox box(parent);
        box.setWindowTitle(dialogTitle);
        box.setText(QObject::tr("\"%1\" already exists. What would you like to do?").arg(*candidate));
        QPushButton* replaceButton = box.addButton(QObject::tr("Replace"), QMessageBox::AcceptRole);
        QPushButton* renameButton = box.addButton(QObject::tr("Rename..."), QMessageBox::ActionRole);
        box.addButton(QObject::tr("Cancel"), QMessageBox::RejectRole);
        box.setDefaultButton(replaceButton);
        box.exec();

        if (box.clickedButton() == replaceButton) {
            return SaveNameResult{SaveNameOutcome::Saved, candidateStd, true};
        }
        if (box.clickedButton() == renameButton) {
            candidate = askForName(parent, dialogTitle, promptLabel, *candidate);
            if (!candidate.has_value()) {
                return SaveNameResult{SaveNameOutcome::Cancelled, {}, false};
            }
            continue;
        }
        // Cancel, or the dialog was otherwise dismissed.
        return SaveNameResult{SaveNameOutcome::Cancelled, {}, false};
    }
}

ImportNameResolution resolveImportName(QWidget* parent, const QString& resourceTypeLabel,
                                        const std::string& desiredName,
                                        const std::function<bool(const std::string&)>& nameExists) {
    if (!nameExists(desiredName)) {
        return ImportNameResolution{ImportNameAction::AddNew, desiredName};
    }

    QMessageBox box(parent);
    box.setWindowTitle(QObject::tr("Import %1").arg(resourceTypeLabel));
    box.setText(QObject::tr("A %1 named \"%2\" already exists. What would you like to do?")
                    .arg(resourceTypeLabel, QString::fromStdString(desiredName)));
    QPushButton* overwriteButton = box.addButton(QObject::tr("Overwrite"), QMessageBox::AcceptRole);
    QPushButton* keepExistingButton = box.addButton(QObject::tr("Keep Existing"), QMessageBox::RejectRole);
    QPushButton* keepBothButton = box.addButton(QObject::tr("Keep Both"), QMessageBox::ActionRole);
    box.setDefaultButton(keepExistingButton);
    box.exec();

    if (box.clickedButton() == overwriteButton) {
        return ImportNameResolution{ImportNameAction::OverwriteExisting, desiredName};
    }
    if (box.clickedButton() == keepBothButton) {
        // The same auto-suffix shape Project::uniqueLayerName() already
        // establishes for Layers - " (2)", " (3)", ... until free.
        int suffix = 2;
        std::string candidate;
        do {
            candidate = desiredName + " (" + std::to_string(suffix) + ")";
            ++suffix;
        } while (nameExists(candidate));
        return ImportNameResolution{ImportNameAction::AddNew, candidate};
    }
    // Keep Existing, or the dialog was otherwise dismissed - both mean
    // "don't touch the existing entry."
    return ImportNameResolution{ImportNameAction::Skip, desiredName};
}

}  // namespace sound_mind::studio
