#pragma once

#include <string_view>

namespace sound_mind::codec {

// Originally a toolchain-validation placeholder, kept on since it's still a
// convenient one-line way to confirm which build of the library is linked
// in. Real codec logic lives alongside it now - see stream_codec.h and
// sound-mind-architecture.md for the Pool/Stream codec design.
[[nodiscard]] std::string_view version_string() noexcept;

}  // namespace sound_mind::codec
