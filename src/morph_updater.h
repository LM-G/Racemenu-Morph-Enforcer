#pragma once

#include <atomic>

// forward-declare SKEE type to avoid pulling skee.h in the header
namespace SKEE { class IBodyMorphInterface; }

class morph_updater {
public:
    static morph_updater& get();
    morph_updater(const morph_updater&) = delete;
    morph_updater& operator=(const morph_updater&) = delete;

    // Enable/disable updates (racemenu_watcher toggles this with menu open/close)
    void set_enabled(bool e) { enabled_.store(e); }

    // Called by racemenu_event_watcher on slider changes
    void notify_slider_changed();

    // Optional: adjust throttle at runtime (ms)
    void set_throttle_ms(int ms) { throttle_ms_.store(ms); }
    void set_stage2_delay_ms(int ms) { stage2_delay_ms_.store(ms); }

    // SKEE hookup (called from main once you have the BodyMorph interface)
    void setMorphInterface(SKEE::IBodyMorphInterface* bmi) noexcept;
    // model refresh that prefers SKEE if available, else falls back to vanilla
    void updateModelWeight(RE::TESObjectREFR* refr) noexcept;
private:
    morph_updater() = default;


    // SKEE BodyMorph interface pointer (null when SKEE not present)
    SKEE::IBodyMorphInterface* skee_bmi_{nullptr};

    void schedule_refresh_throttled();
    void apply();

    // state
    std::atomic<bool> enabled_{false};
    std::atomic<bool> throttle_scheduled_{false};

    // timing bookkeeping (ns since epoch, -1 = unset)
    std::atomic<long long> last_slider_ns_{-1};
    std::atomic<long long> scheduled_ns_{-1};
    std::atomic<long long> last_refresh_ns_{-1};
    std::atomic<unsigned>  coalesced_count_{0};

    std::atomic<int> throttle_ms_{250}; // delay before stage1
    std::atomic<int> stage2_delay_ms_{100}; // delay between stage1 and stage2
};
