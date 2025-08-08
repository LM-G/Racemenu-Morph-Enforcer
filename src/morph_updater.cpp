#include "morph_updater.h"

#include "RE/A/Actor.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/T/TESNPC.h"
#include "RE/U/UI.h"
#include "helpers/ui.h"
#include "logger.h"
#include "pch.h"
#include "skee.h"

// EI driver
#include "helpers/racemenu_ei_driver.h"

#include <algorithm>
#include <atomic>
#include <thread>

using namespace std::chrono_literals;

namespace {
    long long now_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    double ms_between(long long newer_ns, long long older_ns) {
        if (older_ns < 0 || newer_ns < 0) return -1.0;
        return static_cast<double>(newer_ns - older_ns) / 1'000'000.0;
    }

    // For “last event wins” throttling without touching headers
    static std::atomic<uint64_t> s_seq{0};            // bumped on every slider notify
    static std::atomic<uint64_t> s_seq_handled{0};    // last sequence applied
    static std::atomic<long long> s_last_fire_ns{0};  // last apply() time
}

morph_updater& morph_updater::get() {
    static morph_updater s;
    return s;
}

void morph_updater::updateModelWeight(RE::TESObjectREFR* refr) noexcept {
    if (!refr) return;

    if (auto* a = refr->As<RE::Actor>()) {
        a->DoReset3D(true);
    }
}

void morph_updater::setMorphInterface(SKEE::IBodyMorphInterface* bmi) noexcept {
    if (!bmi) {
        LOG_ERROR("BodyMorph interface is null");
        return;
    }

    const bool firstWire = (skee_bmi_ == nullptr);
    skee_bmi_ = bmi;

    if (firstWire) {
        LOG_INFO("[SKEE] BodyMorph wired into morph_updater.");
    } else {
        LOG_INFO("[SKEE] BodyMorph interface refreshed.");
    }
}

void morph_updater::notify_slider_changed() {
    if (!enabled_.load()) return;

    const auto t = now_ns();
    const auto prev = last_slider_ns_.exchange(t);
    const double dt_ms = ms_between(t, prev);

    if (dt_ms >= 0.0) {
        LOG_DEBUG("[morph_updater] slider tick; Δ={:.2f} ms", dt_ms);
    } else {
        LOG_DEBUG("[morph_updater] first slider tick in session");
    }

    // bump sequence and arm the throttle loop
    s_seq.fetch_add(1, std::memory_order_relaxed);
    schedule_refresh_throttled(); // repurposed as throttle loop
}

// THROTTLE LOOP (fires at most once per throttle_ms; last-event-wins)
void morph_updater::schedule_refresh_throttled() {
    bool expected = false;
    if (!throttle_scheduled_.compare_exchange_strong(expected, true)) {
        // already running; just coalesce via s_seq
        return;
    }

    std::thread([this] {
        const int throttle_ms = std::max(0, throttle_ms_.load()); // reuse ini value
        const long long throttle_ns = static_cast<long long>(throttle_ms) * 1'000'000LL;

        while (enabled_.load()) {
            const auto wantSeq = s_seq.load(std::memory_order_relaxed);
            const auto haveSeq = s_seq_handled.load(std::memory_order_relaxed);

            // nothing new → idle briefly
            if (wantSeq == haveSeq) {
                std::this_thread::sleep_for(5ms);
                continue;
            }

            // Throttle window check
            const auto now = now_ns();
            const auto last = s_last_fire_ns.load(std::memory_order_relaxed);
            if (last > 0 && (now - last) < throttle_ns) {
                // sleep the remainder (but not less than a few ms to avoid busy loop)
                const auto remain_ns = throttle_ns - (now - last);
                const auto remain_ms = std::max<int>(1, static_cast<int>(remain_ns / 1'000'000LL));
                std::this_thread::sleep_for(std::chrono::milliseconds(remain_ms));
                continue;
            }

            // Snapshot the “last event wins” sequence we're going to service
            const auto seqToHandle = s_seq.load(std::memory_order_relaxed);

            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([this, seqToHandle] {
                    // If new events arrived since we queued, we still process the latest snapshot (seqToHandle)
                    // RaceMenu-open path: EI nudge (fast). Otherwise: heavy path.
                    apply();

                    s_seq_handled.store(seqToHandle, std::memory_order_relaxed);
                    s_last_fire_ns.store(now_ns(), std::memory_order_relaxed);
                });
            }

            // small yield between polls
            std::this_thread::sleep_for(1ms);
        }

        throttle_scheduled_.store(false);
    }).detach();
}

void morph_updater::apply() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    // If RaceMenu is open, drive the Flash ChangeWeight pipe with nudge+restore.
    if (auto* ui = RE::UI::GetSingleton(); ui) {
        using namespace std::literals;
        if (ui->IsMenuOpen("RaceSex Menu"sv) || ui->IsMenuOpen("RaceMenu"sv)) {

            auto m = ui->GetMenu("RaceSex Menu"sv);
            if (!m) m = ui->GetMenu("RaceMenu"sv);

            if (m && m->uiMovie) {
                if (helpers::racemenu_ei::preset_cooldown_active()) {
                    LOG_DEBUG("[morph_updater] EI suppressed (preset active)");
                    return;
                }

                // Prefer RM’s last seen norm; fallback to base weight.
                double norm = 0.5;
                if (!helpers::racemenu_ei::snapshot_last_weight(norm)) {
                    if (auto* base = player->GetActorBase()) {
                        norm = std::clamp<double>(base->weight / 100.0, 0.0, 1.0);
                    }
                }

                // Two EI calls with fenced arg0 (base+1, base+2)
                const bool ok = helpers::racemenu_ei::nudge_then_restore_norm(m->uiMovie.get(), norm, 0.01);
                LOG_DEBUG("[morph_updater] EI ChangeWeight(nudge±1%) -> {}", ok);
                return;
            }

            LOG_DEBUG("[morph_updater] RaceMenu open but no movie; skip");
            return;
        }
    }

    // Outside RaceMenu: heavy commit (unchanged)
    if (auto* a = player->As<RE::Actor>()) {
        if (skee_bmi_) {
            skee_bmi_->UpdateModelWeight(a, /*immediate=*/true);
        }
        a->DoReset3D(true);
        LOG_DEBUG("[morph_updater] applied morphs (DoReset3D)");
        helpers::ui::notify("Morphs applied");
    }
}
