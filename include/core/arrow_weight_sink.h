#pragma once
#include "pch.h"

namespace MorphFixer {

    // Separate input sink that maps Arrow Up/Down to weight 100/0.
    // Registered from helpers::keybind::install_weight_test_binds().
    class ArrowWeightSink final : public RE::BSTEventSink<RE::InputEvent*> {
    public:
        static ArrowWeightSink& get();

        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events,
                                              RE::BSTEventSource<RE::InputEvent*>*) override;

    private:
        ArrowWeightSink() = default;
    };

}  // namespace MorphFixer
