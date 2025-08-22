#pragma once
#include "pch.h"

namespace MorphFixer {
    namespace Helpers::RaceMenuExternalInterface {

        // Feed every ExternalInterface call from EI proxy.
        void observe(const char* name, const RE::GFxValue* args, std::uint32_t argc);

        // True while a preset operation is in-flight / just finished.
        bool presetCooldownActive();

        // Record the last real ChangeWeight(args) from RM.
        void recordChangeWeightArguments(const RE::GFxValue* args, std::uint32_t argc);

        // Get last seen normalized weight [0..1] from snapshot if we have one.
        bool snapshotLastWeight(double& outNorm);

        // Drive ChangeWeight with normalized [0..1] via EI (arg0 auto-managed).
        bool driveChangeWeightNorm(RE::GFxMovieView* mv, double normalized);

        // --- New overloads for precise arg0 control -----------------------------
        // Drive ChangeWeight with normalized value and an explicit arg0.
        bool driveChangeWeightNormWithArg0(RE::GFxMovieView* mv, double normalized, double arg0);

        // Nudge then restore around 'normalized', using two sequential arg0s.
        // We "fence" one slot (base = nextArg0()+1) to avoid collisions with the
        // next native EI call in the same frame.
        bool nudgeThenRestoreNorm(RE::GFxMovieView* mv, double normalized, double epsilon = 0.01);

    }  // namespace helpers::racemenu_ei
}