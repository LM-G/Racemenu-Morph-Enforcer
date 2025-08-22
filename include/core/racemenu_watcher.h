#pragma once

#include "RE/M/MenuOpenCloseEvent.h"
#include "pch.h"

namespace MorphFixer {

    class RaceMenuWatcher : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    public:
        static RaceMenuWatcher& get();
        RaceMenuWatcher(const RaceMenuWatcher&) = delete;
        RaceMenuWatcher& operator=(const RaceMenuWatcher&) = delete;

        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* evn,
                                              RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

    private:
        RaceMenuWatcher() = default;
    };

}  // namespace MorphFixer
