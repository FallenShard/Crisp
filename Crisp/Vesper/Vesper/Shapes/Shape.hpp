#pragma once

#include <memory>

#pragma warning(push)
#pragma warning(disable: 4324) // alignment warning
#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>
#include <embree2/rtcore_scene.h>
#include <embree2/rtcore_geometry.h>
#include <embree2/rtcore_geometry_user.h>
#pragma warning(pop)

#include <CrispCore/Math/CoordinateFrame.hpp>
#include <CrispCore/Math/Ray.hpp>
#include <CrispCore/Math/Transform.hpp>
#include <CrispCore/Math/BoundingBox.hpp>
#include "Core/Intersection.hpp"
#include "Core/VariantMap.hpp"

namespace crisp
{
    class BSDF;
    class Light;
    class Sampler;
    class Scene;
    class Medium;
    class BSSRDF;

    class Shape
    {
    public:
        struct Sample
        {
            glm::vec3 ref; // Vantage point for the sample
            glm::vec3 p;   // Sampled point on the shape surface
            glm::vec3 n;   // Normal at the sampled point
            float pdf;     // pdf of the sample

            Sample() : pdf(0.0f) {}
            Sample(const glm::vec3& ref) : ref(ref), pdf(0.0f) {}
            Sample(const glm::vec3& ref, const glm::vec3& p) : ref(ref), p(p), pdf(0.0f) {}
        };

        Shape();
        ~Shape();

        virtual void fillIntersection(unsigned int triangleId, const Ray3& ray, Intersection& its) const = 0;
        virtual void sampleSurface(Shape::Sample& shapeSample, Sampler& sampler) const = 0;
        virtual float pdfSurface(const Shape::Sample& shapeSample) const = 0;
        virtual bool addToAccelerationStructure(RTCScene embreeScene) = 0;

        BoundingBox3 getBoundingBox() const;

        void setLight(Light* light);
        const Light* getLight() const;

        void setBSDF(BSDF* bsdf);
        const BSDF* getBSDF() const;

        void setBSSRDF(std::unique_ptr<BSSRDF> bssrdf);
        const BSSRDF* getBSSRDF() const;
        BSSRDF* getBSSRDF();

        void setMedium(Medium* medium);
        const Medium* getMedium() const;

    protected:
        Light* m_light;
        BSDF* m_bsdf;
        std::unique_ptr<BSSRDF> m_bssrdf;
        Medium* m_medium;

        Transform m_toWorld;

        unsigned int m_geomId;


        BoundingBox3 m_boundingBox;
    };
}