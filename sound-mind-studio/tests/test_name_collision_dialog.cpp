#include "test_name_collision_dialog.h"

#include <QAbstractButton>
#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <QtTest/QtTest>

#include "sound_mind/studio/name_collision_dialog.h"

using sound_mind::studio::ImportNameAction;
using sound_mind::studio::resolveImportName;
using sound_mind::studio::resolveNameCollision;
using sound_mind::studio::SaveNameOutcome;
using sound_mind::studio::promptSaveName;

namespace {

/// @brief Schedules `action` to run once the currently-blocking modal's
///        own nested event loop starts dispatching events - the same
///        `QTimer::singleShot(0, ...)` technique `test_main_window.cpp`'s
///        own `shownContextMenuActionTexts()` already establishes for
///        `QMenu::exec()`, applied here to `QInputDialog`/`QMessageBox`
///        (both reachable via `QApplication::activeModalWidget()` instead
///        of `activePopupWidget()`, since neither is a popup).
void onNextModal(std::function<void()> action) {
    QTimer::singleShot(0, std::move(action));
}

/// @brief Accepts the currently-open `QInputDialog`, typing `text` first.
void acceptActiveInputDialog(const QString& text) {
    auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
    QVERIFY(dialog != nullptr);
    dialog->setTextValue(text);
    dialog->accept();
}

/// @brief Rejects (cancels) the currently-open `QInputDialog`.
void rejectActiveInputDialog() {
    auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
    QVERIFY(dialog != nullptr);
    dialog->reject();
}

/// @brief Clicks whichever button on the currently-open `QMessageBox` has
///        the given `role` - `promptSaveName()`'s/`resolveImportName()`'s
///        own three choices are each a distinctly-roled button, not a
///        standard button, so lookup is by role, not by a `QMessageBox::
///        StandardButton` enumerator.
void clickActiveMessageBoxButtonWithRole(QMessageBox::ButtonRole role) {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    QVERIFY(box != nullptr);
    for (QAbstractButton* button : box->buttons()) {
        if (box->buttonRole(button) == role) {
            button->click();
            return;
        }
    }
    QFAIL("No button with the requested role was found.");
}

}  // namespace

void NameCollisionDialogTest::promptSaveNameWithNoCollisionReturnsImmediatelyWithNoDialog() {
    onNextModal([]() { acceptActiveInputDialog(QStringLiteral("Fresh Name")); });

    const auto result = promptSaveName(nullptr, QStringLiteral("Save"), QStringLiteral("Name:"),
                                        QStringLiteral("Fresh Name"), [](const std::string&) { return false; });

    QCOMPARE(result.outcome, SaveNameOutcome::Saved);
    QCOMPARE(QString::fromStdString(result.name), QStringLiteral("Fresh Name"));
    QVERIFY(!result.replacingExisting);
}

void NameCollisionDialogTest::promptSaveNameReplaceReturnsSavedWithReplacingExistingTrue() {
    onNextModal([]() {
        acceptActiveInputDialog(QStringLiteral("Taken"));
        onNextModal([]() { clickActiveMessageBoxButtonWithRole(QMessageBox::AcceptRole); });
    });

    const auto result = promptSaveName(nullptr, QStringLiteral("Save"), QStringLiteral("Name:"),
                                        QStringLiteral("Taken"),
                                        [](const std::string& name) { return name == "Taken"; });

    QCOMPARE(result.outcome, SaveNameOutcome::Saved);
    QCOMPARE(QString::fromStdString(result.name), QStringLiteral("Taken"));
    QVERIFY(result.replacingExisting);
}

void NameCollisionDialogTest::promptSaveNameRenameLoopsUntilACollisionFreeNameIsEntered() {
    onNextModal([]() {
        acceptActiveInputDialog(QStringLiteral("Taken"));
        onNextModal([]() {
            clickActiveMessageBoxButtonWithRole(QMessageBox::ActionRole);  // "Rename..."
            onNextModal([]() { acceptActiveInputDialog(QStringLiteral("Free Name")); });
        });
    });

    const auto result = promptSaveName(nullptr, QStringLiteral("Save"), QStringLiteral("Name:"),
                                        QStringLiteral("Taken"),
                                        [](const std::string& name) { return name == "Taken"; });

    QCOMPARE(result.outcome, SaveNameOutcome::Saved);
    QCOMPARE(QString::fromStdString(result.name), QStringLiteral("Free Name"));
    QVERIFY(!result.replacingExisting);
}

