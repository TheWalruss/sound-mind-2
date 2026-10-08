#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sound_mind::codec_eval {

/// @brief One (input file, codec mode, hop length, bin count) combination's
///        worth of measurements - one row of the CSV
///        writeCsvReport() produces.
struct EvalRow {
    /// @brief The input audio file this row encoded/decoded.
    std::string inputFile;

    /// @brief Which codec this row measured - `"Stream"` or `"Pool"`.
    std::string codecMode;

    /// @brief The `StreamCodecConfig::hopLength` used (samples/column).
    std::uint32_t hopLength = 0;

    /// @brief The `StreamCodecConfig::binCount` used.
    std::uint32_t binCount = 0;

    /// @brief Wall-clock time to encode, in milliseconds.
    double encodeMs = 0.0;

    /// @brief Wall-clock time to decode, in milliseconds.
    double decodeMs = 0.0;

    /// @brief Approximate process working-set growth during encode, in
    ///        bytes - see `measureTimeAndMemory()`'s own docs on why this
    ///        is approximate.
    std::uint64_t encodeMemoryGrowthBytes = 0;

    /// @brief Approximate process working-set growth during decode, in
    ///        bytes - see `measureTimeAndMemory()`'s own docs.
    std::uint64_t decodeMemoryGrowthBytes = 0;

    /// @brief Overall time-domain Pearson correlation between the original
    ///        and decoded audio (both channels averaged) - see
    ///        `correlation()`'s own docs.
    float correlation = 0.0f;

    /// @brief Overall time-domain SNR in dB (both channels averaged) - see
    ///        `signalToNoiseRatioDb()`'s own docs.
    float snrDb = 0.0f;

    /// @brief Low-band (<250 Hz) SNR in dB (both channels averaged).
    float snrLowDb = 0.0f;

    /// @brief Mid-band (250-4000 Hz) SNR in dB (both channels averaged).
    float snrMidDb = 0.0f;

    /// @brief High-band (>4000 Hz) SNR in dB (both channels averaged).
    float snrHighDb = 0.0f;

    /// @brief Path to the decoded audio this row wrote to disk, for
    ///        listening to directly - empty if writing it failed.
    std::string decodedAudioPath;
};

/// @brief Writes a CSV report of every measured row, with one trailing
///        `ListeningNotes` column always left blank - per the user's own
///        request, a place to write subjective listening feedback (e.g. in
///        a spreadsheet, after listening to each row's own
///        `decodedAudioPath`) alongside the automated metrics, in the same
///        file rather than a separate one.
///
/// Every field is quoted (simple, always-quote CSV, no library dependency)
/// so a `inputFile`/`decodedAudioPath` path containing a comma still parses
/// correctly in a spreadsheet.
///
/// @param path Destination path (overwritten if it already exists).
/// @param rows The rows to write, in order.
/// @throws std::ios_base::failure if the file can't be written.
void writeCsvReport(const std::filesystem::path& path, const std::vector<EvalRow>& rows);

}  // namespace sound_mind::codec_eval
