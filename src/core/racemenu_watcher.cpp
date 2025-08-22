#include "core/racemenu_watcher.h"

#include "core/gfx_ei_hook.h"
#include "features/morph_updater.h"
#include "helpers/ui.h"
#include "logger.h"

namespace MorphFixer {
    using namespace std::literals;

    RaceMenuWatcher& RaceMenuWatcher::get() {
        static RaceMenuWatcher instance;
        return instance;
    }

    RE::BSEventNotifyControl RaceMenuWatcher::ProcessEvent(const RE::MenuOpenCloseEvent* evn,
                                                           RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
        if (!evn) {
            return RE::BSEventNotifyControl::kContinue;
        }

        const auto& name = evn->menuName;
        if (const bool isRM = (name == "RaceSex Menu"sv) || (name == "RaceMenu"sv); !isRM) {
            return RE::BSEventNotifyControl::kContinue;
        }

        const bool opening = evn->opening;

        MorphUpdater::get().setEnabled(opening);

        // Install EI proxy on the RaceMenu movie
        if (auto ui = RE::UI::GetSingleton(); ui) {
            if (auto m = ui->GetMenu(name)) {
                if (auto* mv = m->uiMovie.get()) {
                    if (opening) {
                        LOG_DEBUG("[RaceMenuWatcher] RaceMenu opened -> MorphUpdater enabled");
                        Hooks::GfxExternalInterface::enable(mv);
                    } else {
                        LOG_DEBUG("[RaceMenuWatcher] RaceMenu closed -> MorphUpdater disabled");
                        Hooks::GfxExternalInterface::disable(mv);
                        MorphUpdater::get().onMenuClosed();
                    }
                }
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }
}  // namespace MorphFixer
