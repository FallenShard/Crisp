#pragma once

#include "Shapes/Shape.hpp"

namespace crisp
{
    class Sphere : public Shape
    {
    public:
        Sphere(const VariantMap& params = VariantMap());
        virtual ~Sphere();

        static void fillBounds(const Sphere* spheres, size_t item, RTCBounds* bounds);
        static void rayIntersect(const Sphere* spheres, RTCRay& ray, size_t item);
        static void rayOcclude(const Sphere* spheres, RTCRay& ray, size_t item);

        virtual void fillIntersection(unsigned int triangleId, const Ray3& ray, Intersection& its) const override;
        virtual void sampleSurface(Shape::Sample& shapeSample, Sampler& sampler) const override;
        virtual float pdfSurface(const Shape::Sample& shapeSample) const override;
        virtual bool addToAccelerationStructure(RTCScene scene) override;

    protected:
        glm::vec3 m_center;
        float m_radius;
    };
}
