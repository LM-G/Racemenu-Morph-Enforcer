#pragma once

#include "RE/M/MenuOpenCloseEvent.h"
#include "pch.h"

class racemenu_watcher : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    static racemenu_watcher& get();
    racemenu_watcher(const racemenu_watcher&) = delete;
    racemenu_watcher& operator=(const racemenu_watcher&) = delete;

    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* evn,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

private:
    racemenu_watcher() = default;
};
