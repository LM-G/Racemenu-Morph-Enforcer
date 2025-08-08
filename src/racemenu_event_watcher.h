#pragma once

#include <string_view>

#include "RE/B/BSTEvent.h"  // RE::BSTEventSink

namespace SKSE {
    struct ModCallbackEvent;
    class ModCallbackEventSource;
}

class racemenu_event_watcher : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
public:
    static racemenu_event_watcher& get();

    racemenu_event_watcher(const racemenu_event_watcher&) = delete;
    racemenu_event_watcher& operator=(const racemenu_event_watcher&) = delete;

    // Call once when DataLoaded to start receiving RaceMenu mod events
    void start_listening();
    void stop_listening();

    // BSTEventSink override
    RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
                                          RE::BSTEventSource<SKSE::ModCallbackEvent>* a_eventSource) override;

private:
    racemenu_event_watcher() = default;

    static bool is_racemenu_slider_event(std::string_view name);
};