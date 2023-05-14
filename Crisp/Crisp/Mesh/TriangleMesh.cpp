#include <Crisp/Mesh/TriangleMesh.hpp>

#include <Crisp/Core/Checks.hpp>

namespace crisp
{
namespace
{
static void fillInterleaved(
    std::vector<std::byte>& interleavedBuffer,
    size_t vertexSize,
    size_t offset,
    const std::vector<std::byte>& attribute,
    size_t attribSize)
{
    for (size_t i = 0; i < attribute.size(); i++)
    {
        memcpy(&interleavedBuffer[i * vertexSize + offset], &attribute[i * attribSize], attribSize);
    }
}

template <typename T>
static void fillInterleaved(
    std::vector<std::byte>& interleavedBuffer, size_t vertexSize, size_t offset, const std::vector<T>& attribute)
{
    for (size_t i = 0; i < attribute.size(); i++)
    {
        memcpy(&interleavedBuffer[i * vertexSize + offset], &attribute[i], sizeof(T));
    }
}
} // namespace

TriangleMesh::TriangleMesh(
    std::vector<glm::vec3> positions,
    std::vector<glm::vec3> normals,
    std::vector<glm::vec2> texCoords,
    std::vector<glm::uvec3> faces,
    std::vector<VertexAttributeDescriptor> vertexAttributes)
    : m_positions(std::move(positions))
    , m_normals(std::move(normals))
    , m_texCoords(std::move(texCoords))
    , m_faces(std::move(faces))
{
    m_views.emplace_back("", 0, static_cast<uint32_t>(m_faces.size() * 3));

    bool missingTexCoords{m_texCoords.empty()};
    bool missingNormals = false;
    bool missingTangents = false;
    for (const auto& attrib : vertexAttributes)
    {
        if (attrib.type == VertexAttribute::Normal)
        {
            missingNormals = m_normals.empty();
        }
        else if (attrib.type == VertexAttribute::TexCoord)
        {
            if (m_texCoords.empty())
            {
                m_texCoords.reserve(m_positions.size());
                for (uint32_t i = 0; i < m_positions.size(); ++i)
                    m_texCoords.emplace_back(0.0f, 0.0f);
            }
        }
        else if (attrib.type == VertexAttribute::Tangent)
        {
            missingTangents = true;
        }
    }

    if (missingNormals)
        computeVertexNormals();

    if (missingTangents)
    {
        if (!missingTexCoords)
        {
            computeTangentVectors();
        }
        else
        {
            m_tangents.resize(m_positions.size(), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        }
    }

    computeBoundingBox();
}

const std::vector<glm::uvec3>& TriangleMesh::getFaces() const
{
    return m_faces;
}

uint32_t TriangleMesh::getFaceCount() const
{
    return static_cast<uint32_t>(m_faces.size());
}

uint32_t TriangleMesh::getIndexCount() const
{
    return static_cast<uint32_t>(m_faces.size()) * 3;
}

uint32_t TriangleMesh::getVertexCount() const
{
    return static_cast<uint32_t>(m_positions.size());
}

InterleavedVertexBuffer TriangleMesh::interleave(
    const std::vector<VertexAttributeDescriptor>& vertexAttribs, bool padToVec4) const
{
    InterleavedVertexBuffer interleavedBuffer;

    interleavedBuffer.vertexSize = 0;
    for (const auto& attrib : vertexAttribs)
        interleavedBuffer.vertexSize += padToVec4 ? sizeof(glm::vec4) : attrib.size;

    interleavedBuffer.buffer.resize(interleavedBuffer.vertexSize * getVertexCount());

    uint32_t currOffset = 0;
    for (const auto& attrib : vertexAttribs)
    {
        if (attrib.type == VertexAttribute::Position)
            fillInterleaved(interleavedBuffer.buffer, interleavedBuffer.vertexSize, currOffset, m_positions);
        else if (attrib.type == VertexAttribute::Normal)
            fillInterleaved(interleavedBuffer.buffer, interleavedBuffer.vertexSize, currOffset, m_normals);
        else if (attrib.type == VertexAttribute::TexCoord)
            fillInterleaved(interleavedBuffer.buffer, interleavedBuffer.vertexSize, currOffset, m_texCoords);
        else if (attrib.type == VertexAttribute::Tangent)
            fillInterleaved(interleavedBuffer.buffer, interleavedBuffer.vertexSize, currOffset, m_tangents);
        else if (attrib.type == VertexAttribute::Custom)
        {
            const auto& attribBuffer = m_customAttributes.at(attrib.name);
            fillInterleaved(
                interleavedBuffer.buffer,
                interleavedBuffer.vertexSize,
                currOffset,
                attribBuffer.buffer,
                attribBuffer.descriptor.size);
        }

        currOffset += padToVec4 ? sizeof(glm::vec4) : attrib.size;
    }

    return interleavedBuffer;
}

std::vector<InterleavedVertexBuffer> TriangleMesh::createInterleavedVertexBuffers(
    const std::vector<std::vector<VertexAttributeDescriptor>>& vertexAttribs, bool padToVec4) const
{
    std::vector<InterleavedVertexBuffer> buffers{};
    buffers.reserve(vertexAttribs.size());
    for (const auto& attribGroup : vertexAttribs)
    {
        buffers.emplace_back(interleave(attribGroup, padToVec4));
    }
    return buffers;
}

std::vector<glm::vec3> TriangleMesh::computeVertexNormals(
    const std::vector<glm::vec3>& positions, const std::vector<glm::uvec3>& faces)
{
    std::vector<glm::vec3> normals(positions.size(), glm::vec3(0.0f));

    for (const auto& face : faces)
    {
        glm::vec3 a = positions[face[0]] - positions[face[1]];
        glm::vec3 b = positions[face[0]] - positions[face[2]];
        glm::vec3 n = glm::normalize(glm::cross(a, b));
        normals[face[0]] += n;
        normals[face[1]] += n;
        normals[face[2]] += n;
    }

    for (auto& normal : normals)
        normal = glm::normalize(normal);

    return normals;
}

void TriangleMesh::computeTangentVectors()
{
    m_tangents = std::vector<glm::vec4>(m_positions.size(), glm::vec4(0.0f));
    std::vector<glm::vec3> bitangents = std::vector<glm::vec3>(m_positions.size(), glm::vec3(0.0f));

    CRISP_CHECK_EQ(m_texCoords.size(), m_positions.size(), "Missing tex coords.");
    for (const auto& face : m_faces)
    {
        const glm::vec3& v1 = m_positions[face[0]];
        const glm::vec3& v2 = m_positions[face[1]];
        const glm::vec3& v3 = m_positions[face[2]];

        const glm::vec2& w1 = m_texCoords[face[0]];
        const glm::vec2& w2 = m_texCoords[face[1]];
        const glm::vec2& w3 = m_texCoords[face[2]];

        const glm::vec3 e1 = v2 - v1;
        const glm::vec3 e2 = v3 - v1;

        const glm::vec2 s = w2 - w1;
        const glm::vec2 t = w3 - w1;

        const float r = 1.0f / (s.x * t.y - t.x * s.y);
        const glm::vec3 sDir = (e1 * t.y - e2 * s.y) * r;
        const glm::vec3 tDir = (e2 * s.x - e1 * t.x) * r;

        m_tangents[face[0]] += glm::vec4(sDir, 0.0f);
        m_tangents[face[1]] += glm::vec4(sDir, 0.0f);
        m_tangents[face[2]] += glm::vec4(sDir, 0.0f);
        bitangents[face[0]] += tDir;
        bitangents[face[1]] += tDir;
        bitangents[face[2]] += tDir;
    }

    for (std::size_t i = 0; i < m_tangents.size(); ++i)
    {
        const glm::vec3& n = m_normals[i];
        const glm::vec3 t = m_tangents[i];
        const glm::vec3& b = bitangents[i];

        const glm::vec3 clearT = glm::normalize(t - n * glm::dot(t, n));

        float handedness = glm::dot(glm::cross(t, b), n) > 0.0f ? 1.0f : -1.0f;
        m_tangents[i] = glm::vec4(clearT, handedness);
    }
}

void TriangleMesh::computeVertexNormals()
{
    m_normals = computeVertexNormals(m_positions, m_faces);
}

void TriangleMesh::computeBoundingBox()
{
    m_boundingBox = BoundingBox3();

    for (const auto& p : m_positions)
    {
        m_boundingBox.expandBy(p);
    }
}

const std::vector<TriangleMeshView>& TriangleMesh::getViews() const
{
    return m_views;
}

void TriangleMesh::normalizeToUnitBox()
{
    computeBoundingBox();

    const glm::vec3& extent{m_boundingBox.getExtents()};
    float longestAxis = std::max(extent.x, std::max(extent.y, extent.z));
    float scalingFactor = 1.0f / longestAxis;

    const glm::vec3& center{m_boundingBox.getCenter()};
    for (auto& p : m_positions)
    {
        p = (p - center) * scalingFactor;
    }
}

void TriangleMesh::transform(const glm::mat4& transform)
{
    for (auto& p : m_positions)
        p = glm::vec3(transform * glm::vec4(p, 1.0f));

    glm::mat4 invTransp = glm::inverse(glm::transpose(transform));
    for (auto& n : m_normals)
        n = glm::vec3(invTransp * glm::vec4(n, 0.0f));

    for (auto& t : m_tangents)
        t = glm::vec4(glm::vec3(invTransp * glm::vec4(t.x, t.y, t.z, 0.0f)), t.w);

    computeBoundingBox();
}

void TriangleMesh::setMeshName(std::string&& meshName)
{
    m_meshName = std::move(meshName);
}

std::string TriangleMesh::getMeshName() const
{
    return m_meshName;
}

void TriangleMesh::setPositions(std::vector<glm::vec3>&& positions)
{
    m_positions = std::move(positions);
    computeBoundingBox();
}

void TriangleMesh::setNormals(std::vector<glm::vec3>&& normals)
{
    m_normals = std::move(normals);
}

void TriangleMesh::setTexCoords(std::vector<glm::vec2>&& texCoords)
{
    m_texCoords = std::move(texCoords);
}

void TriangleMesh::setCustomAttribute(const std::string& id, VertexAttributeBuffer&& attributeBuffer)
{
    m_customAttributes.emplace(id, std::move(attributeBuffer));
}

void TriangleMesh::setFaces(std::vector<glm::uvec3>&& faces)
{
    m_faces = std::move(faces);
}

void TriangleMesh::setViews(std::vector<TriangleMeshView>&& views)
{
    m_views = std::move(views);
}

const std::vector<glm::vec3>& TriangleMesh::getPositions() const
{
    return m_positions;
}

const std::vector<glm::vec3>& TriangleMesh::getNormals() const
{
    return m_normals;
}

const std::vector<glm::vec2>& TriangleMesh::getTexCoords() const
{
    return m_texCoords;
}

const VertexAttributeBuffer& TriangleMesh::getCustomAttribute(const std::string& id) const
{
    return m_customAttributes.at(id);
}

const BoundingBox3& TriangleMesh::getBoundingBox() const
{
    return m_boundingBox;
}

bool TriangleMesh::hasCustomAttribute(const std::string& attributeName) const
{
    return m_customAttributes.contains(attributeName);
}

float TriangleMesh::calculateFaceArea(uint32_t triangleId) const
{
    const glm::vec3& p0 = m_positions[m_faces[triangleId][0]];
    const glm::vec3& p1 = m_positions[m_faces[triangleId][1]];
    const glm::vec3& p2 = m_positions[m_faces[triangleId][2]];
    return 0.5f * glm::length(glm::cross(p1 - p0, p2 - p0));
}

glm::vec3 TriangleMesh::calculateFaceNormal(uint32_t triangleId) const
{
    const glm::vec3& p0 = m_positions[m_faces[triangleId][0]];
    const glm::vec3& p1 = m_positions[m_faces[triangleId][1]];
    const glm::vec3& p2 = m_positions[m_faces[triangleId][2]];
    return glm::normalize(glm::cross(p1 - p0, p2 - p0));
}

glm::vec3 TriangleMesh::interpolatePosition(const uint32_t triangleId, const glm::vec3& barycentric) const
{
    const glm::vec3& p0 = m_positions[m_faces[triangleId][0]];
    const glm::vec3& p1 = m_positions[m_faces[triangleId][1]];
    const glm::vec3& p2 = m_positions[m_faces[triangleId][2]];
    return barycentric.x * p0 + barycentric.y * p1 + barycentric.z * p2;
}

glm::vec3 TriangleMesh::interpolateNormal(const uint32_t triangleId, const glm::vec3& barycentric) const
{
    const glm::vec3& n0 = m_normals[m_faces[triangleId][0]];
    const glm::vec3& n1 = m_normals[m_faces[triangleId][1]];
    const glm::vec3& n2 = m_normals[m_faces[triangleId][2]];
    return glm::normalize(barycentric.x * n0 + barycentric.y * n1 + barycentric.z * n2);
}

glm::vec2 TriangleMesh::interpolateTexCoord(const uint32_t triangleId, const glm::vec3& barycentric) const
{
    const glm::vec2& tc0 = m_texCoords[m_faces[triangleId][0]];
    const glm::vec2& tc1 = m_texCoords[m_faces[triangleId][1]];
    const glm::vec2& tc2 = m_texCoords[m_faces[triangleId][2]];
    return barycentric.x * tc0 + barycentric.y * tc1 + barycentric.z * tc2;
}
} // namespace crisp