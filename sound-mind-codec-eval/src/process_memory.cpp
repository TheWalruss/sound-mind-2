// Defined before any include, matching sound-mind-gpu/src/compute_device.cpp's
// own established reasoning: NOMINMAX keeps Windows.h's min/max macros from
// shadowing std::min/std::max used elsewhere in this file's own header.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "sound_mind/codec_eval/process_memory.h"

#include <Windows.h>

#include <psapi.h>

namespace sound_mind::codec_eval {

std::uint64_t peakWorkingSetBytes() {
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return 0;
    }
    return static_cast<std::uint64_t>(counters.PeakWorkingSetSize);
}

}  // namespace sound_mind::codec_eval
