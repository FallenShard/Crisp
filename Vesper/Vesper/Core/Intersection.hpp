#pragma once

#include <CrispCore/Math/Headers.hpp>
#include <CrispCore/Math/CoordinateFrame.hpp>

namespace crisp
{
    class Shape;

    struct Intersection
    {
        float tHit;                 // Unoccluded distance the ray traveled
        glm::vec3 p;                // Intersection point
        glm::vec2 uv;               // UV texture coordinates of the hit

        CoordinateFrame shFrame;    // Shading frame, formed by interpolated normal
        CoordinateFrame geoFrame;   // Geometric frame, formed from surface orientation

        const Shape* shape;         // Pointer to the underlying shape

        Intersection()
            : shape(nullptr)
        {
        }

        inline glm::vec3 toLocal(const glm::vec3& dir) const
        {
            return shFrame.toLocal(dir);
        }

        inline glm::vec3 toWorld(const glm::vec3& dir) const
        {
            return shFrame.toWorld(dir);
        }
    };
}
