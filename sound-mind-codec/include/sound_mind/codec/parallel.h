#pragma once

#include <cstddef>
#include <functional>

namespace sound_mind::codec {

/**
 * @brief Runs `body(begin, end)` over `[0, count)` in blocks of `blockSize`,
 *        spread across worker threads (`v0.1.41.2`) - the CPU parallelism
 *        under the codec and the analysis.
 *
 * Blocks are fixed by `count` and `blockSize` alone, never by how many
 * threads run them, so a caller that keeps per-block state (decode()'s
 * overlap-add, say) and combines it in block order gets the same result on
 * every machine. Blocks are handed out dynamically; the calling thread
 * works too. Runs everything inline when there's only one block or one
 * thread.
 *
 * @param count The number of items.
 * @param blockSize Items per block; `0` is treated as 1.
 * @param body Called once per block with its `[begin, end)`; must be safe
 *        to run concurrently with itself on disjoint blocks.
 * @note Not for the real-time audio thread: it creates threads. An exception
 *       thrown by `body` is rethrown on the calling thread after every
 *       block has finished or been skipped.
 */
void parallelFor(std::size_t count, std::size_t blockSize,
                 const std::function<void(std::size_t begin, std::size_t end)>& body);

/// @brief How many threads parallelFor() uses at most, including the
///        caller: `std::thread::hardware_concurrency()` unless overridden.
/// @return The limit, at least 1.
[[nodiscard]] std::size_t maxWorkerThreads() noexcept;

/**
 * @brief Overrides maxWorkerThreads() - `1` makes parallelFor() serial,
 *        `0` restores the default. For tests (comparing one thread with
 *        many) and benchmarks.
 * @param threads The limit.
 * @note Process-wide; not meant to change while parallelFor() is running.
 */
void setMaxWorkerThreads(std::size_t threads) noexcept;

}  // namespace sound_mind::codec
