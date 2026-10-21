#pragma once

#include <filesystem>
#include <optional>

#include <QStringList>

namespace sound_mind::studio {

/**
 * @brief The project a command line asks the Studio to open (`v0.1.42.2`) -
 *        how double-clicking a `.smproj` file opens it, once the MSIX
 *        package (or anything else) associates the file type with the
 *        Studio, which then starts as `sound-mind-studio.exe <path>`.
 * @param arguments The full argument list, program name first, as
 *        `QCoreApplication::arguments()` gives it.
 * @return The first argument after the program name ending in `.smproj` (any
 *         case), or `std::nullopt`. The file isn't checked for existence -
 *         opening it reports any problem.
 */
[[nodiscard]] std::optional<std::filesystem::path> projectToOpenFromArguments(const QStringList& arguments);

}  // namespace sound_mind::studio
