#include "sound_mind/codec/stream_file.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace sound_mind::codec {

namespace {

constexpr std::array<char, 4> kMagic = {'S', 'M', 'S', 'T'};
constexpr std::uint32_t kFormatVersion = 1;

template <typename T>
void writeRaw(std::ofstream& stream, const T& value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
void writeRawArray(std::ofstream& stream, const std::vector<T>& values) {
    if (!values.empty()) {
        stream.write(reinterpret_cast<const char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(T)));
    }
}

template <typename T>
void readRaw(std::ifstream& stream, T& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
}

template <typename T>
void readRawArray(std::ifstream& stream, std::vector<T>& values, std::size_t count) {
    values.resize(count);
    if (count > 0) {
        stream.read(reinterpret_cast<char*>(values.data()), static_cast<std::streamsize>(count * sizeof(T)));
    }
}

}  // namespace

void writeStreamFile(const std::filesystem::path& path, const StreamImage& image) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw std::ios_base::failure("could not open Stream file for writing: " + path.string());
    }
    stream.exceptions(std::ios::badbit | std::ios::failbit);

    stream.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
    writeRaw(stream, kFormatVersion);
    writeRaw(stream, image.config.sampleRateHz);
    writeRaw(stream, image.config.hopLength);
    writeRaw(stream, image.config.binCount);
    writeRaw(stream, image.config.minFrequencyHz);
    writeRaw(stream, image.config.maxFrequencyHz);
    writeRaw(stream, image.frameCount);
    writeRaw(stream, image.sampleCount);

    writeRawArray(stream, image.leftMagnitudeDb);
    writeRawArray(stream, image.rightMagnitudeDb);
    writeRawArray(stream, image.sharedPhaseRadians);
}

StreamImage readStreamFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::ios_base::failure("could not open Stream file for reading: " + path.string());
    }

    std::array<char, 4> magic{};
    stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!stream || magic != kMagic) {
        throw std::invalid_argument("not a Stream file (bad magic bytes): " + path.string());
    }

    std::uint32_t formatVersion = 0;
    readRaw(stream, formatVersion);
    if (!stream || formatVersion != kFormatVersion) {
        throw std::invalid_argument("unsupported Stream file format version: " + path.string());
    }

    stream.exceptions(std::ios::badbit | std::ios::failbit);

    StreamImage image;
    readRaw(stream, image.config.sampleRateHz);
    readRaw(stream, image.config.hopLength);
    readRaw(stream, image.config.binCount);
    readRaw(stream, image.config.minFrequencyHz);
    readRaw(stream, image.config.maxFrequencyHz);
    readRaw(stream, image.frameCount);
    readRaw(stream, image.sampleCount);

    const std::size_t planeSize = static_cast<std::size_t>(image.config.binCount) * image.frameCount;
    readRawArray(stream, image.leftMagnitudeDb, planeSize);
    readRawArray(stream, image.rightMagnitudeDb, planeSize);
    readRawArray(stream, image.sharedPhaseRadians, planeSize);

    return image;
}

}  // namespace sound_mind::codec
