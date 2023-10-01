#pragma once

#include <Crisp/Core/Keyboard.hpp>

namespace crisp {
enum class MouseButton {
    Unknown = -1,
    Left = 0,
    Right = 1,
    Middle = 2,
};

struct MouseEventArgs {
    MouseButton button{MouseButton::Unknown};
    ModifierFlags modifiers;
    double x{0.0};
    double y{0.0};
};
} // namespace crisp