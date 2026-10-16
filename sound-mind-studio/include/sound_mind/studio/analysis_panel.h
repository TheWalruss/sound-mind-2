#pragma once

#include <optional>

#include <QDockWidget>

#include "sound_mind/core/project_analysis.h"

class QCheckBox;
class QLabel;
class QPushButton;
class QTabWidget;
class QTableWidget;

namespace sound_mind::studio {

class BandLevelsWidget;
class IntegerField;
class NumericField;
class BandWidthBarsWidget;
class GoniometerWidget;
class LoudnessGraphWidget;
class PitchGraphWidget;
class StereoGraphWidget;

/**
 * @brief The Analysis panel (Pre-Beta Plan `v0.Y.80.1`) - read-only meters
 *        that never change the project, in tabs: **Loudness**, **Stereo** and
 *        **Pitch**.
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
 * **Stereo tab** (installment B): overall correlation and width, both at
 * the playback position, a correlation/width graph over time, a goniometer
 * of the samples around the playhead (or the whole mix when stopped), and
 * per-band width bars. Negative correlation - channels that cancel when
 * summed to mono - is flagged in red.
 *
 * **Pitch tab** (installment C): the note at the playback position (name,
 * octave, cents and Hz - see `sound_mind::core::noteFor()`), a pitch-over-
 * time graph, and **Show pitch on canvas** (pitchOverlayToggled()), which
 * `MainWindow` forwards to `CanvasWidget::setPitchOverlay()`. **Ignore
 * spikes shorter than** (`v0.1.40.9`, ms, `0` = off) runs the report's raw
 * track through `sound_mind::core::removePitchSpikes()` before the graph,
 * the note readout and the overlay see it - applied at once, without
 * re-analyzing.
 *
 * **Several voices** (`v0.1.40.11`): **Voices** (1-6) sets how many
 * simultaneous pitches the analysis tracks (`detectPitches()`;
 * pitchVoiceCountChanged() - a new analysis applies it). With more than one
 * track in the report, a checkbox per voice ("Voice 1 (highest)" ... in its
 * graph/canvas color) shows or hides it everywhere - graph, readouts and
 * overlay. Hidden voices are remembered for the session by index. The
 * readouts list each shown, sounding voice as "1: A4 +3 cents (440.8 Hz)",
 * one per line; with a single track, just the note. **At mouse** shows the
 * same for the time under the mouse on the canvas or the pitch graph.
 *
 * **Above the tabs** (`v0.1.40.7`), so they're at hand on every tab:
 * **Analyze**, **Auto-update** (autoAnalyzeToggled() - `MainWindow` then
 * re-analyzes after every change, cancelling a run that's in progress) and
 * the status line.
 *
 * **Canvas overlays** (`v0.1.40.7`, besides the pitch one): **Show loudness
 * on canvas** on the Loudness tab (loudnessOverlayToggled()) and **Show
 * correlation and width on canvas** on the Stereo tab
 * (stereoOverlayToggled()), forwarded by `MainWindow` to
 * `CanvasWidget::setLoudnessOverlay()`/`setStereoOverlay()`. Each overlay
 * signal is re-emitted with every new report while its box is checked, and
 * with empty data when the report is cleared.
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

    /// @brief Whether Auto-update is checked.
    /// @return The box's state.
    [[nodiscard]] bool autoAnalyze() const noexcept;

    /// @brief Checks or unchecks Auto-update without emitting
    ///        autoAnalyzeToggled() (e.g. restoring the saved setting).
    /// @param enabled Whether to check it.
    void setAutoAnalyze(bool enabled);

    /// @brief Sets "Voices" without emitting pitchVoiceCountChanged()
    ///        (e.g. restoring the saved setting).
    /// @param voices How many pitches to track, clamped to [1, 6].
    void setPitchVoiceCount(int voices);

    /// @brief The "Voices" setting.
    /// @return How many pitches to track.
    [[nodiscard]] int pitchVoiceCount() const noexcept;

    /// @brief Shows the pitch (or pitches) at `seconds` in the Pitch tab's
    ///        "At mouse" readout - fed by the canvas's and the pitch graph's
    ///        horizontal mouse position (`v0.1.40.11`).
    /// @param seconds The time under the mouse; `std::nullopt` clears it.
    void setMouseTime(std::optional<double> seconds);

    /// @brief Sets "Ignore spikes shorter than" and re-applies it to the
    ///        displayed pitch, without emitting pitchSpikeFilterChanged()
    ///        (e.g. restoring the saved setting).
    /// @param milliseconds The limit; `0` shows the raw track.
    void setPitchSpikeFilterMs(double milliseconds);

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

    /**
     * @brief "Show pitch on canvas" was toggled.
     * @param enabled Whether to show the track.
     * @param tracks The displayed pitch tracks (empty with no report, or
     *        when turned off; a hidden voice's track is empty).
     */
    void pitchOverlayToggled(bool enabled, const std::vector<std::vector<float>>& tracks);

