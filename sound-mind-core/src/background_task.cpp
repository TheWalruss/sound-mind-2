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
        // release, not relaxed: a caller observing isRunning() == false
        // (an acquire load - see isRunning()'s own docs) must also see
        // whatever work_ itself wrote (e.g. a result stored in a plain,
        // non-atomic member the caller owns) before returning - the
        // standard release/acquire flag idiom, not just "eventually
        // visible."
        running_.store(false, std::memory_order_release);
    });
}

void BackgroundTask::requestCancel() { token_.requestCancel(); }

bool BackgroundTask::isRunning() const noexcept { return running_.load(std::memory_order_acquire); }

bool BackgroundTask::wasCancelled() const noexcept { return token_.cancellationRequested(); }

void BackgroundTask::waitForFinished() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

}  // namespace sound_mind::core
