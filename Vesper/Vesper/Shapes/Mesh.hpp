#pragma once

#include <Vesper/Shapes/Shape.hpp>

#include <CrispCore/Math/Distribution1D.hpp>
#include <CrispCore/Mesh/TriangleMesh.hpp>

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

        float triangleArea(unsigned int triangleId) const;

        glm::vec3 interpolatePosition(unsigned int triangleId, const glm::vec3& barycentric) const;
        glm::vec3 interpolateNormal(unsigned int triangleId, const glm::vec3& barycentric) const;
        glm::vec2 interpolateTexCoord(unsigned int triangleId, const glm::vec3& barycentric) const;

    protected:
        TriangleMesh m_mesh;
        Distribution1D m_pdf;
    };
}