#pragma once

#include <type_traits>

#include <CrispCore/BitFlags.hpp>

namespace crisp::gui
{
    enum class SizingPolicy
    {
        Fixed,
        FillParent,
        WrapContent
    };

    enum class Anchor
    {
        TopLeft,
        TopCenter,
        TopRight,
        CenterLeft,
        Center,
        CenterRight,
        BottomLeft,
        BottomCenter,
        BottomRight
    };

    enum class Origin
    {
        TopLeft,
        TopCenter,
        TopRight,
        CenterLeft,
        Center,
        CenterRight,
        BottomLeft,
        BottomCenter,
        BottomRight
    };

    enum class Validation
    {
        None      = 0,
        Geometry  = 1,
        Color     = 2,
        All       = Geometry | Color
    };
    DECLARE_BITFLAG(Validation)

    enum class State
    {
        Idle,
        Hover,
        Pressed,
        Disabled,

        Count
    };
}
