#pragma once

#include "Headers.hpp"
#include "Operations.hpp"

namespace crisp {
struct CoordinateFrame {
    glm::vec3 n;
    glm::vec3 s, t;

    CoordinateFrame() {}

    CoordinateFrame(const glm::vec3& n, const glm::vec3& s, const glm::vec3& t)
        : n(n)
        , s(s)
        , t(t) {}

    CoordinateFrame(const glm::vec3& n)
        : n(n) {
        coordinateSystem(n, s, t);
    }

    inline glm::vec3 toLocal(const glm::vec3& v) const {
        return glm::vec3(glm::dot(v, s), glm::dot(v, t), glm::dot(v, n));
    }

    inline glm::vec3 toWorld(const glm::vec3& v) const {
        return s * v.x + t * v.y + n * v.z;
    }

    inline static float cosTheta(const glm::vec3& v) {
        return v.z;
    }

    inline static float sinTheta(const glm::vec3& v) {
        float sinTheta2 = 1.0f - v.z * v.z;
        if (sinTheta2 <= 0.0f) {
            return 0.0f;
        }

        return std::sqrtf(sinTheta2);
    }

    inline static float tanTheta(const glm::vec3& v) {
        float sinTheta2 = 1.0f - v.z * v.z;
        if (sinTheta2 <= 0.0f) {
            return 0.0f;
        }

        return std::sqrtf(sinTheta2) / v.z;
    }

    inline static float sinTheta2(const glm::vec3& v) {
        return 1.0f - v.z * v.z;
    }

    inline static float sinPhi(const glm::vec3& v) {
        float sinTheta = CoordinateFrame::sinTheta(v);
        if (sinTheta == 0.0f) {
            return 1.0f;
        }

        return clamp(v.y / sinTheta, -1.0f, 1.0f);
    }

    inline static float cosPhi(const glm::vec3& v) {
        float sinTheta = CoordinateFrame::sinTheta(v);
        if (sinTheta == 0.0f) {
            return 1.0f;
        }

        return clamp(v.x / sinTheta, -1.0f, 1.0f);
    }

    inline static float sinPhi2(const glm::vec3& v) {
        return clamp(v.y * v.y / CoordinateFrame::sinTheta2(v), 0.0f, 1.0f);
    }

    inline static float cosPhi2(const glm::vec3& v) {
        return clamp(v.x * v.x / CoordinateFrame::sinTheta2(v), 0.0f, 1.0f);
    }

    bool operator==(const CoordinateFrame& frame) const {
        return frame.s == s && frame.t == t && frame.n == n;
    }

    bool operator!=(const CoordinateFrame& frame) const {
        return !operator==(frame);
    }
};
} // namespace crisp