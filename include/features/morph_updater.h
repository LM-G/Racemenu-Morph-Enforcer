#pragma once

#include <atomic>
#include <string>

namespace RE {
    class GFxMovieView;
    class GFxValue;
    class TESObjectREFR;
}
namespace SKEE {
    class IBodyMorphInterface;
}

namespace MorphFixer {
    // forward-declare SKEE type to avoid pulling skee.h in the header

    class MorphUpdater {
    public:
        static MorphUpdater& get();
        MorphUpdater(const MorphUpdater&) = delete;
        MorphUpdater& operator=(const MorphUpdater&) = delete;

        // Enable/disable updates (RaceMenuWatcher toggles this with menu open/close)
        void setEnabled(bool e) { m_enabled.store(e); }

        // Driver for gfx-EI calls (TracingExternalInterface::Callback must call this)
        void onGfxEvent(const char* name, const RE::GFxValue* args, std::uint32_t argc) noexcept;

        // Settings / wiring
        void setThrottleMs(int ms) { m_throttle_ms.store(ms); }
        void setMorphInterface(SKEE::IBodyMorphInterface* bmi) noexcept;

        // Heavy path outside RaceMenu
        void updateModelWeight(RE::TESObjectREFR* refr) noexcept;

        void onMenuClosed() noexcept {
            m_primed.store(false);
            m_LastEventName.clear();
            m_LastAppliedNs.store(-1);
            m_LastWillApplyNs.store(-1);
            m_LastWasNudge.store(false);
            // End any in-progress session and clear baseline
            m_SessionActive.store(false);
            m_LastBaselineNorm.store(-1.0);
        }

    private:
        MorphUpdater() = default;

        static RE::GFxMovieView* currentRaceMenuMovie() noexcept;
        static bool isRaceMenuOpen() noexcept;

        void applyNudge(RE::GFxMovieView* mv) noexcept;
        void applyRestore(RE::GFxMovieView* mv) noexcept;
        void applyNudgeRestore(RE::GFxMovieView* mv) noexcept;

        void ensureTimerThread() noexcept;

        // state
        std::atomic<bool> m_enabled{false};
        std::atomic<int> m_throttle_ms{100};  // default; INI may override

        std::string m_LastEventName;
        std::atomic<long long> m_LastAppliedNs{-1};
        std::atomic<long long> m_LastWillApplyNs{-1};
        std::atomic<bool> m_LastWasNudge{false};
        std::atomic<bool> m_TimerThreadRunning{false};

        // FYI: last arg0 seen from any EI call
        std::atomic<double> m_LastAnyArg0{0.0};

        // Baseline weight captured for the CURRENT SESSION (in [0,1]; <0 means unset)
        std::atomic<double> m_LastBaselineNorm{-1.0};

        // SKEE
        SKEE::IBodyMorphInterface* m_skee_bmi{nullptr};

        std::atomic<bool> m_primed{false};

        // --- Update-session control (NEW) ---
        std::atomic<bool> m_SessionActive{false};
    };
}  // namespace MorphFixer
