#pragma once

#include <atomic>
#include <string>

// forward-declare SKEE type to avoid pulling skee.h in the header
namespace RE { class GFxMovieView; class GFxValue; class TESObjectREFR; }
namespace SKEE { class IBodyMorphInterface; }

class morph_updater {
public:
    static morph_updater& get();
    morph_updater(const morph_updater&) = delete;
    morph_updater& operator=(const morph_updater&) = delete;

    // Enable/disable updates (racemenu_watcher toggles this with menu open/close)
    void set_enabled(bool e) { enabled_.store(e); }

    // Driver for gfx-EI calls (TracingExternalInterface::Callback must call this)
    void onGfxEvent(const char* name, const RE::GFxValue* args, std::uint32_t argc) noexcept;

    // Settings / wiring
    void set_throttle_ms(int ms) { throttle_ms_.store(ms); }
    void setMorphInterface(SKEE::IBodyMorphInterface* bmi) noexcept;

    // Heavy path outside RaceMenu
    void updateModelWeight(RE::TESObjectREFR* refr) noexcept;

    void onMenuClosed() noexcept {
        primed_.store(false);
        lastEventName_.clear();
        lastAppliedNs_.store(-1);
        lastWillApplyNs_.store(-1);
        lastWasNudge_.store(false);
    }

private:
    morph_updater() = default;

    static RE::GFxMovieView* currentRaceMenuMovie() noexcept;
    static bool isRaceMenuOpen() noexcept;

    void applyNudge(RE::GFxMovieView* mv) noexcept;
    void applyRestore(RE::GFxMovieView* mv) noexcept;
    void applyNudgeRestore(RE::GFxMovieView* mv) noexcept;

    void ensureTimerThread() noexcept;

    // state
    std::atomic<bool> enabled_{false};
    std::atomic<int>  throttle_ms_{100}; // default; INI may override

    std::string            lastEventName_;
    std::atomic<long long> lastAppliedNs_{-1};
    std::atomic<long long> lastWillApplyNs_{-1};
    std::atomic<bool>      lastWasNudge_{false};
    std::atomic<bool>      timerThreadRunning_{false};

    // FYI: last arg0 seen from any EI call
    std::atomic<double>    lastAnyArg0_{0.0};

    // SKEE
    SKEE::IBodyMorphInterface* skee_bmi_{nullptr};

    std::atomic<bool>      primed_{false};   // NEW: becomes true after first real slider "Change*"
};
