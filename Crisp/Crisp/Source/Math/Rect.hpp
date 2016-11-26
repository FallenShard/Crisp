#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

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