#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QWidget>

#include "sound_mind/codec/audio_buffer.h"

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

/**
 * @brief The Stereo tab's graph (Pre-Beta Plan `v0.Y.80.1`): correlation
 *        (-1 at the bottom to +1 at the top, blue) and width (0 to 1, gold)
 *        over time, with the zero-correlation line and a playhead.
 * @note GUI thread only.
 */
class StereoGraphWidget : public QWidget {
    Q_OBJECT

public:
    /// @param parent The owning widget; may be `nullptr`.
    explicit StereoGraphWidget(QWidget* parent = nullptr);

    /**
     * @brief Shows correlation and width readings taken every `stepSeconds`.
     * @param correlation Correlation readings, `[-1, 1]`.
     * @param width Width readings, `[0, 1]`.
     * @param stepSeconds Seconds between readings.
     */
    void setSeries(std::vector<float> correlation, std::vector<float> width, double stepSeconds);

    /// @brief Moves (or, with `std::nullopt`, hides) the playhead line.
    /// @param seconds The playback position.
    void setPlayhead(std::optional<double> seconds);

    /// @brief Whether there is a series to draw.
    /// @return `true` after setSeries() with readings.
    [[nodiscard]] bool hasData() const noexcept { return !correlation_.empty(); }

    /// @brief A sensible default size for the panel layout.
    /// @return The size hint.
    [[nodiscard]] QSize sizeHint() const override { return {320, 100}; }

protected:
    /// @brief Draws the zero line, both series and the playhead.
    /// @param event The paint event (unused).
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<float> correlation_;
    std::vector<float> width_;
    double stepSeconds_ = 0.1;
    std::optional<double> playhead_;
};

/**
 * @brief A goniometer (Pre-Beta Plan `v0.Y.80.1`): each sample plotted as a
 *        point, side `(L-R)/sqrt(2)` across and mid `(L+R)/sqrt(2)` up, so
 *        mono is a vertical line, wide stereo a cloud, and out-of-phase
 *        content leans toward horizontal.
 *
 * Draws the `kWindowSamples` samples around the playback position, or -
 * with no position - points spread across the whole buffer.
 * @note GUI thread only.
 */
class GoniometerWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief How many samples are drawn around the playback position.
    static constexpr int kWindowSamples = 2048;

    /// @param parent The owning widget; may be `nullptr`.
    explicit GoniometerWidget(QWidget* parent = nullptr);

    /// @brief The audio to plot (shared, never modified); null clears it.
    /// @param audio The decoded mix.
    void setAudio(std::shared_ptr<const sound_mind::codec::AudioBuffer> audio);

    /// @brief Which moment to plot - see this class's docs.
    /// @param seconds The playback position, or `std::nullopt` for the whole buffer.
    void setPosition(std::optional<double> seconds);

    /// @brief The points the next paint draws, as (side, mid) pairs.
    /// @return The points; empty without audio.
    [[nodiscard]] std::vector<QPointF> points() const;

    /// @brief A sensible default size for the panel layout.
    /// @return The size hint.
    [[nodiscard]] QSize sizeHint() const override { return {160, 160}; }

protected:
    /// @brief Draws the axes and the points.
    /// @param event The paint event (unused).
    void paintEvent(QPaintEvent* event) override;

private:
    std::shared_ptr<const sound_mind::codec::AudioBuffer> audio_;
    std::optional<double> position_;
};

/**
 * @brief Per-band stereo width as horizontal bars, 0 (mono) to 1, one row
 *        per loudness band, lowest at the bottom (Pre-Beta Plan `v0.Y.80.1`).
 * @note GUI thread only.
 */
class BandWidthBarsWidget : public QWidget {
    Q_OBJECT

public:
    /// @param parent The owning widget; may be `nullptr`.
    explicit BandWidthBarsWidget(QWidget* parent = nullptr);

    /// @brief Shows one width per `loudnessBands()` entry.
    /// @param widths The widths, `[0, 1]`, lowest band first.
    void setWidths(std::vector<float> widths);

    /// @brief The widths shown.
    /// @return The widths, lowest band first.
    [[nodiscard]] const std::vector<float>& widths() const noexcept { return widths_; }

    /// @brief A sensible default size for the panel layout.
    /// @return The size hint.
    [[nodiscard]] QSize sizeHint() const override { return {320, 110}; }

protected:
    /// @brief Draws one labeled bar per band.
    /// @param event The paint event (unused).
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<float> widths_;
};

}  // namespace sound_mind::studio
