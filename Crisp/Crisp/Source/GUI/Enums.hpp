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
        TopRight,
        BottomLeft,
        BottomRight,
        Center,
        CenterTop,
        CenterBottom,
        CenterLeft,
        CenterRight
    };

    enum class Validation
    {
        None      = 0,
        Geometry  = 1,
        Color     = 2,
        All       = Geometry | Color
    };
    DECLARE_BITFLAG(Validation)
}
