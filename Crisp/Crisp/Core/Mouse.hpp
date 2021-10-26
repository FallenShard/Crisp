#pragma once

#include "Keyboard.hpp"

namespace crisp
{
    enum class MouseButton
    {
        Unknown = -1,
        Left    = 0,
        Right   = 1,
        Middle  = 2
    };

    struct MouseEventArgs
    {
        MouseButton button;
        ModifierFlags modifiers;
        double x;
        double y;
    };
}