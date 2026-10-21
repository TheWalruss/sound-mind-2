#pragma once

#include <QDialog>

#include "sound_mind/core/project.h"

class QListWidget;
class QPushButton;

namespace sound_mind::studio {

/**
 * @brief Macro → Manage Macros... (`v0.1.44.1`): lists the project's
 *        recorded Macros (name, length, event count) and renames or deletes
 *        them, directly in the project's library.
 *
 * A Macro being recorded is listed but can't be renamed or deleted.
 */
class MacroManagerDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @param project The project whose library to manage; must outlive the
     *        dialog.
     * @param parent Standard QWidget parent.
     */
    explicit MacroManagerDialog(sound_mind::core::Project& project, QWidget* parent = nullptr);

    /// @brief Whether anything was renamed or deleted.
    /// @return `true` once something was.
    [[nodiscard]] bool changed() const noexcept { return changed_; }

    /**
     * @brief Renames a Macro (the Rename... button, without its prompt).
     * @param id The Macro.
     * @param name The new name; ignored if blank.
     * @return `true` if renamed.
     */
    bool renameMacro(sound_mind::core::MacroId id, const QString& name);

    /**
     * @brief Deletes a Macro (the Delete button, without its confirmation).
     * @param id The Macro.
     * @return `true` if deleted.
     */
    bool deleteMacro(sound_mind::core::MacroId id);

private slots:
    void renameSelected();
    void deleteSelected();
    void updateButtons();

private:
    void refresh();
    [[nodiscard]] std::optional<sound_mind::core::MacroId> selectedId() const;

    sound_mind::core::Project& project_;
    QListWidget* list_ = nullptr;
    QPushButton* renameButton_ = nullptr;
    QPushButton* deleteButton_ = nullptr;
    bool changed_ = false;
};

}  // namespace sound_mind::studio
