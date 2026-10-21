#include "sound_mind/core/project_sources.h"

#include <array>
#include <atomic>
#include <cstring>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <vector>

#include "sound_mind/codec/audio_export.h"
#include "sound_mind/codec/stream_file.h"

namespace sound_mind::core {

namespace {

/// A plain SHA-256 (FIPS 180-4), fed incrementally.
class Sha256 {
public:
    void update(const unsigned char* data, std::size_t size) {
        totalBytes_ += size;
        while (size > 0) {
            const std::size_t take = std::min(size, buffer_.size() - buffered_);
            std::memcpy(buffer_.data() + buffered_, data, take);
            buffered_ += take;
            data += take;
            size -= take;
            if (buffered_ == buffer_.size()) {
                block(buffer_.data());
                buffered_ = 0;
            }
        }
    }

    std::string hex() {
        const std::uint64_t bits = totalBytes_ * 8;
        const unsigned char one = 0x80;
        update(&one, 1);
        const unsigned char zero = 0;
        while (buffered_ != 56) {
            update(&zero, 1);
        }
        unsigned char length[8];
        for (int i = 0; i < 8; ++i) {
            length[i] = static_cast<unsigned char>(bits >> (56 - 8 * i));
        }
        update(length, 8);
        static constexpr char kDigits[] = "0123456789abcdef";
        std::string out;
        out.reserve(64);
        for (const std::uint32_t word : state_) {
            for (int shift = 28; shift >= 0; shift -= 4) {
                out.push_back(kDigits[(word >> shift) & 0xF]);
            }
        }
        return out;
    }

private:
    static std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void block(const unsigned char* p) {
        static constexpr std::array<std::uint32_t, 64> k = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        std::array<std::uint32_t, 64> w{};
        for (int i = 0; i < 16; ++i) {
            w[i] = (std::uint32_t{p[4 * i]} << 24) | (std::uint32_t{p[4 * i + 1]} << 16) |
                   (std::uint32_t{p[4 * i + 2]} << 8) | std::uint32_t{p[4 * i + 3]};
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3], e = state_[4], f = state_[5],
                      g = state_[6], h = state_[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t t1 = h + s1 + ch + k[static_cast<std::size_t>(i)] + w[static_cast<std::size_t>(i)];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + maj;
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<unsigned char, 64> buffer_{};
    std::size_t buffered_ = 0;
    std::uint64_t totalBytes_ = 0;
};

/// UTF-8 text of a path's file name.
std::string utf8Name(const std::filesystem::path& path) {
    const std::u8string name = path.filename().u8string();
    return std::string(name.begin(), name.end());
}

std::filesystem::path fromUtf8(const std::string& name) {
    return std::filesystem::path(std::u8string(name.begin(), name.end()));
}

/// Moves a freshly written temporary file into place under its hash name.
ProjectSource adoptTemporary(const std::filesystem::path& sourcesFolder, const std::filesystem::path& temporary,
                             const std::string& extension, std::string kind, std::string name) {
    ProjectSource source;
    source.kind = std::move(kind);
    source.originalName = std::move(name);
    source.sha256 = sha256OfFile(temporary);
    source.fileName = source.sha256.substr(0, 16) + extension;
    const std::filesystem::path target = sourcesFolder / fromUtf8(source.fileName);
    if (std::filesystem::exists(target)) {
        std::filesystem::remove(temporary);
    } else {
        std::filesystem::rename(temporary, target);
    }
    return source;
}

std::filesystem::path temporaryIn(const std::filesystem::path& folder, const std::string& extension) {
    static std::atomic<std::uint64_t> counter{0};
    return folder / (".incoming-" + std::to_string(counter.fetch_add(1)) + extension);
}

}  // namespace

void to_json(nlohmann::json& json, const ProjectSource& source) {
    json = nlohmann::json{{"id", source.id},
                          {"kind", source.kind},
                          {"originalName", source.originalName},
                          {"sha256", source.sha256},
                          {"fileName", source.fileName}};
}

void from_json(const nlohmann::json& json, ProjectSource& source) {
    json.at("id").get_to(source.id);
    json.at("kind").get_to(source.kind);
    source.originalName = json.value("originalName", std::string());
    json.at("sha256").get_to(source.sha256);
    json.at("fileName").get_to(source.fileName);
}

std::string sha256Hex(const void* data, std::size_t size) {
    Sha256 hash;
    hash.update(static_cast<const unsigned char*>(data), size);
    return hash.hex();
}

std::string sha256OfFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::ios_base::failure("Could not read " + path.string());
    }
    Sha256 hash;
    std::vector<char> chunk(1 << 20);
    while (file) {
        file.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        const auto got = static_cast<std::size_t>(file.gcount());
        if (got > 0) {
            hash.update(reinterpret_cast<const unsigned char*>(chunk.data()), got);
        }
    }
    return hash.hex();
}

ProjectSource storeSourceFile(const std::filesystem::path& sourcesFolder, const std::filesystem::path& file,
                              std::string kind) {
    std::filesystem::create_directories(sourcesFolder);
    ProjectSource source;
    source.kind = std::move(kind);
    source.originalName = utf8Name(file);
    source.sha256 = sha256OfFile(file);
    const std::string extension = utf8Name(file.extension());
    source.fileName = source.sha256.substr(0, 16) + extension;
    const std::filesystem::path target = sourcesFolder / fromUtf8(source.fileName);
    if (!std::filesystem::exists(target)) {
        std::filesystem::copy_file(file, target);
    }
    return source;
}

ProjectSource storeSourceAudio(const std::filesystem::path& sourcesFolder, const sound_mind::codec::AudioBuffer& audio,
                               std::string kind, std::string name) {
    std::filesystem::create_directories(sourcesFolder);
    const std::filesystem::path temporary = temporaryIn(sourcesFolder, ".flac");
    sound_mind::codec::exportCompressedAudio(temporary, audio, sound_mind::codec::CompressedAudioFormat::Flac);
    return adoptTemporary(sourcesFolder, temporary, ".flac", std::move(kind), std::move(name));
}

ProjectSource storeSourceStreamImage(const std::filesystem::path& sourcesFolder,
                                     const sound_mind::codec::StreamImage& image, std::string name) {
    std::filesystem::create_directories(sourcesFolder);
    const std::filesystem::path temporary = temporaryIn(sourcesFolder, ".smstream");
    sound_mind::codec::writeStreamFile(temporary, image);
    return adoptTemporary(sourcesFolder, temporary, ".smstream", "layerImage", std::move(name));
}

}  // namespace sound_mind::core
