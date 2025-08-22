#include "features/morph_updater.h"

#include "core/racemenu_ei_driver.h"
#include "helpers/ui.h"
#include "logger.h"
#include "pch.h"
#include "skee.h"

namespace MorphFixer {
    using namespace std::chrono_literals;
    using namespace std::string_view_literals;

    namespace {
        inline long long now_ns() {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                .count();
        }
        inline bool contains(std::string_view s, std::string_view needle) {
            return s.find(needle) != std::string_view::npos;
        }
        inline double clamp01(double x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

        inline void post_ui(std::function<void()> fn) {
            if (auto* ti = SKSE::GetTaskInterface(); ti) {
                ti->AddUITask(std::move(fn));
            } else {
                // Should not happen post-init, but don't crash release
                fn();
            }
        }
    }

    MorphUpdater& MorphUpdater::get() {
        static MorphUpdater s;
        return s;
    }

    void MorphUpdater::setMorphInterface(SKEE::IBodyMorphInterface* bmi) noexcept {
        m_skee_bmi = bmi;
        if (!m_skee_bmi) {
            LOG_WARN("[SKEE] BodyMorph interface missing");
            return;
        }
        LOG_INFO("[SKEE] BodyMorph wired into MorphUpdater.");
    }

    void MorphUpdater::updateModelWeight(RE::TESObjectREFR* refr) noexcept {
        if (!refr) return;
        if (auto* a = refr->As<RE::Actor>()) {
            a->DoReset3D(true);
        }
    }

    RE::GFxMovieView* MorphUpdater::currentRaceMenuMovie() noexcept {
        if (auto* ui = RE::UI::GetSingleton(); ui) {
            if (ui->IsMenuOpen("RaceSex Menu"sv) || ui->IsMenuOpen("RaceMenu"sv)) {
                auto m = ui->GetMenu("RaceSex Menu"sv);
                if (!m) m = ui->GetMenu("RaceMenu"sv);
                if (m && m->uiMovie) return m->uiMovie.get();
            }
        }
        return nullptr;
    }
    bool MorphUpdater::isRaceMenuOpen() noexcept {
        if (auto* ui = RE::UI::GetSingleton(); ui)
            return ui->IsMenuOpen("RaceSex Menu"sv) || ui->IsMenuOpen("RaceMenu"sv);
        return false;
    }

    void MorphUpdater::applyNudge(RE::GFxMovieView* mv) noexcept {
        if (!mv) return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        double norm = 0.5;
        if (!Helpers::RaceMenuExternalInterface::snapshotLastWeight(norm)) {
            if (auto* base = player->GetActorBase()) norm = clamp01(base->weight / 100.0);
        }
        const double eps = 0.01;
        const double nudge = clamp01(norm - eps);
        const bool ok = Helpers::RaceMenuExternalInterface::driveChangeWeightNorm(mv, nudge);
        LOG_DEBUG("[MorphUpdater] EI ChangeWeight(nudge-only -1%) -> {}", ok);
        m_LastWasNudge.store(true, std::memory_order_relaxed);
        m_LastAppliedNs.store(now_ns(), std::memory_order_relaxed);
    }
    void MorphUpdater::applyRestore(RE::GFxMovieView* mv) noexcept {
        if (!mv) return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        double norm = 0.5;
        if (!Helpers::RaceMenuExternalInterface::snapshotLastWeight(norm)) {
            if (auto* base = player->GetActorBase()) norm = clamp01(base->weight / 100.0);
        }
        const bool ok = Helpers::RaceMenuExternalInterface::driveChangeWeightNorm(mv, norm);
        LOG_DEBUG("[MorphUpdater] EI ChangeWeight(restore-only) -> {}", ok);
        m_LastWasNudge.store(false, std::memory_order_relaxed);
        m_LastAppliedNs.store(now_ns(), std::memory_order_relaxed);
    }
    void MorphUpdater::applyNudgeRestore(RE::GFxMovieView* mv) noexcept {
        if (!mv) return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        double norm = 0.5;
        if (!Helpers::RaceMenuExternalInterface::snapshotLastWeight(norm)) {
            if (auto* base = player->GetActorBase()) norm = clamp01(base->weight / 100.0);
        }
        const bool ok = Helpers::RaceMenuExternalInterface::nudgeThenRestoreNorm(mv, norm, 0.01);
        LOG_DEBUG("[MorphUpdater] EI ChangeWeight(nudge±1% final) -> {}", ok);
        m_LastWasNudge.store(false, std::memory_order_relaxed);
        m_LastAppliedNs.store(now_ns(), std::memory_order_relaxed);
    }

