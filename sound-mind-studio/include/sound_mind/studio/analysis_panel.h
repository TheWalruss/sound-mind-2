#pragma once

#include <optional>

#include <QDockWidget>

#include "sound_mind/core/project_analysis.h"

class QLabel;
class QPushButton;
class QTabWidget;
class QTableWidget;

namespace sound_mind::studio {

class BandLevelsWidget;
class LoudnessGraphWidget;

/**
 * @brief The Analysis panel (Pre-Beta Plan `v0.Y.80.1`) - read-only meters
 *        that never change the project. This installment: the **Loudness**
 *        tab.
 *
 * **Loudness tab:**
 * - **Analyze** (analyzeRequested()) - `MainWindow` composites, decodes and
 *   measures the project off the GUI thread
 *   (`sound_mind::core::analyzeProjectLoudness()`) and hands the report to
 *   setReport().
 * - A summary of the mix: integrated loudness, the loudest short-term and
 *   momentary readings, true peak, sample peak, and a clip count that turns
 *   red when any sample would clip (true peak above 0 dBTP turns red too).
 * - The momentary and short-term readings **at the playback position**
 *   (setPlaybackPosition()).
 * - A loudness-over-time graph and a per-band view, both following the
 *   playhead.
 * - Each audible layer's own contribution: integrated, loudest short-term,
 *   true peak and clips.
 *
 * The report describes the project as it was when analyzed; setStale()
 * says so once it has changed since.
 *
 * @note GUI thread only.
 */
class AnalysisPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @param parent The owning widget; may be `nullptr`.
    explicit AnalysisPanel(QWidget* parent = nullptr);

    /// @brief Shows `report`, clearing any "out of date" note.
    /// @param report The analysis to show.
    void setReport(const sound_mind::core::ProjectLoudnessReport& report);

    /// @brief Forgets the report (e.g. another project was opened).
    void clearReport();

    /// @brief Notes that the project changed since the report was made -
    ///        a no-op with no report showing.
    void setStale();

    /// @brief Shows that an analysis is running (and disables Analyze).
    /// @param analyzing Whether one is running.
    void setAnalyzing(bool analyzing);

    /// @brief Moves the graphs' playhead and updates the readings at the
    ///        playback position. `std::nullopt` hides the playhead.
    /// @param seconds The playback position.
    void setPlaybackPosition(std::optional<double> seconds);

    /// @brief Whether a report is showing.
    /// @return `true` after setReport() until clearReport().
    [[nodiscard]] bool hasReport() const noexcept { return report_.has_value(); }

    /// @brief The loudness readings' text format: one decimal and the unit,
    ///        or "—" at the silence floor.
    /// @param value The value.
    /// @param unit "LUFS", "dBTP" or "dBFS".
    /// @return The formatted reading.
    [[nodiscard]] static QString formatLevel(double value, const QString& unit);

signals:
    /// @brief Analyze was clicked.
    void analyzeRequested();

private:
    QTabWidget* tabs_ = nullptr;
    QPushButton* analyzeButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* integratedLabel_ = nullptr;
    QLabel* maxShortTermLabel_ = nullptr;
    QLabel* maxMomentaryLabel_ = nullptr;
    QLabel* truePeakLabel_ = nullptr;
    QLabel* samplePeakLabel_ = nullptr;
    QLabel* clipsLabel_ = nullptr;
    QLabel* momentaryNowLabel_ = nullptr;
    QLabel* shortTermNowLabel_ = nullptr;
    LoudnessGraphWidget* graph_ = nullptr;
    BandLevelsWidget* bands_ = nullptr;
    QTableWidget* layerTable_ = nullptr;
    std::optional<sound_mind::core::ProjectLoudnessReport> report_;
};

}  // namespace sound_mind::studio
