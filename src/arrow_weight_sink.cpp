#include "arrow_weight_sink.h"

#include "helpers/ui.h"
#include "logger.h"
#include "morph_updater.h"
#include "pch.h"

ArrowWeightSink& ArrowWeightSink::get()
{
    static ArrowWeightSink s;
    return s;
}

RE::BSEventNotifyControl ArrowWeightSink::ProcessEvent(RE::InputEvent* const* a_events,
                                                       RE::BSTEventSource<RE::InputEvent*>*)
{
    if (!a_events) {
        return RE::BSEventNotifyControl::kContinue;
    }

    for (auto e = *a_events; e; e = e->next) {
        auto* btn = e->AsButtonEvent();
        if (!btn) continue;

        if (btn->GetDevice() != RE::INPUT_DEVICE::kKeyboard) continue;
        if (!btn->IsDown()) continue;  // only on press edge

        const auto code = static_cast<unsigned>(btn->GetIDCode());

        float target = -1.0f;
        if (code == 0xC8) {           // DIK_UP
            target = 100.0f;
        } else if (code == 0xD0) {    // DIK_DOWN
            target = 0.0f;
        } else {
            continue;
        }

        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([target] {
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!player) return;
                auto* base = player->GetActorBase();
                if (!base) return;

                const float prev = base->weight;
                base->weight = std::clamp(target, 0.0f, 100.0f);

                // Your existing refresh path
                morph_updater::get().updateModelWeight(player);

                //helpers::refresh::force_weight_refresh_native(player);

                LOG_INFO("[keybind] Arrow key set weight: {:.2f} -> {:.2f}", prev, target);
                helpers::ui::notify(fmt::format("[KEY] weight {:.0f}", target));
            });
        }
    }

    return RE::BSEventNotifyControl::kContinue;
}
