#include "TriangleMesh.hpp"

#include <Vesper/Shapes/MeshLoader.hpp>

#include "IO/WavefrontObjImporter.hpp"
#include "IO/FbxImporter.hpp"

namespace crisp
{
    namespace
    {
        static void fillInterleaved(std::vector<std::byte>& interleavedBuffer, size_t vertexSize, size_t offset, const std::vector<std::byte>& attribute, size_t attribSize)
        {
            for (size_t i = 0; i < attribute.size(); i++)
            {
                memcpy(&interleavedBuffer[i * vertexSize + offset], &attribute[i * attribSize], attribSize);
            }
        }

        template <typename T>
        static void fillInterleaved(std::vector<std::byte>& interleavedBuffer, size_t vertexSize, size_t offset, const std::vector<T>& attribute)
        {
            for (size_t i = 0; i < attribute.size(); i++)
            {
                memcpy(&interleavedBuffer[i * vertexSize + offset], &attribute[i], sizeof(T));
            }
        }
    }

    TriangleMesh::TriangleMesh()
    {
    }

    TriangleMesh::TriangleMesh(const std::filesystem::path& path)
        : TriangleMesh(path, { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord })
    {
    }

    TriangleMesh::TriangleMesh(const std::filesystem::path& path, const std::vector<VertexAttributeDescriptor>& vertexAttributes)
        : m_meshName(path.filename().stem().string())
        , m_interleavedFormat(vertexAttributes)
    {
        if (WavefrontObjImporter::isWavefrontObjFile(path))
        {
            WavefrontObjImporter importer(path);
            importer.moveDataToTriangleMesh(*this, m_interleavedFormat);
            return;
        }
        else if (FbxImporter::isFbxFile(path))
        {
            FbxImporter importer(path);
            importer.moveDataToTriangleMesh(*this, m_interleavedFormat);
            return;
        }

        MeshLoader meshLoader;
        meshLoader.load(path, m_positions, m_normals, m_texCoords, m_faces);
        computeBoundingBox();

        if (m_normals.empty())
            computeVertexNormals();
    }

    TriangleMesh::TriangleMesh(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals, const std::vector<glm::vec2>& texCoords,
        const std::vector<glm::uvec3>& faces, const std::vector<VertexAttributeDescriptor>& vertexAttributes)
        : m_interleavedFormat(vertexAttributes)
        , m_positions(positions)
        , m_normals(normals)
        , m_texCoords(texCoords)
        , m_faces(faces)
    {
        m_parts.emplace_back("", 0, static_cast<uint32_t>(m_faces.size() * 3));
        for (const auto& attrib : vertexAttributes)
            if (attrib.type == VertexAttribute::Tangent)
                computeTangentVectors();

        computeBoundingBox();
    }

    TriangleMesh::~TriangleMesh()
    {
    }

    const std::vector<glm::uvec3>& TriangleMesh::getFaces() const
    {
        return m_faces;
    }

    uint32_t TriangleMesh::getNumFaces() const
    {
        return static_cast<uint32_t>(m_faces.size());
    }

    uint32_t TriangleMesh::getNumIndices() const
    {
        return static_cast<uint32_t>(m_faces.size()) * 3;
    }

    uint32_t TriangleMesh::getNumVertices() const
    {
        return static_cast<uint32_t>(m_positions.size());
    }

    InterleavedVertexBuffer TriangleMesh::interleave() const
    {
        return interleave(m_interleavedFormat);
    }

    InterleavedVertexBuffer TriangleMesh::interleave(const std::vector<VertexAttributeDescriptor>& vertexAttribs) const
    {
        InterleavedVertexBuffer interleavedBuffer;

        interleavedBuffer.vertexSize = 0;
        for (const auto& attrib : vertexAttribs)
            interleavedBuffer.vertexSize += attrib.size;

        interleavedBuffer.buffer.resize(interleavedBuffer.vertexSize * getNumVertices());

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
                fillInterleaved(interleavedBuffer.buffer, interleavedBuffer.vertexSize, currOffset, attribBuffer.buffer, attribBuffer.descriptor.size);
            }