    /// @brief "Voices" was changed by the user (`v0.1.40.11`) - takes a
    ///        new analysis to apply.
    /// @param voices How many pitches to track.
    void pitchVoiceCountChanged(int voices);

    /// @brief "Ignore spikes shorter than" was changed by the user.
    /// @param milliseconds The new limit.
    void pitchSpikeFilterChanged(double milliseconds);

    /// @brief Auto-update was toggled by the user.
    /// @param enabled Whether to re-analyze after every change.
    void autoAnalyzeToggled(bool enabled);

    /**
     * @brief "Show loudness on canvas" was toggled, or the report changed
     *        while it's checked.
     * @param enabled Whether to show it.
     * @param momentaryLufs The mix's momentary readings (empty when off or
     *        with no report).
     * @param shortTermLufs Its short-term readings (likewise).
     */
    void loudnessOverlayToggled(bool enabled, const std::vector<float>& momentaryLufs,
                                const std::vector<float>& shortTermLufs);

    /**
     * @brief "Show correlation and width on canvas" was toggled, or the
     *        report changed while it's checked.
     * @param enabled Whether to show it.
     * @param correlation The mix's correlation readings (empty when off or
     *        with no report).
     * @param width Its width readings (likewise).
     */
    void stereoOverlayToggled(bool enabled, const std::vector<float>& correlation, const std::vector<float>& width);

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
    QLabel* correlationLabel_ = nullptr;
    QLabel* widthLabel_ = nullptr;
    QLabel* correlationNowLabel_ = nullptr;
    QLabel* widthNowLabel_ = nullptr;
    StereoGraphWidget* stereoGraph_ = nullptr;
    GoniometerWidget* goniometer_ = nullptr;
    BandWidthBarsWidget* bandWidths_ = nullptr;
    QLabel* noteNowLabel_ = nullptr;
    PitchGraphWidget* pitchGraph_ = nullptr;
    QCheckBox* pitchOverlayCheckBox_ = nullptr;
    QCheckBox* autoAnalyzeCheckBox_ = nullptr;
    NumericField* pitchSpikeFilterField_ = nullptr;
    IntegerField* pitchVoiceCountField_ = nullptr;
    QWidget* voiceList_ = nullptr;
    std::vector<QCheckBox*> voiceCheckBoxes_;
    /// @brief Voices hidden by index - kept across reports for the session.
    std::vector<bool> hiddenVoices_;
    QLabel* noteMouseLabel_ = nullptr;
    std::optional<double> mouseSeconds_;
    /// @brief The report's pitch tracks with spikes removed and hidden
    ///        voices emptied - what the Pitch tab and its overlay show.
    std::vector<std::vector<float>> displayedPitch_;
    /// @brief Rebuilds the voice checkboxes for the report's track count.
    void rebuildVoiceList();
    /// @brief The readout text for the displayed pitches at `seconds`.
    [[nodiscard]] QString notesAt(double seconds) const;
    /// @brief The last setPlaybackPosition(), re-read when the track changes.
    std::optional<double> playbackSeconds_;
    /// @brief Recomputes displayedPitch_ and refreshes everything showing it.
    void refreshDisplayedPitch();
    QCheckBox* loudnessOverlayCheckBox_ = nullptr;
    QCheckBox* stereoOverlayCheckBox_ = nullptr;

    /// @brief Re-emits pitchOverlayToggled() with the current track.
    void emitPitchOverlay();
    /// @brief Re-emits loudnessOverlayToggled() with the current readings.
    void emitLoudnessOverlay();
    /// @brief Re-emits stereoOverlayToggled() with the current readings.
    void emitStereoOverlay();
    /// @brief Re-emits all three overlay signals.
    void emitOverlays();
    std::optional<sound_mind::core::ProjectLoudnessReport> report_;
};

}  // namespace sound_mind::studio
