#include "sound_mind/codec_eval/eval_report.h"

#include <fstream>
#include <ios>
#include <sstream>
#include <stdexcept>

namespace sound_mind::codec_eval {

namespace {

/// @brief Wraps `value` in double quotes, doubling any embedded quote - the
///        standard CSV escaping rule, applied unconditionally (every field
///        is always quoted, not just ones that need it) so this stays a
///        handful of lines rather than a comma/quote-sniffing dialect
///        decision.
[[nodiscard]] std::string csvField(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (const char c : value) {
        if (c == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(c);
    }
    escaped.push_back('"');
    return escaped;
}

template <typename T>
[[nodiscard]] std::string csvField(const T& value) {
    std::ostringstream stream;
    stream << value;
    return csvField(stream.str());
}

}  // namespace

void writeCsvReport(const std::filesystem::path& path, const std::vector<EvalRow>& rows) {
    std::ofstream stream(path, std::ios::trunc);
    if (!stream) {
        throw std::ios_base::failure("could not open codec evaluation report for writing: " + path.string());
    }
    stream.exceptions(std::ios::badbit | std::ios::failbit);

    stream << "InputFile,CodecMode,HopLength,BinCount,EncodeMs,DecodeMs,EncodeMemoryGrowthBytes,"
              "DecodeMemoryGrowthBytes,Correlation,SnrDb,SnrLowDb,SnrMidDb,SnrHighDb,DecodedAudioPath,ListeningNotes\n";

    for (const EvalRow& row : rows) {
        stream << csvField(row.inputFile) << ',' << csvField(row.codecMode) << ',' << csvField(row.hopLength) << ','
               << csvField(row.binCount) << ',' << csvField(row.encodeMs) << ',' << csvField(row.decodeMs) << ','
               << csvField(row.encodeMemoryGrowthBytes) << ',' << csvField(row.decodeMemoryGrowthBytes) << ','
               << csvField(row.correlation) << ',' << csvField(row.snrDb) << ',' << csvField(row.snrLowDb) << ','
               << csvField(row.snrMidDb) << ',' << csvField(row.snrHighDb) << ',' << csvField(row.decodedAudioPath)
               << ',' << csvField(std::string())  // ListeningNotes - always blank, for the user to fill in by hand.
               << '\n';
    }
}

}  // namespace sound_mind::codec_eval
