#include "racemenu_ei_driver.h"

#include "../logger.h"

#include "RE/G/GFxExternalInterface.h"
#include "RE/G/GFxMovieView.h"
#include "RE/G/GFxState.h"
#include "RE/G/GFxStateBag.h"
#include "RE/G/GFxValue.h"

#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <cmath>
#include <string>

namespace {

using clock_ns = std::chrono::steady_clock;
static inline long long now_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        clock_ns::now().time_since_epoch()).count();
}

struct ArgCopy {
    RE::GFxValue::ValueType t{ RE::GFxValue::ValueType::kUndefined };
    double      num{0.0};
    bool        b{false};
    std::string s;

    void toValue(RE::GFxValue& out) const {
        switch (t) {
        case RE::GFxValue::ValueType::kNumber:  out.SetNumber(num); break;
        case RE::GFxValue::ValueType::kBoolean: out.SetBoolean(b);     break;
        case RE::GFxValue::ValueType::kString:  out.SetString(s.c_str()); break;
        default:                                 out.SetUndefined(); break;
        }
    }
    static ArgCopy from(const RE::GFxValue& v) {
        ArgCopy c; c.t = v.GetType();
        if (v.IsNumber())                                          c.num = v.GetNumber();
        else if (v.GetType() == RE::GFxValue::ValueType::kBoolean) c.b   = v.GetBool();
        else if (v.IsString())                                     c.s   = v.GetString();
        return c;
    }
};

struct Snapshot {
    std::uint32_t        argc{0};
    std::vector<ArgCopy> args;      // arg[1] is normalized weight
};

static std::mutex        s_mu;
static Snapshot          s_snap;
static std::atomic_bool  s_have{false};

// Most recent arg0 seen on ANY EI call
static std::atomic<double> s_last_any_arg0{0.0};
static std::atomic_bool    s_have_any_arg0{false};

// Most recent arg0 seen specifically on ChangeWeight
static std::atomic<double> s_last_cw_arg0{0.0};
static std::atomic_bool    s_have_cw_arg0{false};

// Preset cooldown (ns since epoch)
static std::atomic<long long> s_preset_until_ns{0};

static RE::GFxExternalInterface* get_ei_addref(RE::GFxMovieView* mv)
{
    if (!mv) return nullptr;
    auto* st = mv->GetStateAddRef(RE::GFxState::StateType::kExternalInterface);
    return reinterpret_cast<RE::GFxExternalInterface*>(st);
}

static inline bool contains_case_insensitive(const char* hay, const char* needle)
{
    if (!hay || !needle) return false;
    const auto nlen = std::strlen(needle);
    for (const char* p = hay; *p; ++p) {
        if (_strnicmp(p, needle, nlen) == 0) return true;
    }
    return false;
}

// Next arg0 to use: prefer ANY->+1, else CW->+1, else 0.
static inline double next_arg0()
{
    if (s_have_any_arg0.load(std::memory_order_relaxed)) {
        const auto last = s_last_any_arg0.load(std::memory_order_relaxed);
        const long long i = static_cast<long long>(std::llround(last));
        return static_cast<double>(i + 1);
    }
    if (s_have_cw_arg0.load(std::memory_order_relaxed)) {
        const auto last = s_last_cw_arg0.load(std::memory_order_relaxed);
        const long long i = static_cast<long long>(std::llround(last));
        return static_cast<double>(i + 1);
    }
    return 0.0;
}

} // namespace

