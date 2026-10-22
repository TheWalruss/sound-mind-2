#pragma once

#include <functional>

#include <QObject>
#include <QTimer>

#include "sound_mind/core/project.h"
#include "sound_mind/studio/background_job.h"

class QSettings;

namespace sound_mind::studio {

class AnalysisPanel;

/**
 * @brief Keeps the Analysis panel's report in step with the project
 *        (`v0.1.48.5`, split out of `MainWindow`): runs the loudness,
 *        stereo and pitch analysis on a BackgroundJob, marks the report out
 *        of date on every audible change, and with Auto-update on re-runs it
 *        once edits pause (`kAutoAnalyzeDelayMs`), cancelling a run an edit
 *        has already made stale.
 *
 * Owns the panel's analysis wiring - Analyze, Auto-update, the pitch voice
 * count and spike filter (both remembered in the settings). The panel's
 * overlays draw on the canvas and stay with the window.
 *
 * @note UI thread only.
 */
class AnalysisController : public QObject {
    Q_OBJECT

public:
    /// @brief How long Auto-update waits after the last change, ms.
    static constexpr int kAutoAnalyzeDelayMs = 300;

    /**
     * @brief Wires `panel` and restores Auto-update, the voice count and
     *        the spike filter from `settings`.
     * @param panel The Analysis panel.
     * @param project The open project, or `nullptr` - asked each time.
     * @param settings Where the panel's choices are remembered.
     * @param parent The owning QObject.
     */
    AnalysisController(AnalysisPanel* panel, std::function<const sound_mind::core::Project*()> project,
                       QSettings* settings, QObject* parent = nullptr);

    /// @brief Cancels a running analysis and waits; its report is dropped.
    ~AnalysisController() override;

    /// @brief Analyzes the project now (Analyze, or Auto-update's timer):
    ///        shows the panel unless Auto-update is on. A no-op with no
    ///        project or while a run is going.
    void analyze();

    /**
     * @brief Something audible changed: the report is out of date. With
     *        Auto-update on, a running analysis is cancelled (counted - see
     *        cancelledCount()) and the debounce restarts.
     */
    void noteProjectChanged();

    /// @brief A setting the report depends on changed (the voice count):
    ///        cancels a running analysis and re-runs, even with Auto-update
    ///        off.
    void requestReanalysis();

    /// @brief Turns Auto-update on or off (remembered); turning it on with
    ///        no report, or an out-of-date one, analyzes at once.
    /// @param enabled Whether to re-analyze after edits.
    void setAutoAnalyze(bool enabled);

    /// @brief A different project: drops the report and any running
    ///        analysis, and with Auto-update on analyzes the new one shortly.
    void projectChanged();

    /// @brief Whether an analysis is running.
    /// @return `true` until its report has been shown (or dropped).
    [[nodiscard]] bool isAnalyzing() const noexcept { return job_.isRunning(); }

    /// @brief How many runs were cancelled because an edit made them stale
    ///        - for tests.
    /// @return The count.
    [[nodiscard]] int cancelledCount() const noexcept { return cancelled_; }

private:
    AnalysisPanel* panel_;
    std::function<const sound_mind::core::Project*()> project_;
    QSettings* settings_;
    bool autoAnalyze_ = false;
    bool stale_ = false;
    bool reanalyzeRequested_ = false;
    int cancelled_ = 0;
    int pitchVoiceCount_ = 1;
    QTimer autoTimer_;
    /// @brief Declared last, so it's cancelled and waited for first.
    BackgroundJob job_{nullptr, std::chrono::milliseconds(50)};
};

}  // namespace sound_mind::studio
