#include "sound_mind/studio/background_job.h"

#include <exception>
#include <utility>

#include <QAbstractButton>

namespace sound_mind::studio {

BackgroundJob::BackgroundJob(QObject* parent, std::chrono::milliseconds pollInterval) : QObject(parent) {
    timer_.setInterval(pollInterval);
    connect(&timer_, &QTimer::timeout, this, &BackgroundJob::poll);
}

BackgroundJob::~BackgroundJob() {
    if (task_) {
        task_->requestCancel();
        task_->waitForFinished();
    }
}

bool BackgroundJob::start(Work work, ErrorHandler onError) {
    if (task_) {
        return false;
    }
    completion_ = nullptr;
    errorMessage_.clear();
    failed_ = false;
    cancelRequested_ = false;
    onError_ = std::move(onError);
    task_ = std::make_unique<sound_mind::core::BackgroundTask>(
        [this, work = std::move(work)](sound_mind::core::CancellationToken& token) {
            try {
                completion_ = work(token);
            } catch (const std::exception& e) {
                errorMessage_ = QString::fromStdString(e.what());
                failed_ = true;
            } catch (...) {
                errorMessage_ = tr("An unexpected error occurred.");
                failed_ = true;
            }
        });
    task_->start();
    if (cancelButton_) {
        cancelButton_->show();
    }
    timer_.start();
    return true;
}

void BackgroundJob::requestCancel() {
    if (task_) {
        cancelRequested_ = true;
        task_->requestCancel();
    }
}

bool BackgroundJob::cancelRequested() const noexcept { return task_ != nullptr && cancelRequested_; }

void BackgroundJob::finishNow() {
    if (!task_) {
        return;
    }
    task_->waitForFinished();
    deliver();
}

void BackgroundJob::setCancelButton(QAbstractButton* button) {
    if (cancelButton_) {
        disconnect(cancelButton_, nullptr, this, nullptr);
    }
    cancelButton_ = button;
    if (cancelButton_) {
        cancelButton_->setVisible(task_ != nullptr);
        connect(cancelButton_, &QAbstractButton::clicked, this, &BackgroundJob::requestCancel);
    }
}

void BackgroundJob::poll() {
    if (!task_) {
        return;
    }
    if (task_->isRunning()) {
        emit stillRunning();
        return;
    }
    deliver();
}

void BackgroundJob::deliver() {
    timer_.stop();
    if (cancelButton_) {
        cancelButton_->hide();
    }
    const bool cancelled = cancelRequested_ || task_->wasCancelled();
    cancelRequested_ = false;
    // Reset before running the completion, which may start this job again.
    task_.reset();
    Completion completion = std::move(completion_);
    completion_ = nullptr;
    ErrorHandler onError = std::move(onError_);
    onError_ = nullptr;
    if (failed_) {
        failed_ = false;
        if (onError) {
            onError(errorMessage_);
        }
    } else if (completion) {
        completion(cancelled);
    }
    emit finished(cancelled);
}

}  // namespace sound_mind::studio
