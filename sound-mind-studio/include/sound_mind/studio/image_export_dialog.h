#pragma once

#include <filesystem>

#include <QDialog>
#include <QSize>
#include <QString>

#include "sound_mind/studio/import_export.h"

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QRadioButton;

namespace sound_mind::studio {

class IntegerField;

/**
 * @brief File -> Export Image... (`v0.1.52.1`): where to save a copy of the
 *        canvas, in which format, and at what size.
 *
 * - **File** - a path with a Browse... button; its extension and the
 *   **Format** choice (PNG, JPEG, BMP, TGA, WebP - the formats image import
 *   reads) follow each other.
 * - **Size** - **Full resolution** (the default: one pixel per canvas
 *   column and bin, shown as width x height) or a **Custom size**, whose
 *   height follows the width at the canvas's proportions unless **Keep
 *   proportions** is unticked.
 * - **As Sound Flower** - the polar view, a disk in a square image; starts
 *   ticked when the canvas is showing it. A Sound Flower's custom size is
 *   always square.
 *
 * OK is enabled only with a file name whose extension is one of the
 * formats.
 */
class ImageExportDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Builds the dialog.
     * @param suggestedPath The file to start with; its extension picks the
     *        starting format (PNG if it names none of them).
     * @param polar Whether the canvas is showing the Sound Flower view.
     * @param fullSize The flat canvas's full resolution - see
     *        fullResolutionImageSize().
     * @param fullPolarSize The Sound Flower's full resolution.
     * @param parent The owning widget.
     */
    ImageExportDialog(const std::filesystem::path& suggestedPath, bool polar, QSize fullSize, QSize fullPolarSize,
                      QWidget* parent = nullptr);

    /// @brief The file to write.
    /// @return Its path.
    [[nodiscard]] std::filesystem::path outputPath() const;

    /// @brief The size and view chosen.
    /// @return The options; width and height are `0` for full resolution.
    [[nodiscard]] ImageExportOptions options() const;

private slots:
    void browse();
    void formatChosen();
    void pathEdited();
    void widthChanged(int width);
    void heightChanged(int height);
    void updateState();

private:
    /// @brief The full resolution for the current view.
    [[nodiscard]] QSize currentFullSize() const;

    QSize fullSize_;
    QSize fullPolarSize_;
    QLineEdit* pathEdit_ = nullptr;
    QComboBox* formatCombo_ = nullptr;
    QCheckBox* polarCheckBox_ = nullptr;
    QRadioButton* fullResolutionRadio_ = nullptr;
    QRadioButton* customSizeRadio_ = nullptr;
    IntegerField* widthField_ = nullptr;
    IntegerField* heightField_ = nullptr;
    QCheckBox* keepProportionsCheckBox_ = nullptr;
    QDialogButtonBox* buttons_ = nullptr;
    /// @brief Set while one size field is updating the other.
    bool linking_ = false;
};

}  // namespace sound_mind::studio
