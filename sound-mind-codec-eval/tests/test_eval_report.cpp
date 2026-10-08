#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "sound_mind/codec_eval/eval_report.h"

using sound_mind::codec_eval::EvalRow;
using sound_mind::codec_eval::writeCsvReport;

namespace {

std::vector<std::string> readLines(const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

EvalRow makeRow() {
    EvalRow row;
    row.inputFile = "test.wav";
    row.codecMode = "Stream";
    row.hopLength = 441;
    row.binCount = 512;
    row.encodeMs = 12.5;
    row.decodeMs = 8.25;
    row.encodeMemoryGrowthBytes = 1024;
    row.decodeMemoryGrowthBytes = 2048;
    row.correlation = 0.9998f;
    row.snrDb = 42.5f;
    row.snrLowDb = 50.0f;
    row.snrMidDb = 40.0f;
    row.snrHighDb = 30.0f;
    row.decodedAudioPath = "out/test_Stream_441_512.flac";
    return row;
}

}  // namespace

TEST_CASE("writeCsvReport() writes a header row ending in ListeningNotes", "[eval_report]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-codec-eval-test-report.csv";
    writeCsvReport(path, {makeRow()});

    const std::vector<std::string> lines = readLines(path);
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].find("ListeningNotes") != std::string::npos);
    CHECK(lines[0].find("InputFile") != std::string::npos);
    CHECK(lines[0].find("CodecMode") != std::string::npos);

    std::filesystem::remove(path);
}

TEST_CASE("writeCsvReport() writes one data row per input row, trailing an empty ListeningNotes field",
          "[eval_report]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-codec-eval-test-report-rows.csv";
    writeCsvReport(path, {makeRow(), makeRow()});

    const std::vector<std::string> lines = readLines(path);
    REQUIRE(lines.size() == 3);
    CHECK(lines[1].find("test.wav") != std::string::npos);
    CHECK(lines[1].find("Stream") != std::string::npos);
    // The trailing column is the always-empty ListeningNotes field - the
    // quoted row should end in an empty pair of quotes.
    CHECK(lines[1].ends_with("\"\""));
    CHECK(lines[2].ends_with("\"\""));

    std::filesystem::remove(path);
}

TEST_CASE("writeCsvReport() with no rows still writes just the header", "[eval_report]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-codec-eval-test-report-empty.csv";
    writeCsvReport(path, {});

    const std::vector<std::string> lines = readLines(path);
    REQUIRE(lines.size() == 1);

    std::filesystem::remove(path);
}

TEST_CASE("writeCsvReport() quotes a field containing a comma so it survives spreadsheet parsing",
          "[eval_report]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-codec-eval-test-report-comma.csv";
    EvalRow row = makeRow();
    row.inputFile = "C:/audio, test/clip.wav";
    writeCsvReport(path, {row});

    const std::vector<std::string> lines = readLines(path);
    REQUIRE(lines.size() == 2);
    CHECK(lines[1].find("\"C:/audio, test/clip.wav\"") != std::string::npos);

    std::filesystem::remove(path);
}
