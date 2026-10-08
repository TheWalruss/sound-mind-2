#include "sound_mind/core/resource_file.h"

#include <fstream>
#include <stdexcept>

namespace sound_mind::core {

namespace {

/// @brief Every portable resource file's own envelope format version -
/// bumped only if this envelope's own shape changes incompatibly, same
/// convention as `StreamImage`/`PoolImage`'s own format versions.
constexpr int kFormatVersion = 1;

/// @brief Writes `entry` into a standalone portable resource file at
/// `path`, wrapped in the shared JSON envelope every `exportXxx()` function
/// in this file uses - see `resource_file.h`'s own docs.
///
/// @tparam T One of the `Named...` resource entry types - must have a
///        public `id` field and an ADL-visible `to_json()`.
template <typename T>
void writeResourceFile(const T& entry, PortableResourceType type, const std::filesystem::path& path) {
    T withoutId = entry;
    withoutId.id = 0;

    nlohmann::json envelope;
    envelope["soundMindResourceFile"] = true;
    envelope["resourceType"] = type;
    envelope["formatVersion"] = kFormatVersion;
    envelope["resource"] = withoutId;

    std::ofstream file(path);
    if (!file) {
        throw std::ios_base::failure("Could not open resource file for writing: " + path.string());
    }
    file << envelope.dump(2);
}

/// @brief Reads a standalone portable resource file back from `path`, the
/// inverse of `writeResourceFile()` - see `resource_file.h`'s own docs.
///
/// @tparam T One of the `Named...` resource entry types - must have a
///        public `id` field and an ADL-visible `from_json()`.
template <typename T>
T readResourceFile(PortableResourceType expectedType, const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::ios_base::failure("Could not open resource file for reading: " + path.string());
    }
    nlohmann::json envelope;
    file >> envelope;

    if (!envelope.value("soundMindResourceFile", false)) {
        throw std::invalid_argument("Not a Sound Mind portable resource file: " + path.string());
    }
    const auto actualType = envelope.at("resourceType").get<PortableResourceType>();
    if (actualType != expectedType) {
        throw std::invalid_argument("Wrong resourceType in portable resource file: " + path.string());
    }

    T entry = envelope.at("resource").get<T>();
    entry.id = 0;
    return entry;
}

}  // namespace

std::string_view portableResourceFileExtension(PortableResourceType type) noexcept {
    switch (type) {
        case PortableResourceType::MindWave:
            return ".smwave";
        case PortableResourceType::ToolPreset:
            return ".sminst";
        case PortableResourceType::MindShot:
            return ".smshot";
        case PortableResourceType::ResonanceProfile:
            return ".smresonance";
        case PortableResourceType::ConvolutionKernel:
            return ".smfilter";
    }
    return "";
}

void exportMindWave(const NamedMindWave& entry, const std::filesystem::path& path) {
    writeResourceFile(entry, PortableResourceType::MindWave, path);
}

NamedMindWave importMindWave(const std::filesystem::path& path) {
    return readResourceFile<NamedMindWave>(PortableResourceType::MindWave, path);
}

void exportToolPreset(const NamedToolPreset& entry, const std::filesystem::path& path) {
    if (!entry.config) {
        throw std::invalid_argument("Cannot export a NamedToolPreset with a null config");
    }
    writeResourceFile(entry, PortableResourceType::ToolPreset, path);
}

NamedToolPreset importToolPreset(const std::filesystem::path& path) {
    return readResourceFile<NamedToolPreset>(PortableResourceType::ToolPreset, path);
}

void exportMindShot(const NamedMindShot& entry, const std::filesystem::path& path) {
    writeResourceFile(entry, PortableResourceType::MindShot, path);
}

NamedMindShot importMindShot(const std::filesystem::path& path) {
    return readResourceFile<NamedMindShot>(PortableResourceType::MindShot, path);
}

void exportResonanceProfile(const NamedResonanceProfile& entry, const std::filesystem::path& path) {
    writeResourceFile(entry, PortableResourceType::ResonanceProfile, path);
}

NamedResonanceProfile importResonanceProfile(const std::filesystem::path& path) {
    return readResourceFile<NamedResonanceProfile>(PortableResourceType::ResonanceProfile, path);
}

void exportConvolutionKernel(const NamedConvolutionKernel& entry, const std::filesystem::path& path) {
    writeResourceFile(entry, PortableResourceType::ConvolutionKernel, path);
}

NamedConvolutionKernel importConvolutionKernel(const std::filesystem::path& path) {
    return readResourceFile<NamedConvolutionKernel>(PortableResourceType::ConvolutionKernel, path);
}

}  // namespace sound_mind::core
