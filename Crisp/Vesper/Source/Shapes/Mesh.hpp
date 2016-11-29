#pragma once

#include <vector>

#include "Shapes/Shape.hpp"
#include "Math/DiscretePdf.hpp"

namespace vesper
{
    class Mesh : public Shape
    {
    public:
        Mesh(const VariantMap& params = VariantMap());
        virtual ~Mesh();

        virtual void fillIntersection(unsigned int triangleId, const Ray3& ray, Intersection& its) const override;
        virtual void sampleSurface(Shape::Sample& shapeSample, Sampler& sampler) const override;
        virtual float pdfSurface(const Shape::Sample& shapeSample) const override;
        virtual bool addToAccelerationStructure(RTCScene embreeScene) override;

        virtual size_t getNumTriangles() const;
        virtual size_t getNumVertices() const;

        virtual const std::vector<glm::vec3>& getVertexPositions() const;
        virtual const std::vector<glm::uvec3>& getTriangleIndices() const;

        float triangleArea(unsigned int triangleId) const;

        glm::vec3 interpolatePosition(unsigned int triangleId, const glm::vec3& barycentric) const;
        glm::vec3 interpolateNormal(unsigned int triangleId, const glm::vec3& barycentric) const;
        glm::vec2 interpolateTexCoord(unsigned int triangleId, const glm::vec3& barycentric) const;

    protected:
        std::vector<glm::vec3> m_positions;
        std::vector<glm::vec3> m_normals;
        std::vector<glm::vec2> m_texCoords;
        std::vector<glm::uvec3> m_faces;

        DiscretePdf m_pdf;
    };
}