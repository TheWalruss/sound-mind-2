#pragma once

#include <string_view>

namespace sound_mind::codec {

// Toolchain-validation placeholder: proves the codec library builds, links,
// and is unit-testable end to end. No real codec logic lives here yet -
// see sound-mind-architecture.md for the intended Pool/Stream codec design.
[[nodiscard]] std::string_view version_string() noexcept;

}  // namespace sound_mind::codec
