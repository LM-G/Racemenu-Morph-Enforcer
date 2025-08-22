#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace MorphFixer {
    namespace Helpers::String {
        [[nodiscard]] std::string toUtf8(std::wstring_view ws);

        // Split a comma/semicolon list and trim ASCII spaces around each token.
        // Empty/whitespace-only tokens are skipped.
        [[nodiscard]] std::vector<std::string> splitList(std::string_view input_string);
    }
}
