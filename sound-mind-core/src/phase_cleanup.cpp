#include "sound_mind/core/phase_cleanup.h"

#include <cstddef>

namespace sound_mind::core {

namespace {

/// @brief The same `-96`dB silence floor `silenceGradient()`'s own docs
/// establish - duplicated here rather than shared, the same "a one-line
/// constant isn't worth a shared header over" precedent
/// `docs/sound-mind-architecture.md`'s Decision #59 already gives for
/// this exact value.
constexpr float kSilenceFloorDb = -96.0f;

}  // namespace

void applyPhaseCleanup(sound_mind::codec::StreamImage& content) {
    const std::size_t cellCount = content.sharedPhaseRadians.size();
    for (std::size_t i = 0; i < cellCount; ++i) {
        if (content.leftMagnitudeDb[i] <= kSilenceFloorDb && content.rightMagnitudeDb[i] <= kSilenceFloorDb) {
            content.sharedPhaseRadians[i] = 0.0f;
        }
    }
}

}  // namespace sound_mind::core
