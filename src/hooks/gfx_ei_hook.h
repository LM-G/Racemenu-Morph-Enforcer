#pragma once
#include "../pch.h"

namespace hooks::gfx_ei {

    // Install a proxy ExternalInterface on this movie.
    // Safe to call multiple times; it will no-op if already installed.
    bool enable(RE::GFxMovieView* mv);

    // Restore original EI (if this hook installed one).
    void disable(RE::GFxMovieView* mv);

} // namespace hooks::gfx_ei
