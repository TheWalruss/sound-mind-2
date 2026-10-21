#include "sound_mind/studio/command_line.h"

namespace sound_mind::studio {

std::optional<std::filesystem::path> projectToOpenFromArguments(const QStringList& arguments) {
    for (qsizetype i = 1; i < arguments.size(); ++i) {
        if (arguments[i].endsWith(QStringLiteral(".smproj"), Qt::CaseInsensitive)) {
            return std::filesystem::path(arguments[i].toStdWString());
        }
    }
    return std::nullopt;
}

}  // namespace sound_mind::studio
