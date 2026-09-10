#include "sound_mind/studio/qt_image_conversion.h"

#include <cstddef>
#include <cstring>

namespace sound_mind::studio {

sound_mind::codec::RgbImage toRgbImage(const QImage& source) {
    const QImage rgb888 = source.convertToFormat(QImage::Format_RGB888);

    sound_mind::codec::RgbImage image;
    image.width = static_cast<std::uint32_t>(rgb888.width());
    image.height = static_cast<std::uint32_t>(rgb888.height());
    image.pixels.resize(image.pixelCount() * 3);

    for (int y = 0; y < rgb888.height(); ++y) {
        const uchar* line = rgb888.constScanLine(y);
        std::memcpy(image.pixels.data() + static_cast<std::size_t>(y) * image.width * 3, line,
                    static_cast<std::size_t>(image.width) * 3);
    }
    return image;
}

QImage toQImageView(const sound_mind::codec::RgbImage& image) {
    return QImage(image.pixels.data(), static_cast<int>(image.width), static_cast<int>(image.height),
                  static_cast<int>(image.width) * 3, QImage::Format_RGB888);
}

}  // namespace sound_mind::studio
