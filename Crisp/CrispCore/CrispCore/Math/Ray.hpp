#pragma once

#include <limits>

#include "Headers.hpp"

namespace crisp
{
    template <typename PointType, typename DirType>
    struct Ray
    {
        static constexpr float Epsilon = 1e-4f;

        PointType o;
        DirType d;
        DirType dInv;
        float minT;
        float maxT;
        float time;

        Ray()
            : minT(Epsilon)
            , maxT(std::numeric_limits<float>::infinity())
            , time(0.0f)
        {
        }

        Ray(const PointType& origin, const DirType& dir, float minT = Epsilon, float maxT = std::numeric_limits<float>::infinity(), float time = 0.0f)
            : o(origin)
            , d(dir)
            , minT(minT)
            , maxT(maxT)
            , time(time)
        {
            update();
        }

        Ray(const Ray& ray)
            : o(ray.o)
            , d(ray.d)
            , dInv(ray.dInv)
            , minT(ray.minT)
            , maxT(ray.maxT)
            , time(ray.time)
        {
        }

        Ray(const Ray& ray, float minT, float maxT)
            : o(ray.o)
            , d(ray.d)
            , dInv(ray.dInv)
            , time(ray.time)
            , minT(minT)
            , maxT(maxT)
        {
        }

        void update()
        {
            dInv = DirType(1.0f) / d;
        }

        PointType operator() (float t) const
        {
            return o + t * d;
        }

        Ray reverse() const
        {
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
}