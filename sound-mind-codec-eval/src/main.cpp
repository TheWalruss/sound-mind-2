// sound-mind-codec-eval: a developer-only tool that encodes/decodes real
// audio files through both the Stream and Pool codecs, across a sweep of
// hop lengths (timesteps) and bin counts, and reports fidelity (how close
// the decoded result is to the original), timing, and approximate memory
// cost for each combination - see docs/sound-mind-codec-design.md for the
// codec's own design this tool measures, and
// docs/sound-mind-codec-eval-metrics-report.md (generated per run, next to
// the CSV report itself) for how to read the output.
//
// Never installed, never linked into the shipped app - the same spirit as
// sound-mind-benchmark (docs/sound-mind-benchmarking.md), a sibling
// developer tool this one deliberately doesn't depend on (it measures
// Core operations' speed; this measures the Codec's own fidelity, and only
// needs sound-mind-codec to do it).

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/audio_export.h"
#include "sound_mind/codec/audio_file.h"
#include "sound_mind/codec/pool_codec.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec_eval/eval_report.h"
#include "sound_mind/codec_eval/fidelity_metrics.h"
#include "sound_mind/codec_eval/process_memory.h"

namespace {

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::CompressedAudioFormat;
using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec_eval::bandSignalToNoiseRatioDb;
using sound_mind::codec_eval::correlation;
using sound_mind::codec_eval::EvalRow;
using sound_mind::codec_eval::measureTimeAndMemory;
using sound_mind::codec_eval::signalToNoiseRatioDb;
using sound_mind::codec_eval::TimedMemoryResult;

struct SweepOptions {
    std::vector<std::filesystem::path> inputFiles;
    std::vector<std::uint32_t> hopLengths{220, 441, 882};
    std::vector<std::uint32_t> binCounts{256, 512, 1024};
    std::filesystem::path outputDir{"codec-eval-results"};
};

[[nodiscard]] std::vector<std::uint32_t> parseUintList(const std::string& csv) {
    std::vector<std::uint32_t> values;
    std::stringstream stream(csv);
    std::string token;
    while (std::getline(stream, token, ',')) {
        if (!token.empty()) {
            values.push_back(static_cast<std::uint32_t>(std::stoul(token)));
        }
    }
    return values;
}

[[nodiscard]] bool consumeFlag(const std::string& arg, const std::string& name, std::string& valueOut) {
    const std::string prefix = name + "=";
    if (arg.rfind(prefix, 0) != 0) {
        return false;
    }
    valueOut = arg.substr(prefix.size());
    return true;
}

[[nodiscard]] SweepOptions parseArgs(int argc, char** argv) {
    SweepOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        std::string value;
        if (consumeFlag(arg, "--hop-lengths", value)) {
            options.hopLengths = parseUintList(value);
        } else if (consumeFlag(arg, "--bin-counts", value)) {
            options.binCounts = parseUintList(value);
        } else if (consumeFlag(arg, "--output-dir", value)) {
            options.outputDir = value;
        } else {
            options.inputFiles.emplace_back(arg);
        }
    }
    return options;
}

/// @brief One channel's worth of fidelity metrics, bundled so averaging
/// left/right into the EvalRow's own "both channels averaged" fields is a
/// single plain loop rather than six separately-threaded accumulators.
struct ChannelFidelity {
    float correlation = 0.0f;
    float snrDb = 0.0f;
    float snrLowDb = 0.0f;
    float snrMidDb = 0.0f;
    float snrHighDb = 0.0f;
};

[[nodiscard]] ChannelFidelity measureChannel(const std::vector<float>& original, const std::vector<float>& decoded,
                                              std::uint32_t sampleRateHz) {
    ChannelFidelity fidelity;
    fidelity.correlation = correlation(original, decoded);
    fidelity.snrDb = signalToNoiseRatioDb(original, decoded);
    const auto band = bandSignalToNoiseRatioDb(original, decoded, sampleRateHz);
    fidelity.snrLowDb = band.lowDb;
    fidelity.snrMidDb = band.midDb;
    fidelity.snrHighDb = band.highDb;
    return fidelity;
}

