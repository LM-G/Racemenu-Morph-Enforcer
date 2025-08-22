#pragma once
#include <string>
#include <string_view>

#include "keycombo.h"

namespace RE {
    class BSKeyboardDevice;
}

namespace MorphFixer {
     namespace Helpers::Keybind {

        // Parse a chord from text.
        // Accepts:
        //   - names: "rightalt + numpadenter", "leftctrl + rightshift + f5"
        //   - numbers: "184,156" or "0xB8 + 0x9C" (DIK scancodes)
        // Mixed tokens are fine. Up to 2 modifiers + 1 main (last non-mod becomes main).
        KeyCombo parse(std::string_view s);

        // Match an input event (keyboard ID code) against a KeyCombo,
        // querying modifier state from the keyboard device.
        bool match(const KeyCombo& kc, unsigned idCode, RE::BSKeyboardDevice* kb);

        // For logs/debugging (e.g., "RAlt+NumPadEnter").
        std::string toString(const KeyCombo& kc);

    }  // namespace helpers::keybind
}