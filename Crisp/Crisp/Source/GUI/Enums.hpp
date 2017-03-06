#pragma once

namespace crisp
{
    namespace gui
    {
        enum class Sizing
        {
            Fixed,
            FillParent,
            WrapContent
        };

        enum class Anchor
        {
            TopLeft,
            BottomLeft,
            TopRight,
            BottomRight,
            TopCenter,
            LeftCenter,
            BottomCenter,
            RightCenter,
            Center
        };
    }
}