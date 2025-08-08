#include "racemenu_watcher.h"

#include "RE/U/UI.h"
#include "helpers/ui.h"
#include "hooks/gfx_ei_hook.h"
#include "logger.h"
#include "morph_updater.h"

using namespace std::literals;

racemenu_watcher& racemenu_watcher::get() {
    static racemenu_watcher inst;
    return inst;
}

RE::BSEventNotifyControl racemenu_watcher::ProcessEvent(
    const RE::MenuOpenCloseEvent* evn,
    RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
    if (!evn) {
        return RE::BSEventNotifyControl::kContinue;
    }

    const auto& name = evn->menuName;
    const bool isRM = (name == "RaceSex Menu"sv) || (name == "RaceMenu"sv);
    if (!isRM) {
        return RE::BSEventNotifyControl::kContinue;
    }

    const bool opening = evn->opening;

    morph_updater::get().set_enabled(opening);
    if (opening) {
        LOG_DEBUG("[racemenu_watcher] RaceMenu opened -> morph_updater enabled");

        // Install EI proxy on the RaceMenu movie
        if (auto ui = RE::UI::GetSingleton(); ui) {
            if (auto m = ui->GetMenu(name)) {
                if (auto* mv = m->uiMovie.get()) {
                    if (hooks::gfx_ei::enable(mv)) {
                        LOG_INFO("[gfx-ei] enabled");
                    }
                }
            }
        }
    } else {
        LOG_DEBUG("[racemenu_watcher] RaceMenu closed -> morph_updater disabled");

        if (auto ui = RE::UI::GetSingleton(); ui) {
            if (auto m = ui->GetMenu(name)) {
                if (auto* mv = m->uiMovie.get()) {
                    hooks::gfx_ei::disable(mv);
                    LOG_INFO("[gfx-ei] disabled");
                }
            }
        }
    }

    return RE::BSEventNotifyControl::kContinue;
}
