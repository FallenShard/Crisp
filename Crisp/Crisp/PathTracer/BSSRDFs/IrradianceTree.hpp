#pragma once

#include <Crisp/Math/BoundingBox.hpp>
#include <Crisp/PathTracer/Spectra/Spectrum.hpp>
#include <array>
#include <memory>
#include <vector>

namespace crisp {
struct SurfacePoint {
    SurfacePoint() {}

    SurfacePoint(const glm::vec3& p, const glm::vec3& n, float a)
        : p(p)
        , n(n)
        , area(a)
        , rayEpsilon(Ray3::Epsilon) {}

    glm::vec3 p;
    glm::vec3 n;
    float area;
    float rayEpsilon;
};

struct PoissonCheck {
    PoissonCheck(float maxDist, const glm::vec3& point)
        : maxDist2(maxDist * maxDist)
        , failed(false)
        , p(point) {}

    bool operator()(const SurfacePoint& point) {
        glm::vec3 diff = point.p - p;
        if (glm::dot(diff, diff) < maxDist2) {
            failed = true;
            return false;
        }

        return true;
    }

    float maxDist2;
    bool failed;
    glm::vec3 p;
};

struct IrradiancePoint {
    IrradiancePoint() {}

    IrradiancePoint(const SurfacePoint& surfPt, const Spectrum& spectrum)
        : p(surfPt.p)
        , n(surfPt.n)
        , e(spectrum)
        , area(surfPt.area)
        , rayEpsilon(surfPt.rayEpsilon) {}

    glm::vec3 p;
    glm::vec3 n;
    Spectrum e;
    float area;
    float rayEpsilon;
};

struct IrradianceNode {
    glm::vec3 p;
    bool isLeaf;
    Spectrum e;
    float sumArea;
    std::array<std::unique_ptr<IrradianceNode>, 8> children;
    std::array<IrradiancePoint*, 8> irrPts;

    IrradianceNode()
        : isLeaf(true)
        , sumArea(0.0f) {
        for (int i = 0; i < 8; i++) {
            irrPts[i] = nullptr;
            children[i] = nullptr;
        }
    }

    inline BoundingBox3 getChildBound(int child, const BoundingBox3& bounds, const glm::vec3& mid) {
        BoundingBox3 bound;
        bound.min.x = (child & 4) ? mid.x : bounds.min.x;
        bound.min.y = (child & 2) ? mid.y : bounds.min.y;
        bound.min.z = (child & 1) ? mid.z : bounds.min.z;
        bound.max.x = (child & 4) ? bounds.max.x : mid.x;
        bound.max.y = (child & 2) ? bounds.max.y : mid.y;
        bound.max.z = (child & 1) ? bounds.max.z : mid.z;
        return bound;
    }

    void insert(const BoundingBox3& nodeBounds, IrradiancePoint* point) {
        glm::vec3 mid = nodeBounds.getCenter();
        if (isLeaf) {
            for (int i = 0; i < 8; i++) {
                if (!irrPts[i]) {
                    irrPts[i] = point;
                    return;
                }
            }

            isLeaf = false;
            IrradiancePoint* localPts[8];
            for (int i = 0; i < 8; i++) {
                localPts[i] = irrPts[i];
                children[i] = nullptr;
            }

            for (int i = 0; i < 8; i++) {
                IrradiancePoint* currPt = localPts[i];

                int child =
                    (currPt->p.x > mid.x ? 4 : 0) + (currPt->p.y > mid.y ? 2 : 0) + (currPt->p.z > mid.z ? 1 : 0);
                if (children[child] == nullptr) {
                    children[child] = std::make_unique<IrradianceNode>();
                }

                BoundingBox3 childBounds = getChildBound(child, nodeBounds, mid);
                children[child]->insert(childBounds, currPt);
            }

            int child = (point->p.x > mid.x ? 4 : 0) + (point->p.y > mid.y ? 2 : 0) + (point->p.z > mid.z ? 1 : 0);
            if (children[child] == nullptr) {
                children[child] = std::make_unique<IrradianceNode>();
            }

            BoundingBox3 childBounds = getChildBound(child, nodeBounds, mid);
            children[child]->insert(childBounds, point);
        }
    }

    Spectrum getLeafIrradiance() {
        Spectrum sum(0.0f);
        if (isLeaf) {
            for (int i = 0; i < irrPts.size(); i++) {
                if (!irrPts[i]) {
                    break;
                }
                sum += irrPts[i]->e;
            }
        } else {
            for (int i = 0; i < children.size(); i++) {
                if (!children[i]) {
                    continue;
                }
                sum += children[i]->getLeafIrradiance();
            }
        }

        return sum;
    }

    Spectrum getTotalIrradiance() {
        Spectrum sum(0.0f);
        if (isLeaf) {
            for (int i = 0; i < irrPts.size(); i++) {
                if (!irrPts[i]) {
                    break;
                }
                sum += irrPts[i]->e;
            }
        } else {
            for (int i = 0; i < children.size(); i++) {
                if (!children[i]) {
                    continue;
                }
                sum += children[i]->getLeafIrradiance();
            }

            sum += e;
        }

        return sum;
    }

    void initHierarchy() {
        if (isLeaf) {
            float weightSum = 0.0f;
            size_t i;
            for (i = 0; i < 8; i++) {
                if (!irrPts[i]) {
                    break;
                }
                float weight = irrPts[i]->e.getLuminance();
                e += irrPts[i]->e;
                p += weight * irrPts[i]->p;
                weightSum += weight;
                sumArea += irrPts[i]->area;
            }

            if (weightSum > 0.0f) {
                p /= weightSum;
            }

            if (i > 0) {
                e /= static_cast<float>(i);
            }
        } else {
            float weightSum = 0.0f;
            size_t numChildren = 0;
            for (uint32_t i = 0; i < 8; i++) {
                if (!children[i]) {
                    continue;
                }
                ++numChildren;
                children[i]->initHierarchy();
                float weight = children[i]->e.getLuminance();
                e += children[i]->e;
                p += weight * children[i]->p;
                weightSum += weight;
                sumArea += children[i]->sumArea;
            }
            if (weightSum > 0.0f) {
                p /= weightSum;
            }

            if (numChildren > 0) {
                e /= static_cast<float>(numChildren);
            }
        }
    }

    template <typename Func>
    Spectrum Mo(const BoundingBox3& nodeBounds, const glm::vec3& pt, Func& func, float maxError) {
        glm::vec3 diff = pt - p;
        float sqrDist = glm::dot(diff, diff);
        float distanceWeight = sumArea / sqrDist;
        if (distanceWeight < maxError && !nodeBounds.contains(pt)) {
            return func(sqrDist) * e * sumArea;
        }

        Spectrum mo = 0.0f;
        if (isLeaf) {
            for (int i = 0; i < 8; i++) {
                if (!irrPts[i]) {
                    break;
                }

                glm::vec3 diffC = irrPts[i]->p - pt;
                float cSqrDist = glm::dot(diffC, diffC);

                mo += func(cSqrDist) * irrPts[i]->e * irrPts[i]->area;
            }
        } else {
            glm::vec3 mid = nodeBounds.getCenter();
            for (int child = 0; child < 8; child++) {
                if (!children[child]) {
                    continue;
                }

                BoundingBox3 childBounds = getChildBound(child, nodeBounds, mid);
                mo += children[child]->Mo(childBounds, pt, func, maxError);
            }
        }

        return mo;
    }
};

struct IrradianceTree {
    std::unique_ptr<IrradianceNode> root;
    BoundingBox3 boundingBox;
};
} // namespace crisp