[[nodiscard]] EvalRow buildRow(const std::filesystem::path& inputFile, const std::string& codecMode,
                                const StreamCodecConfig& config, const AudioBuffer& original,
                                const AudioBuffer& decoded, const TimedMemoryResult& encodeResult,
                                const TimedMemoryResult& decodeResult, const std::filesystem::path& decodedAudioPath) {
    const ChannelFidelity left = measureChannel(original.left, decoded.left, original.sampleRateHz);
    const ChannelFidelity right = measureChannel(original.right, decoded.right, original.sampleRateHz);

    EvalRow row;
    row.inputFile = inputFile.string();
    row.codecMode = codecMode;
    row.hopLength = config.hopLength;
    row.binCount = config.binCount;
    row.encodeMs = encodeResult.elapsedMs;
    row.decodeMs = decodeResult.elapsedMs;
    row.encodeMemoryGrowthBytes = encodeResult.workingSetGrowthBytes;
    row.decodeMemoryGrowthBytes = decodeResult.workingSetGrowthBytes;
    row.correlation = 0.5f * (left.correlation + right.correlation);
    row.snrDb = 0.5f * (left.snrDb + right.snrDb);
    row.snrLowDb = 0.5f * (left.snrLowDb + right.snrLowDb);
    row.snrMidDb = 0.5f * (left.snrMidDb + right.snrMidDb);
    row.snrHighDb = 0.5f * (left.snrHighDb + right.snrHighDb);
    row.decodedAudioPath = decodedAudioPath.string();
    return row;
}

[[nodiscard]] std::filesystem::path decodedAudioPathFor(const std::filesystem::path& outputDir,
                                                          const std::filesystem::path& inputFile,
                                                          const std::string& codecMode, std::uint32_t hopLength,
                                                          std::uint32_t binCount) {
    std::ostringstream name;
    name << inputFile.stem().string() << "_" << codecMode << "_hop" << hopLength << "_bins" << binCount << ".flac";
    return outputDir / name.str();
}

void runOneCombination(const std::filesystem::path& inputFile, const AudioBuffer& original,
                        const StreamCodecConfig& config, const std::filesystem::path& outputDir,
                        std::vector<EvalRow>& rows) {
    {
        sound_mind::codec::StreamImage image;
        AudioBuffer decoded;
        const TimedMemoryResult encodeResult = measureTimeAndMemory([&] { image = sound_mind::codec::encode(original, config); });
        const TimedMemoryResult decodeResult = measureTimeAndMemory([&] { decoded = sound_mind::codec::decode(image); });

        const std::filesystem::path audioPath =
            decodedAudioPathFor(outputDir, inputFile, "Stream", config.hopLength, config.binCount);
        sound_mind::codec::exportCompressedAudio(audioPath, decoded, CompressedAudioFormat::Flac);

        rows.push_back(buildRow(inputFile, "Stream", image.config, original, decoded, encodeResult, decodeResult, audioPath));
    }
    {
        sound_mind::codec::PoolImage image;
        AudioBuffer decoded;
        const TimedMemoryResult encodeResult = measureTimeAndMemory([&] { image = sound_mind::codec::poolEncode(original, config); });
        const TimedMemoryResult decodeResult = measureTimeAndMemory([&] { decoded = sound_mind::codec::poolDecode(image); });

        const std::filesystem::path audioPath =
            decodedAudioPathFor(outputDir, inputFile, "Pool", config.hopLength, config.binCount);
        sound_mind::codec::exportCompressedAudio(audioPath, decoded, CompressedAudioFormat::Flac);

        rows.push_back(buildRow(inputFile, "Pool", image.config, original, decoded, encodeResult, decodeResult, audioPath));
    }
}

}  // namespace

int main(int argc, char** argv) {
    const SweepOptions options = parseArgs(argc, argv);
    if (options.inputFiles.empty()) {
        std::cerr << "usage: sound-mind-codec-eval <audio file> [<audio file> ...] "
                     "[--hop-lengths=441,882] [--bin-counts=256,512,1024] [--output-dir=path]\n";
        return 1;
    }

    std::error_code ec;
    std::filesystem::create_directories(options.outputDir, ec);

    std::vector<EvalRow> rows;
    for (const std::filesystem::path& inputFile : options.inputFiles) {
        AudioBuffer original;
        try {
            original = sound_mind::codec::readAudioFile(inputFile);
        } catch (const std::exception& e) {
            std::cerr << "skipping " << inputFile << ": " << e.what() << '\n';
            continue;
        }

        for (const std::uint32_t hopLength : options.hopLengths) {
            for (const std::uint32_t binCount : options.binCounts) {
                StreamCodecConfig config;
                config.hopLength = hopLength;
                config.binCount = binCount;

                std::cout << inputFile.filename().string() << "  hop=" << hopLength << "  bins=" << binCount << " ... "
                           << std::flush;
                try {
                    runOneCombination(inputFile, original, config, options.outputDir, rows);
                    std::cout << "done\n";
                } catch (const std::exception& e) {
                    std::cout << "failed: " << e.what() << '\n';
                }
            }
        }
    }

    const std::filesystem::path reportPath = options.outputDir / "codec-evaluation-metrics-report.csv";
    sound_mind::codec_eval::writeCsvReport(reportPath, rows);
    std::cout << "\nWrote " << rows.size() << " rows to " << reportPath << '\n';
    return 0;
}
