#include "gfx_invoke_hook.h"
#include "../logger.h"
#include <atomic>

namespace {
    using Invoke_t = bool(RE::GFxMovieView*, const char*, RE::GFxValue*, const RE::GFxValue*, std::uint32_t);

    static Invoke_t*        s_orig     = nullptr;
    static std::atomic_bool s_enabled{false};
    static std::atomic_bool s_patched{false};

    // Candidate vtable index for AE/SE; if the guard below rejects it,
    // bump +/- 1 and re-run until it accepts.
    constexpr std::size_t INVOKE_VTBL_INDEX = 0x2F;

    bool thunk(RE::GFxMovieView* mv, const char* name, RE::GFxValue* ret,
               const RE::GFxValue* args, std::uint32_t numArgs)
    {
        if (s_enabled.load()) {
            if (name) {
                if (numArgs >= 1 && args && args[0].IsNumber()) {
                    LOG_DEBUG("[gfx] Invoke: {}(arg0={:.3f})", name, args[0].GetNumber());
                } else {
                    LOG_DEBUG("[gfx] Invoke: {}(args={})", name, numArgs);
                }
            }
        }
        return s_orig(mv, name, ret, args, numArgs);
    }

    template <class T>
    void write_vfunc(std::uintptr_t* vtbl, std::size_t index, T* replacement, T** outOrig)
    {
        auto& slot = vtbl[index];                           // slot: uintptr_t
        *outOrig   = reinterpret_cast<T*>(slot);            // save original
        REL::safe_write(reinterpret_cast<std::uintptr_t>(&slot),
                        reinterpret_cast<std::uintptr_t>(replacement)); // patch
    }

    // NG 3.7.0: use Module::segment(Segment::textx/textw)
    // Accept either executable text (.textx) or writable text (.textw)
    bool addr_in_text(std::uintptr_t addr)
    {
        const auto& mod = REL::Module::get();
        const auto textx = mod.segment(REL::Segment::textx);
        const auto textw = mod.segment(REL::Segment::textw);

        const auto in_x = addr >= textx.address() && addr < (textx.address() + textx.size());
        const auto in_w = addr >= textw.address() && addr < (textw.address() + textw.size());
        return in_x || in_w;
    }
} // namespace

namespace hooks::gfx_invoke {

void install_for_movie(RE::GFxMovieView* mv)
{
    if (!mv || s_patched.load()) return;

    // IMPORTANT: triple indirection to get the vtbl (mv -> *vtbl -> slots)
    auto* vtbl = *reinterpret_cast<std::uintptr_t**>(mv);

    // sanity-check the candidate slot before patching
    const std::uintptr_t candidate = vtbl[INVOKE_VTBL_INDEX];
    if (!addr_in_text(candidate)) {
        LOG_ERROR("[gfx] Refusing to patch: vtbl[{}]={:p} not in .text(.x/.w). "
                  "Adjust INVOKE_VTBL_INDEX for your runtime.", INVOKE_VTBL_INDEX, (void*)candidate);
        return;
    }

    write_vfunc<Invoke_t>(vtbl, INVOKE_VTBL_INDEX, thunk, &s_orig);
    s_patched.store(true);
    s_enabled.store(false);
    LOG_INFO("[gfx] Patched GFxMovieView::Invoke at vtbl[{}] (orig={:p})",
             INVOKE_VTBL_INDEX, (void*)s_orig);
}

void set_enabled(bool on)
{
    s_enabled.store(on);
    LOG_INFO("[gfx] Invoke hook {}", on ? "enabled" : "disabled");
}

} // namespace hooks::gfx_invoke
