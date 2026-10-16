#include "sound_mind/codec/parallel.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace sound_mind::codec {

namespace {

std::atomic<std::size_t> threadOverride{0};

}  // namespace

std::size_t maxWorkerThreads() noexcept {
    const std::size_t forced = threadOverride.load(std::memory_order_relaxed);
    if (forced > 0) {
        return forced;
    }
    return std::max<std::size_t>(1, std::thread::hardware_concurrency());
}

void setMaxWorkerThreads(std::size_t threads) noexcept { threadOverride.store(threads, std::memory_order_relaxed); }

void parallelFor(std::size_t count, std::size_t blockSize,
                 const std::function<void(std::size_t begin, std::size_t end)>& body) {
    if (count == 0) {
        return;
    }
    const std::size_t block = std::max<std::size_t>(1, blockSize);
    const std::size_t blocks = (count + block - 1) / block;
    const std::size_t threads = std::min(blocks, maxWorkerThreads());
    if (threads <= 1) {
        for (std::size_t begin = 0; begin < count; begin += block) {
            body(begin, std::min(count, begin + block));
        }
        return;
    }

    std::atomic<std::size_t> next{0};
    std::atomic<bool> failed{false};
    std::exception_ptr error;
    std::mutex errorMutex;
    const auto work = [&]() {
        for (std::size_t index = next.fetch_add(1); index < blocks && !failed.load(); index = next.fetch_add(1)) {
            try {
                const std::size_t begin = index * block;
                body(begin, std::min(count, begin + block));
            } catch (...) {
                const std::lock_guard lock(errorMutex);
                if (!error) {
                    error = std::current_exception();
                }
                failed.store(true);
            }
        }
    };
    std::vector<std::jthread> workers;
    workers.reserve(threads - 1);
    for (std::size_t i = 1; i < threads; ++i) {
        workers.emplace_back(work);
    }
    work();
    workers.clear();  // joins.
    if (error) {
        std::rethrow_exception(error);
    }
}

}  // namespace sound_mind::codec
