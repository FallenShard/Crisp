#pragma once

#include <Crisp/Utils/BitFlags.hpp>

namespace crisp {
namespace gui {

enum class SizingPolicy {
    Fixed,
    FillParent,
    WrapContent,
};

enum class Anchor {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
};

enum class Origin {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
};

enum class Validation {
    None = 0,
    Geometry = 1,
    Color = 2,
    All = Geometry | Color,
};

enum class State {
    Idle,
    Hover,
    Pressed,
    Disabled,

    Count,
};
} // namespace gui

DECLARE_BITFLAG_IN_NAMESPACE(gui, Validation)
} // namespace crisp
