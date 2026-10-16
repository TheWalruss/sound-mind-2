#pragma once

#include <QDockWidget>

#include "sound_mind/studio/text_controller.h"

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLineEdit;
class QPushButton;

namespace sound_mind::studio {

class IntegerField;
class IntensityUnitSelector;
class NumericField;

/**
 * @brief The Text tool's panel (Pre-Beta Plan `v0.Y.78.1`): the text, its
 *        font and style, how loud it's written, and Apply/Reset - see
 *        `TextController` for the workflow.
 *
 * Purely presentational, like the other tool panels: every edit emits
 * settingsChanged() with the complete settings; `MainWindow` forwards them
 * to `TextController`. The two intensity fields share a dB/value toggle.
 *
 * @note GUI thread only.
 */
class TextToolPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @param parent The owning widget; may be `nullptr`.
    explicit TextToolPanel(QWidget* parent = nullptr);

    /// @brief The settings the controls describe.
    /// @return The complete settings.
    [[nodiscard]] TextSettings settings() const;

    /// @brief Shows `settings` without emitting settingsChanged().
    /// @param settings The settings to show.
    void setSettings(const TextSettings& settings);

    /// @brief Enables Apply/Reset only while a text box is placed.
    /// @param placed Whether a box is placed.
    void setBoxPlaced(bool placed);

signals:
    /// @brief Any control changed.
    /// @param settings The complete new settings.
    void settingsChanged(const sound_mind::studio::TextSettings& settings);

    /// @brief Apply was clicked.
    void applyRequested();

    /// @brief Reset was clicked.
    void resetRequested();

private:
    void emitSettings();

    QLineEdit* textEdit_ = nullptr;
    QFontComboBox* fontCombo_ = nullptr;
    IntegerField* sizeField_ = nullptr;
    QCheckBox* boldCheckBox_ = nullptr;
    QCheckBox* italicCheckBox_ = nullptr;
    IntensityUnitSelector* intensityUnit_ = nullptr;
    NumericField* leftIntensityField_ = nullptr;
    NumericField* rightIntensityField_ = nullptr;
    NumericField* opacityField_ = nullptr;
    QComboBox* blendModeCombo_ = nullptr;
    NumericField* rotationField_ = nullptr;
    QPushButton* applyButton_ = nullptr;
    QPushButton* resetButton_ = nullptr;
    bool loading_ = false;
};

}  // namespace sound_mind::studio
