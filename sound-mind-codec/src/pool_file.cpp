#include "sound_mind/codec/pool_file.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ios>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <tiffio.h>

namespace sound_mind::codec {

namespace {

constexpr float kMinDb = -96.0f;
constexpr float kMaxDb = 0.0f;

[[nodiscard]] std::uint16_t dbToUint16(float db) noexcept {
    const float clamped = std::clamp(db, kMinDb, kMaxDb);
    const float normalized = (clamped - kMinDb) / (kMaxDb - kMinDb);
    return static_cast<std::uint16_t>(std::lround(normalized * 65535.0f));
}

[[nodiscard]] float uint16ToDb(std::uint16_t value) noexcept {
    const float normalized = static_cast<float>(value) / 65535.0f;
    return kMinDb + normalized * (kMaxDb - kMinDb);
}

[[nodiscard]] std::uint16_t phaseToUint16(float radians) noexcept {
    constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
    float wrapped = std::fmod(radians + std::numbers::pi_v<float>, kTwoPi);
    if (wrapped < 0.0f) {
        wrapped += kTwoPi;
    }
    return static_cast<std::uint16_t>(std::lround((wrapped / kTwoPi) * 65535.0f));
}

[[nodiscard]] float uint16ToPhase(std::uint16_t value) noexcept {
    const float normalized = static_cast<float>(value) / 65535.0f;
    return normalized * 2.0f * std::numbers::pi_v<float> - std::numbers::pi_v<float>;
}

/// @brief Writes one page's worth of dB or phase values as a 16-bit
/// grayscale TIFF directory. `toPixel` converts one float value to its
/// uint16 pixel encoding.
template <typename ToPixel>
void writePage(TIFF* tiff, const std::vector<float>& values, std::uint32_t width, std::uint32_t height,
               std::uint32_t pageIndex, const std::string* description, ToPixel toPixel) {
    TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 16);
    TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, height == 0 ? 1 : height);
    TIFFSetField(tiff, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);
    TIFFSetField(tiff, TIFFTAG_PAGENUMBER, static_cast<std::uint16_t>(pageIndex), static_cast<std::uint16_t>(4));
    if (description != nullptr) {
        TIFFSetField(tiff, TIFFTAG_IMAGEDESCRIPTION, description->c_str());
    }

    std::vector<std::uint16_t> row(width);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            // Storage is row-major [bin][frame] with bin 0 = lowest
            // frequency; the TIFF's row 0 = highest frequency, matching
            // color_mapping.cpp's toRgbImage() convention (bass at the
            // bottom, treble at the top, as a spectrogram is
            // conventionally displayed).
            const std::uint32_t bin = height - 1 - y;
            const std::size_t cell = static_cast<std::size_t>(bin) * width + x;
            row[x] = toPixel(values[cell]);
        }
        if (TIFFWriteScanline(tiff, row.data(), y, 0) < 0) {
            throw std::ios_base::failure("failed writing a Pool file scanline");
        }
    }

    if (!TIFFWriteDirectory(tiff)) {
        throw std::ios_base::failure("failed writing a Pool file directory");
    }
}

/// @brief Reads one page's worth of pixel values back into a `[bin][frame]`
/// float plane, using `fromPixel` to convert each uint16 pixel back.
template <typename FromPixel>
void readPage(TIFF* tiff, std::vector<float>& values, std::uint32_t width, std::uint32_t height, FromPixel fromPixel) {
    values.resize(std::size_t{width} * height);
    std::vector<std::uint16_t> row(width);
    for (std::uint32_t y = 0; y < height; ++y) {
        if (TIFFReadScanline(tiff, row.data(), y, 0) < 0) {
            throw std::ios_base::failure("failed reading a Pool file scanline");
        }
        const std::uint32_t bin = height - 1 - y;
        for (std::uint32_t x = 0; x < width; ++x) {
            values[static_cast<std::size_t>(bin) * width + x] = fromPixel(row[x]);
        }
    }
}

/// @brief Disables libtiff's default error/warning handlers, which print
/// straight to stderr - noisy, misleading signal (it looks like an
/// unhandled failure) for something we already convey properly via a
/// thrown exception's message. Idempotent; cheap enough to call every time
/// rather than needing separate one-time initialization.
void silenceLibtiffDefaultHandlers() {
    TIFFSetErrorHandler(nullptr);
    TIFFSetWarningHandler(nullptr);
}

}  // namespace

