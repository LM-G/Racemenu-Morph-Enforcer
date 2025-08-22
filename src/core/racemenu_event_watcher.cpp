#include "core/racemenu_event_watcher.h"

#include "features/morph_updater.h"
#include "logger.h"
#include "pch.h"

namespace MorphFixer {
    using namespace std::literals;

    RaceMenuEventWatcher& RaceMenuEventWatcher::get() {
        static RaceMenuEventWatcher inst;
        return inst;
    }

    void RaceMenuEventWatcher::start_listening() {
        if (auto* src = SKSE::GetModCallbackEventSource()) {
            src->AddEventSink(this);
            LOG_INFO("[RaceMenuEventWatcher] listening for RaceMenu mod events");
        } else {
            LOG_WARN("[RaceMenuEventWatcher] GetModCallbackEventSource() returned null");
        }
    }

    void RaceMenuEventWatcher::stop_listening() {
        if (auto* src = SKSE::GetModCallbackEventSource()) {
            src->RemoveEventSink(this);
            LOG_INFO("[RaceMenuEventWatcher] stopped listening");
        }
    }

    bool RaceMenuEventWatcher::is_racemenu_slider_event(const std::string_view name) {
        // RaceMenu historically fires various mod events; we accept a few common ones
        // and anything that clearly mentions "Slider".
        // Known-ish strings people use/see in the wild:
        // - "NiOverrideUpdateBodyMorph"
        // - "SkeeSliderChanged" / "SliderChanged"
        // - "RaceMenuSliderChange"
        // We keep this loose so it still works across RaceMenu versions.
        if (name.empty()) return false;

        // Known event names used by RaceMenu & friends across versions:
        // - "RSM_SliderChange" (what you observed)
        // - "RM_OnSliderChange"
        // - "RaceMenuSliderChanged"
        // - "OBody_SetMorph" (morph triggers)
        // - "TNGAroused_SetMorph" (morph triggers)
        // Also catch anything containing "SliderChange"/"Slider"
        if (name == "RSM_SliderChange"sv || name == "RM_OnSliderChange"sv || name == "RaceMenuSliderChanged"sv ||
            name == "OBody_SetMorph"sv || name == "TNGAroused_SetMorph"sv) {
            return true;
        }

        return name.find("SliderChange"sv) != std::string_view::npos || name.find("Slider"sv) != std::string_view::npos;
    }

    RE::BSEventNotifyControl RaceMenuEventWatcher::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
                                                                RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        const auto& nameBS = a_event->eventName;  // BSFixedString

        // If CommonLibSSE has .empty():
        if (nameBS.empty()) return RE::BSEventNotifyControl::kContinue;

        // Convert to narrow string_view
        std::string_view name{nameBS.c_str()};  // or nameBS.data()

        if (is_racemenu_slider_event(name)) {
            LOG_DEBUG("[RaceMenuEventWatcher] slider event notify -> {}", name);
            // MorphUpdater::get().notify_slider_changed();
        }

        return RE::BSEventNotifyControl::kContinue;
    }
}  // namespace MorphFixer
