#include "sound_mind/studio/color_conversion.h"

#include <algorithm>
#include <cmath>

namespace sound_mind::studio {

namespace {
constexpr float kDisplayMinDb = -96.0f;
constexpr float kDisplayMaxDb = 0.0f;
}  // namespace

int dbToDisplayByte(float db) noexcept {
    const float clamped = std::clamp(db, kDisplayMinDb, kDisplayMaxDb);
    const float normalized = (clamped - kDisplayMinDb) / (kDisplayMaxDb - kDisplayMinDb);
    return static_cast<int>(std::lround(normalized * 255.0f));
}

float displayByteToDb(int value) noexcept {
    const float normalized = static_cast<float>(std::clamp(value, 0, 255)) / 255.0f;
    return kDisplayMinDb + normalized * (kDisplayMaxDb - kDisplayMinDb);
}

}  // namespace sound_mind::studio
