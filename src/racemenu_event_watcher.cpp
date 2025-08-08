#include "racemenu_event_watcher.h"

#include <string_view>

#include "RE/B/BSFixedString.h"
#include "logger.h"
#include "morph_updater.h"
#include "pch.h"

using namespace std::literals;

racemenu_event_watcher& racemenu_event_watcher::get() {
    static racemenu_event_watcher inst;
    return inst;
}

void racemenu_event_watcher::start_listening() {
    if (auto* src = SKSE::GetModCallbackEventSource()) {
        src->AddEventSink(this);
        LOG_INFO("[racemenu_event_watcher] listening for RaceMenu mod events");
    } else {
        LOG_WARN("[racemenu_event_watcher] GetModCallbackEventSource() returned null");
    }
}

void racemenu_event_watcher::stop_listening() {
    if (auto* src = SKSE::GetModCallbackEventSource()) {
        src->RemoveEventSink(this);
        LOG_INFO("[racemenu_event_watcher] stopped listening");
    }
}

bool racemenu_event_watcher::is_racemenu_slider_event(std::string_view name) {
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
    if (name == "RSM_SliderChange"sv ||
        name == "RM_OnSliderChange"sv ||
        name == "RaceMenuSliderChanged"sv ||
        name == "OBody_SetMorph"sv ||
        name == "TNGAroused_SetMorph"sv) {
        return true;
        }

    return name.find("SliderChange"sv) != std::string_view::npos ||
           name.find("Slider"sv)       != std::string_view::npos;
}

RE::BSEventNotifyControl racemenu_event_watcher::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
                                                              RE::BSTEventSource<SKSE::ModCallbackEvent>*) {

    if (!a_event) return RE::BSEventNotifyControl::kContinue;

    const auto& nameBS = a_event->eventName;  // BSFixedString

    // If your CommonLibSSE has .empty():
    if (nameBS.empty()) return RE::BSEventNotifyControl::kContinue;

    // Convert to narrow string_view
    std::string_view name{nameBS.c_str()};  // or nameBS.data()

    if (is_racemenu_slider_event(name)) {
        LOG_DEBUG("[racemenu_event_watcher] slider event notify -> {}", name);
        morph_updater::get().notify_slider_changed();
    }

    return RE::BSEventNotifyControl::kContinue;
}
