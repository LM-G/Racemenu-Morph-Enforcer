#include "ui.h"

#include <unordered_map>

#include "../logger.h"
#include "consts.h"

namespace {
    using clock = std::chrono::steady_clock;
    static std::unordered_map<std::string, clock::time_point> g_last;
}

namespace helpers::ui {

    void console(const std::string_view msg) {
        if (const auto* con = RE::ConsoleLog::GetSingleton()) {
            // ConsoleLog::Print takes a C-style format string
            const std::string s{msg};
            const_cast<RE::ConsoleLog*>(con)->Print("%s", s.c_str());
        }
    }

    void notify(const std::string_view msg) {
        // Build once so both console and HUD get the same tagged message
        std::string prefixed;
        prefixed.reserve(consts::kConsoleTag.size() + msg.size());
        prefixed.append(consts::kConsoleTag.begin(), consts::kConsoleTag.end());
        prefixed.append(msg.begin(), msg.end());

        console(prefixed);                       // console log with tag
        RE::DebugNotification(prefixed.c_str()); // HUD toast with tag
    }

    void notifyThrottled(const std::string_view key, const std::string_view msg,
                         const std::chrono::milliseconds cooldown) {
        const auto now = clock::now();
        auto& last = g_last[std::string(key)];
        if (last.time_since_epoch().count() == 0 || now - last >= cooldown) {
            last = now;
            notify(msg);
        } else {
            LOG_DEBUG("suppressed notify '{}' (cooldown {} ms)", std::string(msg), cooldown.count());
        }
    }

}  // namespace helpers::ui
