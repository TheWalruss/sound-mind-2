#pragma once

#include <optional>
#include <vector>

#include <QWidget>

namespace sound_mind::studio {

/**
 * @brief The loudness meter's graph (Pre-Beta Plan `v0.Y.80.1`): momentary
 *        (thin) and short-term (thick) loudness over time, -60 to 0 LUFS,
 *        with a playhead line.
 * @note GUI thread only.
 */
class LoudnessGraphWidget : public QWidget {
    Q_OBJECT

public:
    /// @param parent The owning widget; may be `nullptr`.
    explicit LoudnessGraphWidget(QWidget* parent = nullptr);

    /**
     * @brief Shows two loudness series sampled every `stepSeconds`.
     * @param momentary Momentary readings, LUFS.
     * @param shortTerm Short-term readings, LUFS.
     * @param stepSeconds Seconds between readings.
     */
    void setSeries(std::vector<float> momentary, std::vector<float> shortTerm, double stepSeconds);

    /// @brief Moves (or, with `std::nullopt`, hides) the playhead line.
    /// @param seconds The playback position.
    void setPlayhead(std::optional<double> seconds);

    /// @brief Whether there is a series to draw.
    /// @return `true` after setSeries() with readings.
    [[nodiscard]] bool hasData() const noexcept { return !momentary_.empty(); }

    /// @brief A sensible default size for the panel layout.
    /// @return The size hint.
    [[nodiscard]] QSize sizeHint() const override { return {320, 120}; }

protected:
    /// @brief Draws the grid, both series and the playhead.
    /// @param event The paint event (unused).
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<float> momentary_;
    std::vector<float> shortTerm_;
    double stepSeconds_ = 0.1;
    std::optional<double> playhead_;
};

/**
 * @brief The loudness meter's per-band view (Pre-Beta Plan `v0.Y.80.1`): one
 *        row per frequency band (lowest at the bottom), one column per frame,
 *        each cell colored by level from -70 dB (dark) to 0 dB (bright), with
 *        a playhead line.
 * @note GUI thread only.
 */
class BandLevelsWidget : public QWidget {
    Q_OBJECT

public:
    /// @param parent The owning widget; may be `nullptr`.
    explicit BandLevelsWidget(QWidget* parent = nullptr);

    /**
     * @brief Shows `levels` - `bandCount` rows of `frameCount` values,
     *        row-major, lowest band first (see `bandLevelsOverTime()`).
     * @param levels The levels, dB.
     * @param bandCount How many bands.
     * @param frameCount How many frames.
     * @param secondsPerFrame Seconds per frame, for the playhead.
     */
    void setLevels(std::vector<float> levels, int bandCount, int frameCount, double secondsPerFrame);

    /// @brief Moves (or, with `std::nullopt`, hides) the playhead line.
    /// @param seconds The playback position.
    void setPlayhead(std::optional<double> seconds);

    /// @brief Whether there are levels to draw.
    /// @return `true` after setLevels() with data.
    [[nodiscard]] bool hasData() const noexcept { return !levels_.empty(); }

    /// @brief The color a level is drawn in - dark blue at -70 dB through
    ///        to bright yellow at 0 dB.
    /// @param levelDb The level.
    /// @return The color.
    [[nodiscard]] static QColor colorFor(float levelDb);

    /// @brief A sensible default size for the panel layout.
    /// @return The size hint.
    [[nodiscard]] QSize sizeHint() const override { return {320, 90}; }

protected:
    /// @brief Draws the band cells and the playhead.
    /// @param event The paint event (unused).
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<float> levels_;
    int bandCount_ = 0;
    int frameCount_ = 0;
    double secondsPerFrame_ = 0.0;
    std::optional<double> playhead_;
};

}  // namespace sound_mind::studio
