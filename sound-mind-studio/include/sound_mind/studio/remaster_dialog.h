#pragma once

#include <filesystem>
#include <map>

#include <QDialog>

#include "sound_mind/core/project.h"
#include "sound_mind/studio/remaster.h"

class QDialogButtonBox;
class QLineEdit;
class QTableWidget;

namespace sound_mind::studio {

class IntegerField;
class NumericField;

/**
 * @brief File → Remaster... (`v0.1.43.4`): choose new codec settings and/or
 *        replacement source files, and where to save the rebuilt project -
 *        see `remasterProject()`.
 *
 * Starts from the open project's own settings, so pressing OK straight away
 * rebuilds it as it is (useful for regenerating missing layer content). The
 * duration isn't editable: a remaster keeps it. OK is enabled once an
 * output path different from the original is given.
 */
class RemasterDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @param project The open project - its settings and sources prefill the
     *        dialog. Must outlive the dialog.
     * @param projectPath The open project's file, which the output must not
     *        overwrite.
     * @param parent Standard QWidget parent.
     */
    RemasterDialog(const sound_mind::core::Project& project, std::filesystem::path projectPath,
                   QWidget* parent = nullptr);

    /// @brief What the dialog's fields currently ask for.
    /// @return The request.
    [[nodiscard]] RemasterRequest request() const;

    /// @brief Sets the output path field (tests; Browse does it otherwise).
    /// @param path The new project's file.
    void setOutputPath(const std::filesystem::path& path);

    /// @brief Sets a replacement for a source (tests; the row's Replace...
    ///        button does it otherwise).
    /// @param id The source.
    /// @param path The replacement file.
    void setReplacement(sound_mind::core::SourceId id, const std::filesystem::path& path);

private slots:
    void browseForOutput();
    void updateOkEnabled();

private:
    void chooseReplacement(int row);

    const sound_mind::core::Project& project_;
    std::filesystem::path projectPath_;
    IntegerField* sampleRateField_ = nullptr;
    NumericField* timestepField_ = nullptr;
    IntegerField* binCountField_ = nullptr;
    IntegerField* minFrequencyField_ = nullptr;
    IntegerField* maxFrequencyField_ = nullptr;
    QTableWidget* sourcesTable_ = nullptr;
    QLineEdit* outputEdit_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
    std::map<sound_mind::core::SourceId, std::filesystem::path> replacements_;
};

}  // namespace sound_mind::studio
