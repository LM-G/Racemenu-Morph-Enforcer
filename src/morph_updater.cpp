#include "morph_updater.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <string_view>
#include <functional>

#include "RE/A/Actor.h"
#include "RE/G/GFxMovieView.h"
#include "RE/G/GFxValue.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/T/TESNPC.h"
#include "RE/U/UI.h"

#include "SKSE/API.h"         // for GetTaskInterface / AddUITask
#include "SKSE/Interfaces.h"

#include "helpers/racemenu_ei_driver.h"
#include "helpers/ui.h"
#include "logger.h"
#include "pch.h"
#include "skee.h"

using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace {
    inline long long now_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
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

morph_updater& morph_updater::get() {
    static morph_updater s;
    return s;
}

void morph_updater::setMorphInterface(SKEE::IBodyMorphInterface* bmi) noexcept {
    skee_bmi_ = bmi;
    if (!skee_bmi_) {
        LOG_WARN("[SKEE] BodyMorph interface missing");
        return;
    }
    LOG_INFO("[SKEE] BodyMorph wired into morph_updater.");
}

void morph_updater::updateModelWeight(RE::TESObjectREFR* refr) noexcept {
    if (!refr) return;
    if (auto* a = refr->As<RE::Actor>()) {
        a->DoReset3D(true);
    }
}

RE::GFxMovieView* morph_updater::currentRaceMenuMovie() noexcept {
    if (auto* ui = RE::UI::GetSingleton(); ui) {
        if (ui->IsMenuOpen("RaceSex Menu"sv) || ui->IsMenuOpen("RaceMenu"sv)) {
            auto m = ui->GetMenu("RaceSex Menu"sv);
            if (!m) m = ui->GetMenu("RaceMenu"sv);
            if (m && m->uiMovie) return m->uiMovie.get();
        }
    }
    return nullptr;
}
bool morph_updater::isRaceMenuOpen() noexcept {
    if (auto* ui = RE::UI::GetSingleton(); ui)
        return ui->IsMenuOpen("RaceSex Menu"sv) || ui->IsMenuOpen("RaceMenu"sv);
    return false;
}

void morph_updater::applyNudge(RE::GFxMovieView* mv) noexcept {
    if (!mv) return;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    double norm = 0.5;
    if (!helpers::racemenu_ei::snapshot_last_weight(norm)) {
        if (auto* base = player->GetActorBase())
            norm = clamp01(base->weight / 100.0);
    }
    const double eps = 0.01;
    const double nudge = clamp01(norm - eps);
    const bool ok = helpers::racemenu_ei::drive_changeweight_norm(mv, nudge);
    LOG_DEBUG("[morph_updater] EI ChangeWeight(nudge-only -1%) -> {}", ok);
    lastWasNudge_.store(true, std::memory_order_relaxed);
    lastAppliedNs_.store(now_ns(), std::memory_order_relaxed);
}
void morph_updater::applyRestore(RE::GFxMovieView* mv) noexcept {
    if (!mv) return;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    double norm = 0.5;
    if (!helpers::racemenu_ei::snapshot_last_weight(norm)) {
        if (auto* base = player->GetActorBase())
            norm = clamp01(base->weight / 100.0);
    }
    const bool ok = helpers::racemenu_ei::drive_changeweight_norm(mv, norm);
    LOG_DEBUG("[morph_updater] EI ChangeWeight(restore-only) -> {}", ok);
    lastWasNudge_.store(false, std::memory_order_relaxed);
    lastAppliedNs_.store(now_ns(), std::memory_order_relaxed);
}
void morph_updater::applyNudgeRestore(RE::GFxMovieView* mv) noexcept {
    if (!mv) return;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    double norm = 0.5;
    if (!helpers::racemenu_ei::snapshot_last_weight(norm)) {
        if (auto* base = player->GetActorBase())
            norm = clamp01(base->weight / 100.0);
    }
    const bool ok = helpers::racemenu_ei::nudge_then_restore_norm(mv, norm, 0.01);
    LOG_DEBUG("[morph_updater] EI ChangeWeight(nudge±1% final) -> {}", ok);
    lastWasNudge_.store(false, std::memory_order_relaxed);
    lastAppliedNs_.store(now_ns(), std::memory_order_relaxed);
}

void morph_updater::ensureTimerThread() noexcept {
    bool expected = false;
    if (!timerThreadRunning_.compare_exchange_strong(expected, true)) return;

    std::thread([this] {
        LOG_TRACE("[morph_updater] timer thread started");
        while (true) {
            if (!enabled_.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(10ms);
                continue;
            }

            const auto due = lastWillApplyNs_.load(std::memory_order_relaxed);
            if (due > 0) {
                const auto now = now_ns();
                if (now >= due) {
                    // Only flush if we’ve been idle a bit (150 ms)
                    const auto last = lastAppliedNs_.load(std::memory_order_relaxed);
                    const long long minGapNs = 150LL * 1'000'000LL;
                    if (last < 0 || (now - last) >= minGapNs) {
                        const bool wasNudge = lastWasNudge_.load(std::memory_order_relaxed);
                        post_ui([this, wasNudge] {
                            if (auto* mv = currentRaceMenuMovie(); mv && isRaceMenuOpen()) {
                                if (wasNudge) {
                                    LOG_DEBUG("[morph_updater] ChangeWeight Operation (tail) for: <idle>");
                                    applyRestore(mv);        // finish from nudge → restore-only
                                } else {
                                    LOG_DEBUG("[morph_updater] ChangeWeight Operation (tail) for: <idle>");
                                    applyNudgeRestore(mv);   // single-tap / balanced finish
                                }
                            }
                        });
                    }
                    lastWillApplyNs_.store(-1, std::memory_order_relaxed); // disarm
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

void morph_updater::onGfxEvent(const char* nameC, const RE::GFxValue* args, std::uint32_t argc) noexcept {
    if (!enabled_.load(std::memory_order_relaxed)) return;
    if (!nameC) return;

    std::string_view name{nameC};

    // Step 1: store arg0 (for diagnostics / future routing)
    if (argc >= 1 && args && args[0].IsNumber()) {
        lastAnyArg0_.store(args[0].GetNumber(), std::memory_order_relaxed);
    }

    // Steps 2–3: only "Change*" events
    if (!contains(name, "Change"sv))
        return;

    // Step 4: blacklist events we won't treat as slider changes
    if (name == "ChangeWeight"sv || name == "ChangeRace"sv || name == "ChangeSex"sv || name == "ChangeDoubleMorph"sv || name == "ChangeHeadPart"sv)
        return;

    // Step 5: cadence
    static std::mutex mu;
    std::lock_guard lk(mu);

    if (!isRaceMenuOpen()) return;

    const bool nameChanged = (lastEventName_ != name);
    const auto now = now_ns();
    const int thr = std::max(0, throttle_ms_.load(std::memory_order_relaxed));
    const long long thrNs = static_cast<long long>(thr) * 1'000'000LL;

    const auto last = lastAppliedNs_.load(std::memory_order_relaxed);
    const bool okToApplyNow = nameChanged || last < 0 || (now - last) >= thrNs;

    // We never touch GFx here; queue to UI thread so RaceMenu finishes handling this EI first.
    if (okToApplyNow) {
        const bool doNudge = !lastWasNudge_.load(std::memory_order_relaxed);
        std::string src(name);
        post_ui([this, doNudge, src = std::move(src)] {
            if (auto* mv = currentRaceMenuMovie(); mv && isRaceMenuOpen()) {
                LOG_DEBUG("[morph_updater] ChangeWeight Operation applied for: {} event", src);
                if (doNudge) {
                    applyNudge(mv);
                } else {
                    applyRestore(mv);
                }
            }
        });
        lastEventName_.assign(name.data(), name.size());
    }

    // Always schedule the "last" cleanup tick
    const long long due = now + (thr + 50) * 1'000'000LL;
    lastWillApplyNs_.store(due, std::memory_order_relaxed);
    ensureTimerThread();
}
