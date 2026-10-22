#include "sound_mind/studio/analysis_controller.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include <QSettings>

#include "sound_mind/core/pitch_detection.h"
#include "sound_mind/core/project_analysis.h"
#include "sound_mind/studio/analysis_panel.h"

namespace sound_mind::studio {

AnalysisController::AnalysisController(AnalysisPanel* panel,
                                       std::function<const sound_mind::core::Project*()> project,
                                       QSettings* settings, QObject* parent)
    : QObject(parent), panel_(panel), project_(std::move(project)), settings_(settings) {
    autoTimer_.setSingleShot(true);
    autoTimer_.setInterval(kAutoAnalyzeDelayMs);
    connect(&autoTimer_, &QTimer::timeout, this, [this]() {
        if ((!autoAnalyze_ && !reanalyzeRequested_) || project_() == nullptr) {
            return;
        }
        if (isAnalyzing()) {
            autoTimer_.start();  // a cancelled run is still winding down: try again shortly.
            return;
        }
        analyze();
    });
    connect(panel_, &AnalysisPanel::analyzeRequested, this, &AnalysisController::analyze);
    connect(panel_, &AnalysisPanel::autoAnalyzeToggled, this, &AnalysisController::setAutoAnalyze);
    connect(panel_, &AnalysisPanel::pitchVoiceCountChanged, this, [this](int voices) {
        pitchVoiceCount_ = voices;
        settings_->setValue(QStringLiteral("pitchVoiceCount"), voices);
        if (panel_->hasReport() || isAnalyzing()) {
            requestReanalysis();  // the report's pitch tracks no longer match.
        }
    });
    connect(panel_, &AnalysisPanel::pitchSpikeFilterChanged, this, [this](double milliseconds) {
        settings_->setValue(QStringLiteral("pitchSpikeFilterMs"), milliseconds);
    });

    setAutoAnalyze(settings_->value(QStringLiteral("analysisAutoUpdate"), false).toBool());
    panel_->setPitchSpikeFilterMs(settings_->value(QStringLiteral("pitchSpikeFilterMs"), 0.0).toDouble());
    pitchVoiceCount_ = std::clamp(settings_->value(QStringLiteral("pitchVoiceCount"), 1).toInt(), 1,
                                  sound_mind::core::kMaxPitchVoices);
    panel_->setPitchVoiceCount(pitchVoiceCount_);
}

AnalysisController::~AnalysisController() {
    job_.requestCancel();  // job_'s destructor waits and drops the report.
}

void AnalysisController::noteProjectChanged() {
    stale_ = true;
    panel_->setStale();
    if (!autoAnalyze_) {
        return;
    }
    if (isAnalyzing() && !job_.cancelRequested()) {
        job_.requestCancel();  // its result would already be out of date.
        ++cancelled_;
    }
    autoTimer_.start();  // restarts the wait on each change.
}

void AnalysisController::requestReanalysis() {
    if (isAnalyzing() && !job_.cancelRequested()) {
        job_.requestCancel();
        ++cancelled_;
    }
    reanalyzeRequested_ = true;
    autoTimer_.start();
}

void AnalysisController::setAutoAnalyze(bool enabled) {
    autoAnalyze_ = enabled;
    settings_->setValue(QStringLiteral("analysisAutoUpdate"), enabled);
    panel_->setAutoAnalyze(enabled);
    if (!enabled) {
        autoTimer_.stop();
        return;
    }
    if (project_() != nullptr && (!panel_->hasReport() || stale_)) {
        analyze();
    }
}

void AnalysisController::projectChanged() {
    job_.requestCancel();  // its result is for the old project: the completion drops it.
    panel_->clearReport();
    stale_ = false;
    autoTimer_.stop();
    if (autoAnalyze_) {
        autoTimer_.start();  // analyze the newly opened project once it's in place.
    }
}

void AnalysisController::analyze() {
    const sound_mind::core::Project* project = project_();
    if (project == nullptr || isAnalyzing()) {
        return;
    }
    stale_ = false;
    reanalyzeRequested_ = false;
    panel_->setAnalyzing(true);
    if (!autoAnalyze_) {
        panel_->show();  // an explicit request; Auto-update never pops the panel up by itself.
    }
    job_.start([this, projectCopy = *project,
                voices = pitchVoiceCount_](sound_mind::core::CancellationToken& token) -> BackgroundJob::Completion {
        std::optional<sound_mind::core::ProjectLoudnessReport> report;
        try {
            report = sound_mind::core::analyzeProjectLoudness(
                projectCopy, [&token]() { return token.cancellationRequested(); }, voices);
        } catch (const std::exception&) {
            report.reset();  // cancelled or failed - nothing to show.
        }
        auto shared = std::make_shared<std::optional<sound_mind::core::ProjectLoudnessReport>>(std::move(report));
        return [this, shared](bool cancelled) {
            panel_->setAnalyzing(false);
            if (cancelled || !shared->has_value()) {
                return;  // stale (an edit, or another project) or failed.
            }
            panel_->setReport(**shared);
            if (stale_) {
                panel_->setStale();  // a change landed after this run had already finished.
            }
        };
    });
}

}  // namespace sound_mind::studio
