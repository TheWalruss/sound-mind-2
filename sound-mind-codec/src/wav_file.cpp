#include "sound_mind/codec/wav_file.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace sound_mind::codec {

namespace {

[[nodiscard]] std::uint16_t readUint16(const std::vector<char>& data, std::size_t offset) {
    return static_cast<std::uint16_t>(static_cast<std::uint8_t>(data[offset])) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(static_cast<std::uint8_t>(data[offset + 1])) << 8);
}

[[nodiscard]] std::uint32_t readUint32(const std::vector<char>& data, std::size_t offset) {
    return static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[offset])) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[offset + 1])) << 8) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[offset + 2])) << 16) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[offset + 3])) << 24);
}

[[nodiscard]] bool tagMatches(const std::vector<char>& data, std::size_t offset, const char* tag) {
    return offset + 4 <= data.size() && std::memcmp(data.data() + offset, tag, 4) == 0;
}

}  // namespace

AudioBuffer readWavFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::ios_base::failure("could not open WAV file for reading: " + path.string());
    }
    const std::vector<char> data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    if (data.size() < 12 || !tagMatches(data, 0, "RIFF") || !tagMatches(data, 8, "WAVE")) {
        throw std::invalid_argument("not a WAV file: " + path.string());
    }

    std::uint16_t audioFormat = 0;
    std::uint16_t numChannels = 0;
    std::uint32_t sampleRateHz = 0;
    std::uint16_t bitsPerSample = 0;
    std::size_t dataOffset = 0;
    std::size_t dataSize = 0;
    bool haveFmt = false;
    bool haveData = false;

    std::size_t offset = 12;
    while (offset + 8 <= data.size()) {
        const std::uint32_t chunkSize = readUint32(data, offset + 4);
        const std::size_t chunkDataOffset = offset + 8;

        if (tagMatches(data, offset, "fmt ")) {
            if (chunkDataOffset + 16 > data.size()) {
                throw std::invalid_argument("WAV file's fmt chunk is truncated: " + path.string());
            }
            audioFormat = readUint16(data, chunkDataOffset);
            numChannels = readUint16(data, chunkDataOffset + 2);
            sampleRateHz = readUint32(data, chunkDataOffset + 4);
            bitsPerSample = readUint16(data, chunkDataOffset + 14);
            haveFmt = true;
        } else if (tagMatches(data, offset, "data")) {
            dataOffset = chunkDataOffset;
            dataSize = chunkSize;
            haveData = true;
        }

        // Chunks are padded to an even number of bytes.
        offset = chunkDataOffset + chunkSize + (chunkSize % 2);
    }

    if (!haveFmt || !haveData) {
        throw std::invalid_argument("WAV file is missing a fmt or data chunk: " + path.string());
    }
    if (audioFormat != 1 && audioFormat != 3) {
        throw std::invalid_argument(
            "unsupported WAV audio format (only PCM and IEEE float are supported): " + path.string());
    }
    if (numChannels != 1 && numChannels != 2) {
        throw std::invalid_argument(
            "unsupported WAV channel count (only mono and stereo are supported): " + path.string());
    }
    if (audioFormat == 1 && bitsPerSample != 16) {
        throw std::invalid_argument("unsupported WAV PCM bit depth (only 16-bit is supported): " + path.string());
    }
    if (audioFormat == 3 && bitsPerSample != 32) {
        throw std::invalid_argument("unsupported WAV float bit depth (only 32-bit is supported): " + path.string());
    }
    if (dataOffset + dataSize > data.size()) {
        throw std::invalid_argument("WAV file's data chunk is truncated: " + path.string());
    }

    const std::size_t bytesPerSample = bitsPerSample / 8;
    const std::size_t bytesPerFrame = bytesPerSample * numChannels;
    const std::size_t frameCount = bytesPerFrame == 0 ? 0 : dataSize / bytesPerFrame;

    AudioBuffer audio;
    audio.sampleRateHz = sampleRateHz;
    audio.left.resize(frameCount);
    audio.right.resize(frameCount);

    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        const std::size_t frameOffset = dataOffset + frame * bytesPerFrame;

        auto readSample = [&](std::size_t channel) -> float {
            const std::size_t sampleOffset = frameOffset + channel * bytesPerSample;
            if (audioFormat == 1) {
                const auto raw = static_cast<std::int16_t>(readUint16(data, sampleOffset));
                return static_cast<float>(raw) / 32768.0f;
            }
            float value = 0.0f;
            std::memcpy(&value, data.data() + sampleOffset, sizeof(float));
            return value;
        };

        const float left = readSample(0);
        audio.left[frame] = left;
        audio.right[frame] = (numChannels == 2) ? readSample(1) : left;
    }

    return audio;
}

}  // namespace sound_mind::codec
