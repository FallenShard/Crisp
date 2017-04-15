#include "Mesh.hpp"

#include "Samplers/Sampler.hpp"
#include "Math/Warp.hpp"
#include "Core/Scene.hpp"

#include "MeshLoader.hpp"

namespace vesper
{
    Mesh::Mesh(const VariantMap& params)
    {
        std::string filename = params.get<std::string>("filename");

        m_toWorld = params.get<Transform>("toWorld");

        MeshLoader meshLoader;
        if (!meshLoader.load(filename, m_positions, m_normals, m_texCoords, m_faces))
        {
            std::cerr << "Could not open " + filename << std::endl;
            return;
        }

        for (auto& p : m_positions)
            p = m_toWorld.transformPoint(p);

        for (auto& n : m_normals)
            n = m_toWorld.transformNormal(n);

        for (auto& p : m_positions)
            m_boundingBox.expandBy(p);

        m_pdf.reserve(getNumTriangles());
        for (auto i = 0; i < getNumTriangles(); i++)
            m_pdf.append(triangleArea(i));
        m_pdf.normalize();
    }

    Mesh::~Mesh()
    {
    }

    void Mesh::fillIntersection(unsigned int triangleId, const Ray3& ray, Intersection& its) const
    {
        glm::vec3 barycentric;
        barycentric[0] = 1.0f - its.uv.x - its.uv.y;
        barycentric[1] = its.uv.x;
        barycentric[2] = its.uv.y;
        
        glm::vec3 p0 = m_positions[m_faces[triangleId][0]];
        glm::vec3 p1 = m_positions[m_faces[triangleId][1]];
        glm::vec3 p2 = m_positions[m_faces[triangleId][2]];

        its.p = barycentric.x * p0 + barycentric.y * p1 + barycentric.z * p2;

        if (m_texCoords.size() > 0)
            its.uv = interpolateTexCoord(triangleId, barycentric);

        its.geoFrame = CoordinateFrame(glm::normalize(glm::cross(p1 - p0, p2 - p0)));

        if (m_normals.size() > 0)
            its.shFrame = CoordinateFrame(interpolateNormal(triangleId, barycentric));
        else
            its.shFrame = its.geoFrame;

        its.shape = this;
    }

    void Mesh::sampleSurface(Shape::Sample& shapeSample, Sampler& sampler) const
    {
        glm::vec2 sample = sampler.next2D();
        unsigned int triangleId = static_cast<unsigned int>(m_pdf.sampleReuse(sample.x));

        glm::vec3 barycentric = Warp::squareToUniformTriangle(sample);

        shapeSample.p = interpolatePosition(triangleId, barycentric);
        if (m_normals.size() > 0)
            shapeSample.n = interpolateNormal(triangleId, barycentric);
        else
        {
            const auto& p0 = m_positions[m_faces[triangleId][0]];
            const auto& p1 = m_positions[m_faces[triangleId][1]];
            const auto& p2 = m_positions[m_faces[triangleId][2]];
            shapeSample.n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
        }
        shapeSample.pdf = m_pdf.getNormFactor();
    }

    float Mesh::pdfSurface(const Shape::Sample& shapeSample) const
    {
        return m_pdf.getNormFactor();
    }

    bool Mesh::addToAccelerationStructure(RTCScene scene)
    {
        if (!m_faces.empty())
        {
            m_geomId = rtcNewTriangleMesh(scene, RTC_GEOMETRY_STATIC, getNumTriangles(), getNumVertices(), 1);

            glm::vec4* vertices = static_cast<glm::vec4*>(rtcMapBuffer(scene, m_geomId, RTC_VERTEX_BUFFER));
            for (int i = 0; i < m_positions.size(); i++)
            {
                vertices[i].x = m_positions[i].x;
                vertices[i].y = m_positions[i].y;
                vertices[i].z = m_positions[i].z;
                vertices[i].w = 1.0f;
            }
            rtcUnmapBuffer(scene, m_geomId, RTC_VERTEX_BUFFER);

            glm::uvec3* faces = static_cast<glm::uvec3*>(rtcMapBuffer(scene, m_geomId, RTC_INDEX_BUFFER));
            for (int i = 0; i < m_faces.size(); i++)
            {
                faces[i].x = m_faces[i].x;
                faces[i].y = m_faces[i].y;
                faces[i].z = m_faces[i].z;
            }
            rtcUnmapBuffer(scene, m_geomId, RTC_INDEX_BUFFER);
        }

        return !m_faces.empty();
    }

    size_t Mesh::getNumTriangles() const
    {
        return m_faces.size();
    }

    size_t Mesh::getNumVertices() const
    {
        return m_positions.size();
    }

    const std::vector<glm::vec3>& Mesh::getVertexPositions() const
    {
        return m_positions;
    }
    const std::vector<glm::uvec3>& Mesh::getTriangleIndices() const
    {
        return m_faces;
    }

    float Mesh::triangleArea(unsigned int triangleId) const
    {
        const auto& p0 = m_positions[m_faces[triangleId][0]];
        const auto& p1 = m_positions[m_faces[triangleId][1]];
        const auto& p2 = m_positions[m_faces[triangleId][2]];

        return 0.5f * glm::length(glm::cross(p1 - p0, p2 - p0));
    }

    glm::vec3 Mesh::interpolatePosition(unsigned int triangleId, const glm::vec3& barycentric) const
    {
        const auto& p0 = m_positions[m_faces[triangleId][0]];
        const auto& p1 = m_positions[m_faces[triangleId][1]];
        const auto& p2 = m_positions[m_faces[triangleId][2]];
        return barycentric.x * p0 + barycentric.y * p1 + barycentric.z * p2;
    }

    glm::vec3 Mesh::interpolateNormal(unsigned int triangleId, const glm::vec3& barycentric) const
    {
        const auto& n0 = m_normals[m_faces[triangleId][0]];
        const auto& n1 = m_normals[m_faces[triangleId][1]];
        const auto& n2 = m_normals[m_faces[triangleId][2]];
        return glm::normalize(barycentric.x * n0 + barycentric.y * n1 + barycentric.z * n2);
    }

    glm::vec2 Mesh::interpolateTexCoord(unsigned int triangleId, const glm::vec3& barycentric) const
    {
        const auto& tc0 = m_texCoords[m_faces[triangleId][0]];
        const auto& tc1 = m_texCoords[m_faces[triangleId][1]];
        const auto& tc2 = m_texCoords[m_faces[triangleId][2]];
        return barycentric.x * tc0 + barycentric.y * tc1 + barycentric.z * tc2;
    }
}