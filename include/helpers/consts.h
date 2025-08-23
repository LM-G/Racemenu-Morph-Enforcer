#pragma once
#include <string_view>

namespace MorphFixer {
    namespace Helpers::Consts {
        // Prefix shown in console + HUD toasts for human-friendly tagging.
        inline constexpr std::string_view CONSOLE_TAG = "[racemenu morph fixer] ";
        inline constexpr long long NS_PER_MS = 1'000'000LL;
    }
}