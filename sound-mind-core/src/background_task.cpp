#include "sound_mind/core/background_task.h"

#include <utility>

namespace sound_mind::core {

BackgroundTask::BackgroundTask(WorkFunction work) : work_(std::move(work)) {}

BackgroundTask::~BackgroundTask() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void BackgroundTask::start() {
    running_.store(true, std::memory_order_relaxed);
    thread_ = std::thread([this]() {
        work_(token_);
        running_.store(false, std::memory_order_relaxed);
    });
}

void BackgroundTask::requestCancel() { token_.requestCancel(); }

bool BackgroundTask::isRunning() const noexcept { return running_.load(std::memory_order_relaxed); }

bool BackgroundTask::wasCancelled() const noexcept { return token_.cancellationRequested(); }

void BackgroundTask::waitForFinished() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

}  // namespace sound_mind::core
