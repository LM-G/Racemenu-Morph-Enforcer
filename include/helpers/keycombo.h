#pragma once

struct KeyCombo {
    // DIK of the main key to trigger on (e.g., 156 for NUMPADENTER)
    int main{0};

    // Required modifiers (exact side when set)
    bool reqLeftAlt{false};
    bool reqRightAlt{false};
    bool reqLeftCtrl{false};
    bool reqRightCtrl{false};
    bool reqLeftShift{false};
    bool reqRightShift{false};

    [[nodiscard]] bool empty() const noexcept { return main == 0; }
};