void NameCollisionDialogTest::promptSaveNameCancelOnTheCollisionDialogReturnsCancelled() {
    onNextModal([]() {
        acceptActiveInputDialog(QStringLiteral("Taken"));
        onNextModal([]() { clickActiveMessageBoxButtonWithRole(QMessageBox::RejectRole); });
    });

    const auto result = promptSaveName(nullptr, QStringLiteral("Save"), QStringLiteral("Name:"),
                                        QStringLiteral("Taken"),
                                        [](const std::string& name) { return name == "Taken"; });

    QCOMPARE(result.outcome, SaveNameOutcome::Cancelled);
    QVERIFY(result.name.empty());
}

void NameCollisionDialogTest::promptSaveNameCancellingTheInitialNamePromptReturnsCancelled() {
    onNextModal([]() { rejectActiveInputDialog(); });

    const auto result = promptSaveName(nullptr, QStringLiteral("Save"), QStringLiteral("Name:"),
                                        QStringLiteral("Taken"), [](const std::string&) { return true; });

    QCOMPARE(result.outcome, SaveNameOutcome::Cancelled);
}

void NameCollisionDialogTest::resolveNameCollisionWithACollisionFreeCandidateReturnsSavedImmediately() {
    // No dialog armed - a caller like MindCaptureDialog already obtained
    // this candidate from its own dialog, so no QInputDialog should show.
    const auto result = resolveNameCollision(nullptr, QStringLiteral("Capture Mind Shot"), QStringLiteral("Name:"),
                                              QStringLiteral("Fresh Name"), [](const std::string&) { return false; });

    QCOMPARE(result.outcome, SaveNameOutcome::Saved);
    QCOMPARE(QString::fromStdString(result.name), QStringLiteral("Fresh Name"));
    QVERIFY(!result.replacingExisting);
}

void NameCollisionDialogTest::resolveImportNameWithNoCollisionReturnsAddNewImmediatelyWithNoDialog() {
    // No dialog armed at all - a timeout/assertion failure inside a
    // QVERIFY above would be how an unexpected dialog shows up; the
    // absence of any onNextModal() call here is itself the assertion
    // that none appears.
    const auto result = resolveImportName(nullptr, QStringLiteral("Tool Preset"), "Fresh Name",
                                           [](const std::string&) { return false; });

    QCOMPARE(result.action, ImportNameAction::AddNew);
    QCOMPARE(QString::fromStdString(result.name), QStringLiteral("Fresh Name"));
}

void NameCollisionDialogTest::resolveImportNameOverwriteReturnsOverwriteExisting() {
    onNextModal([]() { clickActiveMessageBoxButtonWithRole(QMessageBox::AcceptRole); });

    const auto result = resolveImportName(nullptr, QStringLiteral("Tool Preset"), "Taken",
                                           [](const std::string& name) { return name == "Taken"; });

    QCOMPARE(result.action, ImportNameAction::OverwriteExisting);
    QCOMPARE(QString::fromStdString(result.name), QStringLiteral("Taken"));
}

void NameCollisionDialogTest::resolveImportNameKeepExistingReturnsSkip() {
    onNextModal([]() { clickActiveMessageBoxButtonWithRole(QMessageBox::RejectRole); });

    const auto result = resolveImportName(nullptr, QStringLiteral("Tool Preset"), "Taken",
                                           [](const std::string& name) { return name == "Taken"; });

    QCOMPARE(result.action, ImportNameAction::Skip);
}

void NameCollisionDialogTest::resolveImportNameKeepBothReturnsAFreshlySuffixedName() {
    onNextModal([]() { clickActiveMessageBoxButtonWithRole(QMessageBox::ActionRole); });

    const auto result = resolveImportName(nullptr, QStringLiteral("Tool Preset"), "Taken",
                                           [](const std::string& name) { return name == "Taken"; });

    QCOMPARE(result.action, ImportNameAction::AddNew);
    QCOMPARE(QString::fromStdString(result.name), QStringLiteral("Taken (2)"));
}

void NameCollisionDialogTest::resolveImportNameKeepBothSkipsAnAlreadyTakenSuffix() {
    onNextModal([]() { clickActiveMessageBoxButtonWithRole(QMessageBox::ActionRole); });

    const auto result =
        resolveImportName(nullptr, QStringLiteral("Tool Preset"), "Taken",
                           [](const std::string& name) { return name == "Taken" || name == "Taken (2)"; });

    QCOMPARE(result.action, ImportNameAction::AddNew);
    QCOMPARE(QString::fromStdString(result.name), QStringLiteral("Taken (3)"));
}
