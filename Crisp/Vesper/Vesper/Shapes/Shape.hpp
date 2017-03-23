#pragma once

#include <glm/glm.hpp>

#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>
#include <embree2/rtcore_scene.h>
#include <embree2/rtcore_geometry.h>
#include <embree2/rtcore_geometry_user.h>

#include "Math/CoordinateFrame.hpp"
#include "Math/Ray.hpp"
#include "Math/Transform.hpp"
#include "Math/BoundingBox.hpp"
#include "Core/Intersection.hpp"
#include "Core/VariantMap.hpp"

namespace vesper
{
    class BSDF;
    class Light;
    class Sampler;
    class Scene;

    class Shape
    {
    public:
        struct Sample
        {
            glm::vec3 ref; // Vantage point for the sample
            glm::vec3 p;   // Sampled point on the shape surface
            glm::vec3 n;   // Normal at the sampled point
            float pdf;     // pdf of the sample

            Sample() {}
            Sample(const glm::vec3& ref) : ref(ref) {}
            Sample(const glm::vec3& ref, const glm::vec3& p) : ref(ref), p(p) {}
        };

        Shape();

        virtual void fillIntersection(unsigned int triangleId, const Ray3& ray, Intersection& its) const = 0;
        virtual void sampleSurface(Shape::Sample& shapeSample, Sampler& sampler) const = 0;
        virtual float pdfSurface(const Shape::Sample& shapeSample) const = 0;
        virtual bool addToAccelerationStructure(RTCScene embreeScene) = 0;

        BoundingBox3 getBoundingBox() const;

        void setLight(Light* light);
        const Light* getLight() const;

        void setBSDF(BSDF* bsdf);
        const BSDF* getBSDF() const;

    protected:
        Light* m_light;
        BSDF* m_bsdf;

        Transform m_toWorld;

        unsigned int m_geomId;


        BoundingBox3 m_boundingBox;
    };
}