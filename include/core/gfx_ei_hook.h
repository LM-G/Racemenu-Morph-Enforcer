#pragma once
#include "pch.h"

namespace MorphFixer {
    namespace Hooks::GfxExternalInterface {

        // Install a proxy ExternalInterface on this movie.
        // Safe to call multiple times; it will no-op if already installed.
        bool enable(RE::GFxMovieView* mv);

        // Restore original EI (if this hook installed one).
        void disable(RE::GFxMovieView* mv);

    }  // namespace hooks::gfx_ei
}  // namespace MorphFixer
