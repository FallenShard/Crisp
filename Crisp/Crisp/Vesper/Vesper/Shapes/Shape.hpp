#pragma once

#include <memory>

#pragma warning(push)
#pragma warning(disable : 4324) // alignment warning
#include <embree3/rtcore.h>
#include <embree3/rtcore_geometry.h>
#include <embree3/rtcore_ray.h>
#include <embree3/rtcore_scene.h>
#pragma warning(pop)

#include "Core/Intersection.hpp"
#include "Core/VariantMap.hpp"
#include <Crisp/Math/BoundingBox.hpp>
#include <Crisp/Math/CoordinateFrame.hpp>
#include <Crisp/Math/Ray.hpp>
#include <Crisp/Math/Transform.hpp>

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

        Sample()
            : pdf(0.0f)
        {
        }

        Sample(const glm::vec3& ref)
            : ref(ref)
            , pdf(0.0f)
        {
        }

        Sample(const glm::vec3& ref, const glm::vec3& p)
            : ref(ref)
            , p(p)
            , pdf(0.0f)
        {
        }
    };

    Shape();
    virtual ~Shape();

    virtual void fillIntersection(unsigned int triangleId, const Ray3& ray, Intersection& its) const = 0;
    virtual void sampleSurface(Shape::Sample& shapeSample, Sampler& sampler) const = 0;
    virtual float pdfSurface(const Shape::Sample& shapeSample) const = 0;
    virtual bool addToAccelerationStructure(RTCDevice embreeDevice, RTCScene embreeScene) = 0;

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

    unsigned int m_geometryId;
    RTCGeometry m_geometry;
    RTCBuffer m_vertexBuffer;
    RTCBuffer m_indexBuffer;

    BoundingBox3 m_boundingBox;
};
} // namespace crisp