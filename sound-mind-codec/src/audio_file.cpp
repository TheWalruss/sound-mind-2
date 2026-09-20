#include "sound_mind/codec/audio_file.h"

#include <cctype>
#include <string>

#include "ffmpeg_audio_decoder.h"
#include "sound_mind/codec/wav_file.h"

namespace sound_mind::codec {

namespace {

/// @brief Whether `path`'s own extension is `.wav`, case-insensitively.
bool hasWavExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    for (char& c : extension) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return extension == ".wav";
}

}  // namespace

AudioBuffer readAudioFile(const std::filesystem::path& path) {
    if (hasWavExtension(path)) {
        return readWavFile(path);
    }
    return detail::decodeAudioFileViaFFmpeg(path);
}

}  // namespace sound_mind::codec