namespace helpers::racemenu_ei {

void record_changeweight_args(const RE::GFxValue* args, std::uint32_t argc)
{
    if (!args || argc == 0) return;

    std::lock_guard lk(s_mu);

    s_snap.argc = argc;
    s_snap.args.resize(argc);
    for (std::uint32_t i = 0; i < argc; ++i)
        s_snap.args[i] = ArgCopy::from(args[i]);

    s_have.store(true, std::memory_order_relaxed);

    if (argc >= 1 && args[0].IsNumber()) {
        const double a0 = args[0].GetNumber();
        s_last_any_arg0.store(a0, std::memory_order_relaxed);
        s_have_any_arg0.store(true, std::memory_order_relaxed);
        s_last_cw_arg0.store(a0, std::memory_order_relaxed);
        s_have_cw_arg0.store(true, std::memory_order_relaxed);
    }
}

bool snapshot_last_weight(double& outNorm)
{
    std::lock_guard lk(s_mu);
    if (!s_have.load(std::memory_order_relaxed) || s_snap.argc < 2)
        return false;
    if (s_snap.args[1].t != RE::GFxValue::ValueType::kNumber)
        return false;
    outNorm = std::clamp(s_snap.args[1].num, 0.0, 1.0);
    return true;
}

bool drive_changeweight_norm_with_arg0(RE::GFxMovieView* mv, double normalized, double arg0)
{
    if (!mv) return false;

    std::vector<RE::GFxValue> callArgs;
    {
        std::lock_guard lk(s_mu);
        if (s_have.load(std::memory_order_relaxed)) {
            callArgs.resize(s_snap.argc);
            for (std::uint32_t i = 0; i < s_snap.argc; ++i)
                s_snap.args[i].toValue(callArgs[i]);

            if (s_snap.argc > 1) {
                callArgs[1].SetNumber(std::clamp(normalized, 0.0, 1.0));
            } else {
                callArgs.resize(3);
                callArgs[1].SetNumber(std::clamp(normalized, 0.0, 1.0));
                callArgs[2].SetNumber(2.0);
            }
        } else {
            callArgs.resize(3);
            callArgs[1].SetNumber(std::clamp(normalized, 0.0, 1.0));
            callArgs[2].SetNumber(2.0);
        }

        if (callArgs.empty()) callArgs.resize(1);
        callArgs[0].SetNumber(arg0);

        // Stamp trackers with what we're sending
        s_last_any_arg0.store(arg0, std::memory_order_relaxed);
        s_have_any_arg0.store(true, std::memory_order_relaxed);
        s_last_cw_arg0.store(arg0, std::memory_order_relaxed);
        s_have_cw_arg0.store(true, std::memory_order_relaxed);
    }

    auto* ei = get_ei_addref(mv);
    if (!ei) return false;

    ei->Callback(mv, "ChangeWeight", callArgs.data(), static_cast<std::uint32_t>(callArgs.size()));
    ei->Release();
    return true;
}

bool drive_changeweight_norm(RE::GFxMovieView* mv, double normalized)
{
    const double a0 = next_arg0();
    return drive_changeweight_norm_with_arg0(mv, normalized, a0);
}

bool nudge_then_restore_norm(RE::GFxMovieView* mv, double normalized, double epsilon)
{
    if (!mv) return false;

    const double eps  = std::clamp(epsilon, 0.001, 0.05);
    const double norm = std::clamp(normalized, 0.0, 1.0);
    const double nudged = (norm - eps >= 0.0) ? (norm - eps) : std::min(1.0, norm + eps);

    // Fence one slot to avoid colliding with the *very next* native EI call.
    const double base = next_arg0() + 1.0;

    const bool a = drive_changeweight_norm_with_arg0(mv, nudged, base);
    const bool b = drive_changeweight_norm_with_arg0(mv, norm,   base + 1.0);

    LOG_DEBUG("[RMF] EI nudge+restore: norm={:.3f} -> {:.3f} -> {:.3f} (arg0 {:.0f},{:.0f}) ok={} {}",
              norm, nudged, norm, base, base + 1.0, a, b);
    return a && b;
}

bool preset_cooldown_active()
{
    const auto now = now_ns();
    const auto until = s_preset_until_ns.load(std::memory_order_relaxed);
    return until > now;
}

void observe(const char* name, const RE::GFxValue* args, std::uint32_t argc)
{
    if (!name) return;

    // Track arg0 from ANY EI call if numeric
    if (argc >= 1 && args && args[0].IsNumber()) {
        s_last_any_arg0.store(args[0].GetNumber(), std::memory_order_relaxed);
        s_have_any_arg0.store(true, std::memory_order_relaxed);
    }

    if (_stricmp(name, "ChangeWeight") == 0) {
        record_changeweight_args(args, argc);
        return;
    }

    // Any preset-related EI call extends a short cooldown window
    if (contains_case_insensitive(name, "Preset")) {
        constexpr auto ttl = std::chrono::milliseconds(1500);
        const auto until = now_ns() + std::chrono::duration_cast<std::chrono::nanoseconds>(ttl).count();
        s_preset_until_ns.store(until, std::memory_order_relaxed);
        LOG_DEBUG("[RMF] preset cooldown armed ({} ms)", static_cast<int>(ttl.count()));
    }
}

} // namespace helpers::racemenu_ei
