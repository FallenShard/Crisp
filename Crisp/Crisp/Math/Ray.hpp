#pragma once

#include <limits>

#include <Crisp/Math/Headers.hpp>

namespace crisp {
template <typename PointType, typename DirType>
struct Ray {
    using ScalarType = typename PointType::value_type;
    static constexpr ScalarType Epsilon = 1e-4f;

    PointType o;
    DirType d;
    DirType dInv;
    ScalarType minT{Epsilon};
    ScalarType maxT{std::numeric_limits<ScalarType>::infinity()};
    ScalarType time{0.0f};

    Ray() = default;

    Ray(const PointType& origin,
        const DirType& dir,
        float minT = Epsilon,
        float maxT = std::numeric_limits<float>::infinity(),
        float time = 0.0f)
        : o(origin)
        , d(dir)
        , minT(minT)
        , maxT(maxT)
        , time(time) {
        update();
    }

    Ray(const Ray& ray, float minT, float maxT)
        : o(ray.o)
        , d(ray.d)
        , dInv(ray.dInv)
        , time(ray.time)
        , minT(minT)
        , maxT(maxT) {}

    void update() {
        dInv = DirType(1.0f) / d;
    }

    PointType operator()(float t) const {
        return o + t * d;
    }

    Ray reverse() const {
        Ray result;
        result.o = o;
        result.d = -d;
        result.dInv = -d;
        result.minT = minT;
        result.maxT = maxT;
        result.time = time;
        return result;
    }
};

using Ray3 = Ray<glm::vec3, glm::vec3>;
} // namespace crisp