void writePoolFile(const std::filesystem::path& path, const PoolImage& image) {
    silenceLibtiffDefaultHandlers();
    TIFF* tiff = TIFFOpenW(path.wstring().c_str(), "w");
    if (tiff == nullptr) {
        throw std::ios_base::failure("could not open Pool file for writing: " + path.string());
    }

    std::ostringstream metadata;
    metadata << "SoundMindPool:Version=1\n"
             << "SoundMindPool:SampleRate=" << image.config.sampleRateHz << '\n'
             << "SoundMindPool:HopLength=" << image.config.hopLength << '\n'
             << "SoundMindPool:BinCount=" << image.config.binCount << '\n'
             << "SoundMindPool:MinFrequencyHz=" << image.config.minFrequencyHz << '\n'
             << "SoundMindPool:MaxFrequencyHz=" << image.config.maxFrequencyHz << '\n'
             << "SoundMindPool:FrameCount=" << image.frameCount << '\n'
             << "SoundMindPool:SampleCount=" << image.sampleCount << '\n';
    const std::string metadataString = metadata.str();

    const std::uint32_t width = image.frameCount;
    const std::uint32_t height = image.config.binCount;

    try {
        writePage(tiff, image.leftMagnitudeDb, width, height, 0, &metadataString, dbToUint16);
        writePage(tiff, image.rightMagnitudeDb, width, height, 1, nullptr, dbToUint16);
        writePage(tiff, image.leftPhaseRadians, width, height, 2, nullptr, phaseToUint16);
        writePage(tiff, image.rightPhaseRadians, width, height, 3, nullptr, phaseToUint16);
    } catch (...) {
        TIFFClose(tiff);
        throw;
    }

    TIFFClose(tiff);
}

PoolImage readPoolFile(const std::filesystem::path& path) {
    silenceLibtiffDefaultHandlers();
    TIFF* tiff = TIFFOpenW(path.wstring().c_str(), "r");
    if (tiff == nullptr) {
        throw std::ios_base::failure("could not open Pool file for reading (not a valid TIFF?): " + path.string());
    }

    char* description = nullptr;
    if (!TIFFGetField(tiff, TIFFTAG_IMAGEDESCRIPTION, &description) || description == nullptr) {
        TIFFClose(tiff);
        throw std::invalid_argument("not a Pool file (no Sound Mind metadata): " + path.string());
    }

    PoolImage image;
    bool haveSampleRate = false;
    bool haveHopLength = false;
    bool haveBinCount = false;

    std::istringstream lines(description);
    std::string line;
    while (std::getline(lines, line)) {
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, separator);
        const std::string value = line.substr(separator + 1);

        if (key == "SoundMindPool:SampleRate") {
            image.config.sampleRateHz = static_cast<std::uint32_t>(std::stoul(value));
            haveSampleRate = true;
        } else if (key == "SoundMindPool:HopLength") {
            image.config.hopLength = static_cast<std::uint32_t>(std::stoul(value));
            haveHopLength = true;
        } else if (key == "SoundMindPool:BinCount") {
            image.config.binCount = static_cast<std::uint32_t>(std::stoul(value));
            haveBinCount = true;
        } else if (key == "SoundMindPool:MinFrequencyHz") {
            image.config.minFrequencyHz = std::stof(value);
        } else if (key == "SoundMindPool:MaxFrequencyHz") {
            image.config.maxFrequencyHz = std::stof(value);
        } else if (key == "SoundMindPool:FrameCount") {
            image.frameCount = static_cast<std::uint32_t>(std::stoul(value));
        } else if (key == "SoundMindPool:SampleCount") {
            image.sampleCount = std::stoull(value);
        }
        // Unknown keys are silently ignored, for forward compatibility -
        // matching the legacy TIFF spec's own parsing rules.
    }

    if (!haveSampleRate || !haveHopLength || !haveBinCount) {
        TIFFClose(tiff);
        throw std::invalid_argument("Pool file is missing required metadata: " + path.string());
    }

    const std::uint32_t width = image.frameCount;
    const std::uint32_t height = image.config.binCount;

    try {
        readPage(tiff, image.leftMagnitudeDb, width, height, uint16ToDb);
        if (!TIFFReadDirectory(tiff)) {
            throw std::invalid_argument("Pool file is missing a page: " + path.string());
        }
        readPage(tiff, image.rightMagnitudeDb, width, height, uint16ToDb);
        if (!TIFFReadDirectory(tiff)) {
            throw std::invalid_argument("Pool file is missing a page: " + path.string());
        }
        readPage(tiff, image.leftPhaseRadians, width, height, uint16ToPhase);
        if (!TIFFReadDirectory(tiff)) {
            throw std::invalid_argument("Pool file is missing a page: " + path.string());
        }
        readPage(tiff, image.rightPhaseRadians, width, height, uint16ToPhase);
    } catch (...) {
        TIFFClose(tiff);
        throw;
    }

    TIFFClose(tiff);
    return image;
}

}  // namespace sound_mind::codec
