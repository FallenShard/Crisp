#pragma once

#include <algorithm>

#include "Ray.hpp"

namespace crisp
{
    template <typename PointType>
    struct BoundingBox
    {
        PointType min;
        PointType max;

        BoundingBox()
        {
            reset();
        }

        BoundingBox(const PointType& p)
            : min(p)
            , max(p)
        {
        }

        BoundingBox(const PointType& min, const PointType& max)
            : min(min)
            , max(max)
        {
        }

        bool operator==(const BoundingBox& other) const
        {
            return min == other.min && max = other.max;
        }

        bool operator!=(const BoundingBox& other) const
        {
            return min != other.min || max != other.max;
        }

        float getVolume() const
        {
            auto temp = max - min;
            auto result = 1.0f;
            for (int i = 0; i < temp.length(); i++)
                result *= temp[i];
            return result;
        }

        float getSurfaceArea() const
        {
            auto diag = max - min;
            float result = 0.0f;
            for (int i = 0; i < diag.length(); i++)
            {
                float term = 1.0f;
                for (int j = 0; j < diag.length(); j++)
                {
                    if (i == j) continue;
                    term *= diag[j];
                }
                result += term;
            }
            return 2.0f * result;
        }

        PointType getCenter() const
        {
            return (max + min) * 0.5f;
        }

        bool contains(const PointType& p, bool strict = false) const
        {
            if (strict)
                return glm::all(glm::greaterThan(p, min)) && glm::all(glm::lessThan(p, max));
            else
                return glm::all(glm::greaterThanEqual(p, min)) && glm::all(glm::lessThanEqual(p, max));
        }

        bool contains(const BoundingBox& box, bool strict = false) const
        {
            if (strict)
                return glm::all(glm::greaterThan(box.min, min)) && glm::all(glm::lessThan(box.max, max));
            else
                return glm::all(glm::greaterThanEqual(box.min, min)) && glm::all(glm::lessThanEqual(box.max, max));
        }

        bool overlaps(const BoundingBox& box, bool strict = false) const
        {
            if (strict)
                return glm::all(glm::greaterThan(box.max, min)) && glm::all(glm::lessThan(box.min, max));
            else
                return glm::all(glm::greaterThanEqual(box.max, min)) && glm::all(glm::lessThanEqual(box.min, max));
        }

        float squaredDistanceTo(const PointType& p) const
        {
            float result = 0.0f;

            for (int i = 0; i < p.length(); i++)
            {
                float value = 0.0f;
                if (p[i] < min[i])
                    value = min[i] - p[i];
                else if (p[i] > max[i])
                    value = p[i] - max[i];
                result += value * value;
            }

            return result;
        }

        float distanceTo(const PointType& p) const
        {
            return std::sqrtf(squaredDistanceTo(p));
        }

        float squaredDistanceTo(const BoundingBox& box) const
        {
            float result = 0;

            for (int i = 0; i < min.length(); i++)
            {
                float value = 0;
                if (box.max[i] < min[i])
                    value = min[i] - box.max[i];
                else if (box.min[i] > max[i])
                    value = box.min[i] - max[i];
                result += value * value;
            }

            return result;
        }

        float distanceTo(const BoundingBox& box) const
        {
            return std::sqrtf(squaredDistanceTo(box));
        }

        bool isValid() const
        {
            return glm::all(glm::greaterThanEqual(max, min));
        }

        bool isPoint() const
        {
            return glm::all(glm::equal(max, min));
        }

        bool hasVolume() const
        {
            return glm::all(glm::greaterThanEqual(max, min));
        }

        int getMajorAxis() const
        {
            auto diag = max - min;
            int longest = 0;
            for (int i = 1; i < diag.length(); i++)
                if (diag[i] > diag[longest])
                    longest = i;
            return longest;
        }

        int getMinorAxis() const
        {
            auto diag = max - min;
            int shortest = 0;
            for (int i = 1; i < diag.length(); i++)
                if (diag[i] < diag[shortest])
                    shortest = i;
            return shortest;
        }

        PointType getExtents() const
        {
            return max - min;
        }

        void clip(const BoundingBox& box)
        {
            min = glm::max(min, box.min);
            max = glm::min(max, box.max);
        }

        void reset()
        {
            min = PointType(+std::numeric_limits<float>::infinity());
            max = PointType(-std::numeric_limits<float>::infinity());
        }

        void expandBy(float delta)
        {
            min -= PointType(delta);
            max += PointType(delta);
        }

        void expandBy(const PointType& p)
        {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }

        void expandBy(const BoundingBox& box)
        {
            min = glm::min(min, box.min);
            max = glm::max(max, box.max);
        }

        BoundingBox merge(const BoundingBox& box) const
        {
            return BoundingBox(glm::min(min, box.min), glm::max(max, box.max));
        }

        PointType getCorner(int index) const
        {
            PointType result;
            for (int i = 0; i < result.length(); i++)
                result[i] = (index & (1 << i)) ? max[i] : min[i];
            return result;
        }

        bool rayIntersect(const Ray<PointType, PointType>& ray) const
        {
            float nearT = -std::numeric_limits<float>::infinity();
            float farT  =  std::numeric_limits<float>::infinity();

            for (int i = 0; i < min.length(); i++)
            {
                float origin = ray.o[i];
                float minVal = min[i];
                float maxVal = max[i];

                if (ray.d[i] == 0)
                {
                    if (origin < minVal || origin > maxVal)
                        return false;
                }
                else
                {
                    float t1 = (minVal - origin) * ray.dInv[i];
                    float t2 = (maxVal - origin) * ray.dInv[i];

                    if (t1 > t2)
                        std::swap(t1, t2);

                    nearT = std::max(t1, nearT);
                    farT = std::min(t2, farT);

                    if (!(nearT <= farT))
                        return false;
                }
            }

            return ray.minT <= farT && nearT <= ray.maxT;
        }

        PointType lerp(PointType t) const
        {
            PointType result;
            for (int i = 0; i < result.length(); i++)
            {
                result[i] = lerp(min[i], max[i], t[i]);
            }
            return result;
        }

        PointType offset(const PointType& p) const
        {
            return (p - min) / (max - p);
        }

        float radius() const
        {
            return glm::length(getCenter() - min);
        }
    };

    using BoundingBox3 = BoundingBox<glm::vec3>;
}