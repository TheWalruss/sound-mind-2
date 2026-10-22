#pragma once

#include <filesystem>

#include <QDialog>
#include <QString>

#include "sound_mind/codec/video_options.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QWidget;

namespace sound_mind::studio {

class IntegerField;
class NumericField;

/**
 * @brief Export Video / Export Macro Video settings (`v0.1.45.1`) -
 *        modelled on the legacy Studio's video export dialog.
 *
 * - **Output file**, with Browse...; the extension follows the format.
 * - **Format:** only those this PC can write (`availableVideoCodecs()`):
 *   MP4 H.264, MP4 H.265/HEVC, MOV ProRes 422, MP4 MPEG-4 Part 2.
 * - **Resolution:** YouTube 1080p/1440p/4K, 720p, vertical 1080x1920 and
 *   720x1280 (TikTok/Shorts), square 1080, the canvas's own size, or custom.
 *   The canvas is letterboxed into it.
 * - **Frame rate** 24/30/60 and **Quality** Low/Medium/High.
 * - **Animation:** the whole canvas with a sweeping playhead, or a scrolling
 *   window (its length in seconds, and whether to draw the playhead). In
 *   Sound Flower view a banner explains the polar equivalents.
 * - **Open file after export**.
 *
 * Choices other than the file are remembered between exports (QSettings).
 */
class VideoExportDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @param title The window title ("Export Video", "Export Macro Video").
     * @param suggestedPath The output file to start with; its extension is
     *        replaced to match the format.
     * @param polar Whether the canvas is in Sound Flower view - exported the
     *        same way.
     * @param canvasWidth The canvas's width (the "canvas size" choice).
     * @param canvasHeight Its height.
     * @param parent Standard QWidget parent.
     */
    VideoExportDialog(const QString& title, const std::filesystem::path& suggestedPath, bool polar,
                      std::uint32_t canvasWidth, std::uint32_t canvasHeight, QWidget* parent = nullptr);

    /// @brief The settings chosen.
    /// @return The options.
    [[nodiscard]] sound_mind::codec::VideoExportOptions options() const;

    /// @brief The output file.
    /// @return Its path.
    [[nodiscard]] std::filesystem::path outputPath() const;

    /// @brief Whether to open the file once exported.
    /// @return `true` if ticked.
    [[nodiscard]] bool openWhenDone() const;

    /// @brief Selects a format, if available (tests).
    /// @param codec The format.
    void setCodec(sound_mind::codec::VideoCodec codec);

    /// @brief Sets the output file (tests; Browse... does it otherwise).
    /// @param path The file.
    void setOutputPath(const std::filesystem::path& path);

    /// @brief Selects the scrolling animation and its window (tests).
    /// @param windowSeconds Seconds in view.
    void setScrolling(double windowSeconds);

    /// @brief Selects a resolution preset by its index (tests).
    /// @param index Into the presets, the last being Custom.
    void setResolutionPreset(int index);

    /// @brief Sets the custom size (tests).
    /// @param width Pixels.
    /// @param height Pixels.
    void setCustomSize(int width, int height);

    /// @brief Accepts the dialog if the output folder can be written to,
    /// remembering the choices for the next export; otherwise warns and
    /// stays open.
    void accept() override;

private slots:
    void browse();
    void updateForFormat();
    void updateVisibility();
    void validate();

private:
    void remember() const;
    void restore();

    bool polar_ = false;
    std::uint32_t canvasWidth_ = 0;
    std::uint32_t canvasHeight_ = 0;
    QLabel* polarBanner_ = nullptr;
    QLineEdit* pathEdit_ = nullptr;
    QComboBox* formatCombo_ = nullptr;
    QComboBox* resolutionCombo_ = nullptr;
    QWidget* customSize_ = nullptr;
    QLabel* customLabel_ = nullptr;
    IntegerField* customWidth_ = nullptr;
    IntegerField* customHeight_ = nullptr;
    QComboBox* frameRateCombo_ = nullptr;
    QButtonGroup* qualityGroup_ = nullptr;
    QRadioButton* wholeButton_ = nullptr;
    QRadioButton* scrollingButton_ = nullptr;
    QWidget* scrollingOptions_ = nullptr;
    NumericField* windowField_ = nullptr;
    QCheckBox* playheadCheck_ = nullptr;
    QCheckBox* openCheck_ = nullptr;
    QDialogButtonBox* buttons_ = nullptr;
};

}  // namespace sound_mind::studio
