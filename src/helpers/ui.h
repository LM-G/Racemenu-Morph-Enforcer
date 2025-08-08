#pragma once
#include <chrono>
#include <string_view>

namespace helpers::ui {
    // Print to console + show a notification immediately.
    void notify(std::string_view msg);

    // Same as notify(), but rate-limited per key (to avoid spam).
    // Example: notifyThrottled("toggle", "Tracking Vilkas", 1000ms);
    void notifyThrottled(std::string_view key, std::string_view msg, std::chrono::milliseconds cooldown);

    // Print only to the in-game console (no HUD toast).
    void console(std::string_view msg);
}
