#include "gfx_ei_hook.h"

#include "../logger.h"
#include "../helpers/racemenu_ei_driver.h"

#include "RE/G/GFxExternalInterface.h"
#include "RE/G/GFxMovieView.h"
#include "RE/G/GFxState.h"
#include "RE/G/GFxStateBag.h"
#include "RE/G/GFxValue.h"

namespace {

class TracingExternalInterface : public RE::GFxExternalInterface
{
public:
    explicit TracingExternalInterface(RE::GFxExternalInterface* orig)
        : orig_(orig)
    {
        // We keep a reference to the original EI while installed.
        if (orig_) {
            orig_->AddRef();
        }
    }

    ~TracingExternalInterface() override
    {
        if (orig_) {
            orig_->Release();
            orig_ = nullptr;
        }
    }

    void Callback(RE::GFxMovieView* movie, const char* name, const RE::GFxValue* args, std::uint32_t argc) override
    {
        // Mirror every EI call into our observer
        helpers::racemenu_ei::observe(name, args, argc);

        // (Optional) lightweight log of a few interesting calls
        if (name) {
            if (_stricmp(name, "ChangeWeight") == 0 || _strnicmp(name, "ChangePart", 10) == 0 || _stricmp(name, "PlaySound") == 0) {
                // Try to print first 2 args compactly
                auto arg0 = (argc >= 1 && args[0].IsNumber()) ? args[0].GetNumber() : 0.0;
                auto arg1 = (argc >= 2 && args[1].IsNumber()) ? args[1].GetNumber() : 0.0;
                LOG_DEBUG("[gfx-ei] {}(argc={}) [0]=num:{:.3f} [1]=num:{:.3f}",
                          name, argc, arg0, arg1);
            }
        }

        // Forward to original EI if present
        if (orig_) {
            orig_->Callback(movie, name, args, argc);
        }
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
    return reinterpret_cast<RE::GFxExternalInterface*>(st);
}

} // namespace

namespace hooks::gfx_ei {

bool enable(RE::GFxMovieView* mv)
{
    if (!mv) return false;

    // Already installed for this movie?
    if (s_proxy && s_movie == mv) {
        return true;
    }

    // If installed for another movie, clean it up
    if (s_proxy && s_movie && s_movie != mv) {
        disable(s_movie);
    }

    auto* orig = get_ei_addref(mv); // addref'd
    if (!orig) {
        LOG_WARN("[gfx-ei] no ExternalInterface on movie {}", fmt::ptr(mv));
        return false;
    }

    s_proxy = new TracingExternalInterface(orig);
    s_movie = mv;

    // Install our proxy. SetState takes ownership of the ref-counted pointer.
    mv->SetState(RE::GFxState::StateType::kExternalInterface, s_proxy);

    // Release our local addref to the original (proxy holds its own ref)
    orig->Release();

    LOG_INFO("[gfx-ei] installed proxy EI for movie {} (orig {})", fmt::ptr(mv), fmt::ptr(s_proxy->orig_));
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
