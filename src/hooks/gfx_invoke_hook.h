// hooks/gfx_invoke_hook.h
#pragma once
#include "../pch.h"

namespace hooks::gfx_invoke {

    void install_for_movie(RE::GFxMovieView* mv);  // patch THIS movie's vtable
    void set_enabled(bool on);                     // just gate logging

} // namespace hooks::gfx_invoke
