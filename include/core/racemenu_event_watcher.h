#pragma once

#include <string_view>

#include "RE/B/BSTEvent.h"  // RE::BSTEventSink

namespace SKSE {
    struct ModCallbackEvent;
    class ModCallbackEventSource;
}

namespace MorphFixer {

    class RaceMenuEventWatcher : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
    public:
        static RaceMenuEventWatcher& get();

        RaceMenuEventWatcher(const RaceMenuEventWatcher&) = delete;
        RaceMenuEventWatcher& operator=(const RaceMenuEventWatcher&) = delete;

        // Call once when DataLoaded to start receiving RaceMenu mod events
        void start_listening();
        void stop_listening();

        // BSTEventSink override
        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
                                              RE::BSTEventSource<SKSE::ModCallbackEvent>* a_eventSource) override;

    private:
        RaceMenuEventWatcher() = default;

        static bool is_racemenu_slider_event(std::string_view name);
    };
}  // namespace MorphFixer
