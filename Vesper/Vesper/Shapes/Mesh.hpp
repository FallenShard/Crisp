#pragma once

#include <Vesper/Shapes/Shape.hpp>

#include <Crisp/Math/Distribution1D.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>

namespace crisp
{
class Mesh : public Shape
{
public:
    Mesh(const VariantMap& params = VariantMap());

    virtual void fillIntersection(unsigned int triangleId, const Ray3& ray, Intersection& its) const override;
    virtual void sampleSurface(Shape::Sample& shapeSample, Sampler& sampler) const override;
    virtual float pdfSurface(const Shape::Sample& shapeSample) const override;
    virtual bool addToAccelerationStructure(RTCDevice embreeDevice, RTCScene embreeScene) override;

    virtual size_t getNumTriangles() const;
    virtual size_t getNumVertices() const;

    virtual const std::vector<glm::vec3>& getVertexPositions() const;
    virtual const std::vector<glm::uvec3>& getTriangleIndices() const;

protected:
    TriangleMesh m_mesh;
    Distribution1D m_pdf;
};
} // namespace crisp