    void MorphUpdater::ensureTimerThread() noexcept {
        bool expected = false;
        if (!m_TimerThreadRunning.compare_exchange_strong(expected, true)) return;

        std::thread([this] {
            LOG_TRACE("[MorphUpdater] timer thread started");
            while (true) {
                if (!m_enabled.load(std::memory_order_relaxed)) {
                    std::this_thread::sleep_for(10ms);
                    continue;
                }

                const auto due = m_LastWillApplyNs.load(std::memory_order_relaxed);
                if (due > 0) {
                    const auto now = now_ns();
                    if (now >= due) {
                        // Only flush if we’ve been idle a bit (150 ms)
                        const auto last = m_LastAppliedNs.load(std::memory_order_relaxed);
                        const long long minGapNs = 150LL * 1'000'000LL;
                        if (last < 0 || (now - last) >= minGapNs) {
                            const bool wasNudge = m_LastWasNudge.load(std::memory_order_relaxed);
                            post_ui([this, wasNudge] {
                                if (auto* mv = currentRaceMenuMovie(); mv && isRaceMenuOpen()) {
                                    if (wasNudge) {
                                        LOG_DEBUG("[MorphUpdater] ChangeWeight Operation (tail) for: <idle>");
                                        applyRestore(mv);  // finish from nudge → restore-only
                                    } else {
                                        LOG_DEBUG("[MorphUpdater] ChangeWeight Operation (tail) for: <idle>");
                                        applyNudgeRestore(mv);  // single-tap / balanced finish
                                    }
                                }
                            });
                        }
                        m_LastWillApplyNs.store(-1, std::memory_order_relaxed);  // disarm
                    } else {
                        const auto remain = due - now;
                        const auto ms = std::max<long long>(1, remain / 1'000'000LL);
                        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                        continue;
                    }
                }

                std::this_thread::sleep_for(5ms);
            }
        }).detach();
    }

    void MorphUpdater::onGfxEvent(const char* nameC, const RE::GFxValue* args, std::uint32_t argc) noexcept {
        if (!m_enabled.load(std::memory_order_relaxed)) return;
        if (!nameC) return;

        std::string_view name{nameC};

        // Step 1: store arg0 (for diagnostics / future routing)
        if (argc >= 1 && args && args[0].IsNumber()) {
            m_LastAnyArg0.store(args[0].GetNumber(), std::memory_order_relaxed);
        }

        // Steps 2–3: only "Change*" events
        if (!contains(name, "Change"sv)) return;

        // Step 4: blacklist events we won't treat as slider changes
        if (name == "ChangeWeight"sv || name == "ChangeRace"sv || name == "ChangeSex"sv ||
            name == "ChangeDoubleMorph"sv || name == "ChangeHeadPart"sv)
            return;

        // Step 5: cadence
        static std::mutex mu;
        std::lock_guard lk(mu);

        if (!isRaceMenuOpen()) return;

        const bool nameChanged = (m_LastEventName != name);
        const auto now = now_ns();
        const int thr = std::max(0, m_throttle_ms.load(std::memory_order_relaxed));
        const long long thrNs = static_cast<long long>(thr) * 1'000'000LL;

        const auto last = m_LastAppliedNs.load(std::memory_order_relaxed);
        const bool okToApplyNow = nameChanged || last < 0 || (now - last) >= thrNs;

        // We never touch GFx here; queue to UI thread so RaceMenu finishes handling this EI first.
        if (okToApplyNow) {
            const bool doNudge = !m_LastWasNudge.load(std::memory_order_relaxed);
            std::string src(name);
            post_ui([this, doNudge, src = std::move(src)] {
                if (auto* mv = currentRaceMenuMovie(); mv && isRaceMenuOpen()) {
                    LOG_DEBUG("[MorphUpdater] ChangeWeight Operation applied for: {} event", src);
                    if (doNudge) {
                        applyNudge(mv);
                    } else {
                        applyRestore(mv);
                    }
                }
            });
            m_LastEventName.assign(name.data(), name.size());
        }

        // Always schedule the "last" cleanup tick
        const long long due = now + (thr + 50) * 1'000'000LL;
        m_LastWillApplyNs.store(due, std::memory_order_relaxed);
        ensureTimerThread();
    }
}  // namespace MorphFixer
