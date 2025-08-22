#include "gfx_ei_hook.h"

#include "../logger.h"
#include "../helpers/racemenu_ei_driver.h"
#include "../morph_updater.h"

#include "RE/G/GFxExternalInterface.h"
#include "RE/G/GFxMovieView.h"
#include "RE/G/GFxState.h"
#include "RE/G/GFxStateBag.h"
#include "RE/G/GFxValue.h"

namespace {

class TracingExternalInterface : public RE::GFxExternalInterface
{
public:
    TracingExternalInterface(RE::GFxExternalInterface* orig) : orig_(orig)
    {
        if (orig_) {
            orig_->AddRef();
        }
    }

    ~TracingExternalInterface() override
    {
        if (orig_) {
            // orig_->Release();
            orig_ = nullptr;
        }
    }

    void Callback(RE::GFxMovieView* movie, const char* name, const RE::GFxValue* args, std::uint32_t argc) override
    {
        // Observe/log first for visibility
        helpers::racemenu_ei::observe(name, args, argc);

        // (Optional) lightweight printf of first args
        if (name) {
            auto arg0 = (argc >= 1 && args[0].IsNumber()) ? args[0].GetNumber() : 0.0;
            auto arg1 = (argc >= 2 && args[1].IsNumber()) ? args[1].GetNumber() : 0.0;
            auto arg2 = (argc >= 3 && args[2].IsNumber()) ? args[2].GetNumber() : 0.0;
            LOG_DEBUG("[gfx-ei] {}(argc={}) [0]=num:{:.3f} [1]=num:{:.3f} [2]=num:{:.3f}",
                      name, argc, arg0, arg1, arg2);
        }

        // Let RaceMenu handle its event first (safer ordering)
        if (orig_) {
            orig_->Callback(movie, name, args, argc);
        }

        // Route EI to our morph cadence AFTER original handling.
        morph_updater::get().onGfxEvent(name, args, argc);
    }

    RE::GFxExternalInterface* orig_{nullptr};
};

// Single-movie installation (RaceMenu)
static TracingExternalInterface* s_proxy{nullptr};
static RE::GFxMovieView*         s_movie{nullptr};

static RE::GFxExternalInterface* get_ei_addref(RE::GFxMovieView* mv)
{
    if (!mv) return nullptr;
    auto* st = mv->GetStateAddRef(RE::GFxState::StateType::kExternalInterface);
    return static_cast<RE::GFxExternalInterface*>(st);
}

} // namespace

namespace hooks::gfx_ei {

bool enable(RE::GFxMovieView* mv)
{
    if (!mv) return false;
    if (s_movie == mv && s_proxy) {
        // already installed
        return true;
    }

    auto* ei = get_ei_addref(mv);
    if (!ei) {
        LOG_WARN("[gfx-ei] movie has no EI");
        return false;
    }

    // Wrap the existing EI with our tracing proxy
    s_proxy = new TracingExternalInterface(ei);

    // Point the movie at our proxy
    mv->SetState(RE::GFxState::StateType::kExternalInterface, s_proxy);
    s_movie = mv;

    LOG_INFO("[gfx-ei] installed proxy EI for movie {} (orig {})", fmt::ptr(mv), fmt::ptr(ei));
    LOG_INFO("[gfx-ei] enabled");
    return true;
}

void disable(RE::GFxMovieView* mv)
{
    if (!mv || !s_proxy) return;

    // Restore original EI (proxy holds the ref)
    mv->SetState(RE::GFxState::StateType::kExternalInterface, s_proxy->orig_);

    // Drop our proxy (will Release original)
    delete s_proxy;
    s_proxy = nullptr;
    s_movie = nullptr;

    LOG_INFO("[gfx-ei] disabled");
}

} // namespace hooks::gfx_ei
