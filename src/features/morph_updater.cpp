#include "features/morph_updater.h"

#include "core/racemenu_ei_driver.h"
#include "helpers/consts.h"
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

        inline double read_current_norm_baseline() {
            double norm = 0.5;
            if (!Helpers::RaceMenuExternalInterface::snapshotLastWeight(norm)) {
                if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                    if (auto* base = player->GetActorBase()) {
                        norm = clamp01(base->weight / 100.0);
                    }
                }
            }
            return clamp01(norm);
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

        // Use session baseline (prevents drift across taps)
        double baseline = m_LastBaselineNorm.load(std::memory_order_relaxed);
        if (baseline < 0.0 || baseline > 1.0) {
            baseline = read_current_norm_baseline();
        }

        // At 0.0, nudge UP by +1% instead of down
        const double eps = 0.01;
        const bool nudgeUp = (baseline <= 0.0);
        const double target = clamp01(baseline + (nudgeUp ? +eps : -eps));

        const bool ok = Helpers::RaceMenuExternalInterface::driveChangeWeightNorm(mv, target);
        LOG_DEBUG("[MorphUpdater] EI ChangeWeight(nudge-only {}1%) -> {}", nudgeUp ? "+" : "-", ok);

        m_LastWasNudge.store(true, std::memory_order_relaxed);
        m_LastAppliedNs.store(now_ns(), std::memory_order_relaxed);
    }

    void MorphUpdater::applyRestore(RE::GFxMovieView* mv) noexcept {
        if (!mv) return;
        // Restore to session baseline
        double baseline = m_LastBaselineNorm.load(std::memory_order_relaxed);
        if (baseline < 0.0 || baseline > 1.0) {
            baseline = read_current_norm_baseline();
        }
        const bool ok = Helpers::RaceMenuExternalInterface::driveChangeWeightNorm(mv, baseline);
        LOG_DEBUG("[MorphUpdater] EI ChangeWeight(restore-only) -> {}", ok);
        m_LastWasNudge.store(false, std::memory_order_relaxed);
        m_LastAppliedNs.store(now_ns(), std::memory_order_relaxed);
    }

    void MorphUpdater::applyNudgeRestore(RE::GFxMovieView* mv) noexcept {
        if (!mv) return;
        // Use session baseline to keep nudge+restore symmetric and drift-free
        double baseline = m_LastBaselineNorm.load(std::memory_order_relaxed);
        if (baseline < 0.0 || baseline > 1.0) {
            baseline = read_current_norm_baseline();
        }
        const bool ok = Helpers::RaceMenuExternalInterface::nudgeThenRestoreNorm(mv, baseline, 0.01);
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
                                    LOG_DEBUG("[MorphUpdater] ChangeWeight Operation (tail) for: <idle>");
                                    if (wasNudge) {
                                        applyRestore(mv);  // finish from nudge → restore-only
                                    } else {
                                        applyNudgeRestore(mv);  // single-tap / balanced finish
                                    }
                                    // --- END SESSION ---
                                    m_SessionActive.store(false, std::memory_order_relaxed);
                                    m_LastWasNudge.store(false, std::memory_order_relaxed);
                                    LOG_DEBUG("[MorphUpdater] Session end;");
                                }
                            });
                            // disarm after executing tail
                            m_LastWillApplyNs.store(-1, std::memory_order_relaxed);
                        } else {
                            // Not enough idle gap yet; push due forward to guarantee tail will run ~150ms after last
                            // apply
                            const long long newDue = last + minGapNs;
                            m_LastWillApplyNs.store(newDue, std::memory_order_relaxed);
                        }
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

        // --- SPECIAL: ChangeWeight carries the live weight value ---
        if (name == "ChangeWeight"sv) {
            // arg1 is the normalized value in logs; guard against bad argc/types
            if (argc >= 2 && args && args[1].IsNumber()) {
                const double norm = clamp01(args[1].GetNumber());
                // Only record origin when NOT in an update session
                if (!m_SessionActive.load(std::memory_order_relaxed)) {
                    m_LastBaselineNorm.store(norm, std::memory_order_relaxed);
                    LOG_DEBUG("[MorphUpdater] primed baseline from ChangeWeight (no session): norm={:.3f}", norm);
                }
            }
            return;  // never treat ChangeWeight itself as a slider-change trigger
        }

        // Steps 2–3: only "Change*" slider-ish events
        if (!contains(name, "Change"sv)) return;

        // Step 4: blacklist events we won't treat as slider changes (ChangeWeight handled above)
        if (name == "ChangeRace"sv || name == "ChangeSex"sv || name == "ChangeMenuOpen"sv ||
            name == "ChangeMenuClose"sv) {
            return;
        }

        // Step 5: cadence
        static std::mutex mu;
        std::lock_guard lk(mu);

        if (!isRaceMenuOpen()) {
            return;
        }

        const bool nameChanged = (m_LastEventName != name);
        const auto now = now_ns();
        const int thr = std::max(0, m_throttle_ms.load(std::memory_order_relaxed));
        const long long thrNs = static_cast<long long>(thr) * 1'000'000LL;

        const auto last = m_LastAppliedNs.load(std::memory_order_relaxed);
        const bool okToApplyNow = nameChanged || last < 0 || (now - last) >= thrNs;

        if (okToApplyNow) {
            // --- START SESSION  ---
            bool wasActive = m_SessionActive.exchange(true, std::memory_order_relaxed);
            if (!wasActive) {
                // If baseline wasn't filled by a prior ChangeWeight, fall back to a snapshot now.
                double cur = m_LastBaselineNorm.load(std::memory_order_relaxed);
                if (cur < 0.0 || cur > 1.0) {
                    cur = read_current_norm_baseline();
                    m_LastBaselineNorm.store(cur, std::memory_order_relaxed);
                    LOG_DEBUG("[MorphUpdater] Session start; baseline snapshot: norm={:.3f}", cur);
                } else {
                    LOG_DEBUG("[MorphUpdater] Session start; baseline already primed: norm={:.3f}", cur);
                }
            }

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
        const long long due = now + (static_cast<long long>(thr) * Helpers::Consts::NS_PER_MS);
        m_LastWillApplyNs.store(due, std::memory_order_relaxed);
        ensureTimerThread();
    }
}  // namespace MorphFixer
