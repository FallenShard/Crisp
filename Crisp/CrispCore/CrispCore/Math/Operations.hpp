#pragma once

#include <glm/glm.hpp>

#include <CrispCore/Math/Constants.hpp>

namespace crisp
{
    inline static float degToRad(float value)
    {
        return value * PI / 180.0f;
    }

    inline static void coordinateSystem(const glm::vec3& v1, glm::vec3& v2, glm::vec3& v3)
    {
        if (std::fabsf(v1.x) > std::fabsf(v1.y))
        {
            float invLen = 1.0f / std::sqrtf(v1.x * v1.x + v1.z * v1.z);
            v3 = glm::vec3(v1.z * invLen, 0.0f, -v1.x * invLen);
        }
        else
        {
            float invLen = 1.0f / std::sqrtf(v1.y * v1.y + v1.z * v1.z);
            v3 = glm::vec3(0.0f, v1.z * invLen, -v1.y * invLen);
        }
        v2 = glm::cross(v3, v1);
    }

    inline static glm::vec3 faceForward(const glm::vec3& n, const glm::vec3& v)
    {
        return glm::dot(v, n) < 0.0f ? -n : n;
    }

    template <typename T>
    inline static T clamp(T value, T minVal, T maxVal)
    {
        if (value > maxVal) return maxVal;
        if (value < minVal) return minVal;

        return value;
    }

    inline static float sign(float value)
    {
        return value < 0.0f ? -1.0f : 1.0f;
    }
}