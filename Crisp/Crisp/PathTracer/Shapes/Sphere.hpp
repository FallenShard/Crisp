#pragma once

#include <Crisp/PathTracer/Shapes/Shape.hpp>

namespace crisp {
class Sphere : public Shape {
public:
    Sphere(const VariantMap& params = VariantMap());
    virtual ~Sphere();

    static void fillBounds(const RTCBoundsFunctionArguments* args);
    static void rayIntersect(const RTCIntersectFunctionNArguments* args);
    static void rayOcclude(const RTCOccludedFunctionNArguments* args);

    virtual void fillIntersection(unsigned int triangleId, const Ray3& ray, Intersection& its) const override;
    virtual void sampleSurface(Shape::Sample& shapeSample, Sampler& sampler) const override;
    virtual float pdfSurface(const Shape::Sample& shapeSample) const override;
    virtual bool addToAccelerationStructure(RTCDevice device, RTCScene scene) override;

protected:
    glm::vec3 m_center;
    float m_radius;
};
} // namespace crisp
