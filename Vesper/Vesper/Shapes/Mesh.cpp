#include <Vesper/Shapes/Mesh.hpp>

#include <Vesper/Samplers/Sampler.hpp>
#include <CrispCore/Math/Warp.hpp>
#include <CrispCore/IO/MeshReader.hpp>

namespace crisp
{
    Mesh::Mesh(const VariantMap& params)
        : m_mesh(loadMesh(params.get<std::string>("filename")))
    {
        m_toWorld = params.get<Transform>("toWorld");

        m_mesh.transform(m_toWorld.mat);
        m_boundingBox = m_mesh.getBoundingBox();

        m_pdf.reserve(getNumTriangles());
        for (auto i = 0; i < getNumTriangles(); i++)
            m_pdf.append(triangleArea(i));
        m_pdf.normalize();
    }

    void Mesh::fillIntersection(unsigned int triangleId, const Ray3& /*ray*/, Intersection& its) const
    {
        const glm::vec3 barycentric(1.0f - its.uv.x - its.uv.y, its.uv.x, its.uv.y);
        const auto& positions = m_mesh.getPositions();
        const auto& triangles = m_mesh.getFaces();
        const glm::vec3& p0 = positions[triangles[triangleId][0]];
        const glm::vec3& p1 = positions[triangles[triangleId][1]];
        const glm::vec3& p2 = positions[triangles[triangleId][2]];

        its.p = barycentric.x * p0 + barycentric.y * p1 + barycentric.z * p2;
        its.geoFrame = CoordinateFrame(glm::normalize(glm::cross(p1 - p0, p2 - p0)));

        if (m_mesh.getNormals().size() > 0)
            its.shFrame = CoordinateFrame(interpolateNormal(triangleId, barycentric));
        else
            its.shFrame = its.geoFrame;

        if (m_mesh.getTexCoords().size() > 0)
            its.uv = interpolateTexCoord(triangleId, barycentric);

        its.shape = this;
    }

    void Mesh::sampleSurface(Shape::Sample& shapeSample, Sampler& sampler) const
    {
        glm::vec2 sample = sampler.next2D();
        const uint32_t triangleId = static_cast<uint32_t>(m_pdf.sampleReuse(sample.x));

        const glm::vec3 barycentric = warp::squareToUniformTriangle(sample);

        shapeSample.p = interpolatePosition(triangleId, barycentric);
        if (m_mesh.getNormals().size() > 0)
            shapeSample.n = interpolateNormal(triangleId, barycentric);
        else
        {
            const auto& positions = m_mesh.getPositions();
            const auto& triangles = m_mesh.getFaces();
            const glm::vec3& p0 = positions[triangles[triangleId][0]];
            const glm::vec3& p1 = positions[triangles[triangleId][1]];
            const glm::vec3& p2 = positions[triangles[triangleId][2]];
            shapeSample.n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
        }
        shapeSample.pdf = m_pdf.getNormFactor();
    }

    float Mesh::pdfSurface(const Shape::Sample& /*shapeSample*/) const
    {
        return m_pdf.getNormFactor();
    }

    bool Mesh::addToAccelerationStructure(RTCDevice device, RTCScene scene)
    {
        if (!m_mesh.getFaces().empty())
        {
            m_geometry = rtcNewGeometry(device, RTCGeometryType::RTC_GEOMETRY_TYPE_TRIANGLE);

            m_vertexBuffer = rtcNewBuffer(device, sizeof(glm::vec3) * m_mesh.getVertexCount());
            glm::vec3* vertices = static_cast<glm::vec3*>(rtcGetBufferData(m_vertexBuffer));
            memcpy(vertices, m_mesh.getPositions().data(), m_mesh.getVertexCount() * sizeof(glm::vec3));
            rtcSetGeometryBuffer(m_geometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, m_vertexBuffer, 0, sizeof(glm::vec3), m_mesh.getVertexCount());

            m_indexBuffer = rtcNewBuffer(device, sizeof(glm::uvec3) * m_mesh.getFaceCount());
            glm::uvec3* faces = static_cast<glm::uvec3*>(rtcGetBufferData(m_indexBuffer));
            memcpy(faces, m_mesh.getFaces().data(), m_mesh.getFaceCount() * sizeof(glm::uvec3));
            rtcSetGeometryBuffer(m_geometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, m_indexBuffer, 0, sizeof(glm::uvec3), m_mesh.getFaceCount());

            rtcCommitGeometry(m_geometry);
            m_geometryId = rtcAttachGeometry(scene, m_geometry);
            return true;
        }

        return false;
    }

    size_t Mesh::getNumTriangles() const
    {
        return m_mesh.getFaceCount();
    }

    size_t Mesh::getNumVertices() const
    {
        return m_mesh.getVertexCount();
    }

    const std::vector<glm::vec3>& Mesh::getVertexPositions() const
    {
        return m_mesh.getPositions();
    }
    const std::vector<glm::uvec3>& Mesh::getTriangleIndices() const
    {
        return m_mesh.getFaces();
    }

    float Mesh::triangleArea(unsigned int triangleId) const
    {
        const auto& positions = m_mesh.getPositions();
        const auto& triangles = m_mesh.getFaces();
        const glm::vec3& p0 = positions[triangles[triangleId][0]];
        const glm::vec3& p1 = positions[triangles[triangleId][1]];
        const glm::vec3& p2 = positions[triangles[triangleId][2]];

        return 0.5f * glm::length(glm::cross(p1 - p0, p2 - p0));
    }

    glm::vec3 Mesh::interpolatePosition(unsigned int triangleId, const glm::vec3& barycentric) const
    {
        const auto& positions = m_mesh.getPositions();
        const auto& triangles = m_mesh.getFaces();
        const glm::vec3& p0 = positions[triangles[triangleId][0]];
        const glm::vec3& p1 = positions[triangles[triangleId][1]];
        const glm::vec3& p2 = positions[triangles[triangleId][2]];
        return barycentric.x * p0 + barycentric.y * p1 + barycentric.z * p2;
    }

    glm::vec3 Mesh::interpolateNormal(unsigned int triangleId, const glm::vec3& barycentric) const
    {
        const auto& normals = m_mesh.getNormals();
        const auto& triangles = m_mesh.getFaces();
        const glm::vec3& n0 = normals[triangles[triangleId][0]];
        const glm::vec3& n1 = normals[triangles[triangleId][1]];
        const glm::vec3& n2 = normals[triangles[triangleId][2]];
        return glm::normalize(barycentric.x * n0 + barycentric.y * n1 + barycentric.z * n2);
    }

    glm::vec2 Mesh::interpolateTexCoord(unsigned int triangleId, const glm::vec3& barycentric) const
    {
        const auto& texCoords = m_mesh.getTexCoords();
        const auto& triangles = m_mesh.getFaces();
        const glm::vec2& tc0 = texCoords[triangles[triangleId][0]];
        const glm::vec2& tc1 = texCoords[triangles[triangleId][1]];
        const glm::vec2& tc2 = texCoords[triangles[triangleId][2]];
        return barycentric.x * tc0 + barycentric.y * tc1 + barycentric.z * tc2;
    }
}