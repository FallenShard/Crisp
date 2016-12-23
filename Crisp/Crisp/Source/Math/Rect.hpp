#pragma once

#include "Math/Headers.hpp"

namespace crisp
{
    template <typename T>
    struct Rect
    {
        T x;
        T y;
        T width;
        T height;

        template <typename U>
        bool contains(U x, U y)
        {
            return x >= this->x && x <= this->x + width &&
                   y >= this->y && y <= this->y + height;
        }
    };
}