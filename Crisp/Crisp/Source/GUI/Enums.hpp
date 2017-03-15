#pragma once

#include <type_traits>

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
            Center,
            CenterVertically,
            CenterHorizontally
        };

        enum class Validation
        {
            None = 0,
            Transform = 1,
            Color = 2,
            All = Transform | Color
        };

        static Validation operator|(Validation f1, Validation f2)
        {
            return static_cast<Validation>(static_cast<std::underlying_type<Validation>::type>(f1) | static_cast<std::underlying_type<Validation>::type>(f2));
        }

        static bool operator&(Validation f1, Validation f2)
        {
            return static_cast<std::underlying_type<Validation>::type>(f1) & static_cast<std::underlying_type<Validation>::type>(f2);
        }
    }
}