            currOffset += attrib.size;
        }

        return interleavedBuffer;
    }

    InterleavedVertexBuffer TriangleMesh::interleavePadded(const std::vector<VertexAttributeDescriptor>& vertexAttribs) const
    {
        InterleavedVertexBuffer interleavedBuffer;
        interleavedBuffer.vertexSize = vertexAttribs.size() * sizeof(glm::vec4);

        uint32_t numVertices = getNumVertices();
        interleavedBuffer.buffer.resize(interleavedBuffer.vertexSize * numVertices);

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
                fillInterleaved(interleavedBuffer.buffer, interleavedBuffer.vertexSize, currOffset, attribBuffer.buffer, attribBuffer.descriptor.size);
            }

            currOffset += sizeof(glm::vec4);
        }

        return interleavedBuffer;
    }

    std::vector<glm::vec3> TriangleMesh::computeVertexNormals(const std::vector<glm::vec3>& positions, const std::vector<glm::uvec3>& faces)
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
        m_tangents   = std::vector<glm::vec4>(m_positions.size(), glm::vec4(0.0f));
        m_bitangents = std::vector<glm::vec3>(m_positions.size(), glm::vec3(0.0f));

        for (const auto& face : m_faces)
        {
            const glm::vec3& v1 = m_positions[face[0]];
            const glm::vec3& v2 = m_positions[face[1]];
            const glm::vec3& v3 = m_positions[face[2]];

            const glm::vec2& w1 = m_texCoords[face[0]];
            const glm::vec2& w2 = m_texCoords[face[1]];
            const glm::vec2& w3 = m_texCoords[face[2]];

            glm::vec3 e1 = v2 - v1;
            glm::vec3 e2 = v3 - v1;

            glm::vec2 s = w2 - w1;
            glm::vec2 t = w3 - w1;

            float r = 1.0f / (s.x * t.y - t.x * s.y);
            glm::vec3 sDir = (e1 * t.y - e2 * s.y) * r;
            glm::vec3 tDir = (e2 * s.x - e1 * t.x) * r;

            m_tangents[face[0]]   += glm::vec4(sDir, 0.0f);
            m_tangents[face[1]]   += glm::vec4(sDir, 0.0f);
            m_tangents[face[2]]   += glm::vec4(sDir, 0.0f);
            m_bitangents[face[0]] += tDir;
            m_bitangents[face[1]] += tDir;
            m_bitangents[face[2]] += tDir;
        }

        for (std::size_t i = 0; i < m_tangents.size(); ++i)
        {
            const glm::vec3& n = m_normals[i];
            const glm::vec3 t = m_tangents[i];
            const glm::vec3& b = m_bitangents[i];

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
            m_boundingBox.expandBy(p);
    }

    const std::vector<GeometryPart>& TriangleMesh::getGeometryParts() const
    {
        return m_parts;
    }

    void TriangleMesh::normalizeToUnitBox()
    {
        glm::vec3 minCorner(std::numeric_limits<float>::max());
        glm::vec3 maxCorner(std::numeric_limits<float>::lowest());

        for (const auto& p : m_positions)
        {
            minCorner = glm::min(minCorner, p);
            maxCorner = glm::max(maxCorner, p);
        }

        float longestAxis = std::max(maxCorner.x - minCorner.x, std::max(maxCorner.y - minCorner.y, maxCorner.z - minCorner.z));
        float scalingFactor = 1.0f / longestAxis;

        glm::vec3 center = (minCorner + maxCorner) * 0.5f;

        for (auto& p : m_positions)
        {
            p -= center;
            p *= scalingFactor;
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

        for (auto& b : m_bitangents)
            b = glm::vec3(invTransp * glm::vec4(b, 0.0f));
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

    void TriangleMesh::setParts(std::vector<GeometryPart>&& parts)
    {
        m_parts = std::move(parts);
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
}