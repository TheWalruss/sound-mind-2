#pragma once

#include <stdexcept>

namespace sound_mind::codec {

/**
 * @brief Thrown by exportVideo()/exportCompressedAudio() when their own
 *        `shouldCancel` callback starts returning `true` mid-encode -
 *        `docs/sound-mind-roadmap.md`'s "real, non-blocking cancel
 *        affordance for long operations" milestone (finding #12).
 *
 * A distinct type, not a plain `std::runtime_error`, specifically so a
 * caller can tell "cancelled on request" apart from a genuine encode
 * failure - e.g. to skip showing an error message for the former while
 * still surfacing the latter. Neither export function cleans up whatever
 * partial file it had already written by the point of cancellation -
 * matching every other error path in each, which also leaves the caller
 * responsible for deciding what to do with a partially-written output on
 * failure.
 *
 * Shared by both export functions (rather than one owning it and the other
 * including that header just for this type) since a caller driving both
 * kinds of export through the same cancellable-background-task machinery
 * (`sound_mind::core::BackgroundTask`) wants to catch one exception type
 * regardless of which export it started.
 */
class ExportCancelled : public std::runtime_error {
public:
    ExportCancelled() : std::runtime_error("export cancelled") {}
};

}  // namespace sound_mind::codec
