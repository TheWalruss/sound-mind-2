#include "sound_mind/studio/import_helpers.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace sound_mind::studio {

std::string lowercasedExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    for (char& c : extension) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return extension;
}

bool isImageExtension(const std::string& lowercaseExtension) {
    return lowercaseExtension == ".png" || lowercaseExtension == ".jpg" || lowercaseExtension == ".jpeg" ||
           lowercaseExtension == ".bmp" || lowercaseExtension == ".tga" || lowercaseExtension == ".webp";
}

std::optional<sound_mind::codec::CompressedAudioFormat> audioFormatFromExtension(const std::filesystem::path& path) {
    const std::string extension = lowercasedExtension(path);
    if (extension == ".flac") {
        return sound_mind::codec::CompressedAudioFormat::Flac;
    }
    if (extension == ".ogg") {
        return sound_mind::codec::CompressedAudioFormat::Ogg;
    }
    if (extension == ".mp3") {
        return sound_mind::codec::CompressedAudioFormat::Mp3;
    }
    return std::nullopt;
}

std::string formatSnippetIndex(std::size_t index) {
    std::ostringstream stream;
    stream << std::setw(4) << std::setfill('0') << index;
    return stream.str();
}

QImage scaleImageForImport(const QImage& source, ImageScalePickerDialog::Mode mode, int canvasWidth,
                            int canvasHeight) {
    using Mode = ImageScalePickerDialog::Mode;
    switch (mode) {
        case Mode::RescaleToFitProject:
            return source.scaled(canvasWidth, canvasHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        case Mode::ScaleVerticalKeepHorizontal:
            return source.scaled(source.width(), canvasHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        case Mode::ScaleHorizontalKeepVertical:
            return source.scaled(canvasWidth, source.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        case Mode::ScaleVerticalProportional: {
            const int proportionalWidth =
                source.height() > 0
                    ? std::max(1, static_cast<int>(std::lround(static_cast<double>(source.width()) * canvasHeight /
                                                                source.height())))
                    : canvasWidth;
            return source.scaled(proportionalWidth, canvasHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        case Mode::KeepNativeResolution:
            return source;
    }
    return source;  // unreachable - every Mode value is handled above.
}

}  // namespace sound_mind::studio
