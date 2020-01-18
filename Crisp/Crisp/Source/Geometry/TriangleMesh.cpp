#include "TriangleMesh.hpp"

#include <Vesper/Shapes/MeshLoader.hpp>
#include <CrispCore/Log.hpp>

#include "IO/WavefrontObjImporter.hpp"

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

        template <typename T>
        static void fill(std::vector<float>& buffer, const std::vector<T>& attribute)
        {
            buffer.resize(attribute.size() * sizeof(T) / sizeof(float));
            memcpy(buffer.data(), attribute.data(), buffer.size() * sizeof(float));
        }

        template <VertexAttribute attribute, typename T>
        void insertAttribute(std::unordered_map<VertexAttribute, std::vector<float>>& attributes, const std::vector<T>& attribData)
        {
            attributes[attribute] = std::vector<float>(attribData.size() * VertexAttributeTraits<attribute>::numComponents);
            auto& attribBuffer = attributes[attribute];
            memcpy(attribBuffer.data(), attribData.data(), attribData.size() * sizeof(T));
        }
    }

    TriangleMesh::TriangleMesh()
    {
    }

    TriangleMesh::TriangleMesh(const std::filesystem::path& path)
        : m_meshName(path.filename().stem().string())
        , m_interleavedFormat({ VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord })
    {
        if (WavefrontObjImporter::isWavefrontObjFile(path))
        {
            WavefrontObjImporter importer(path);
            importer.moveDataToTriangleMesh(*this, m_interleavedFormat);
            return;
        }

        MeshLoader meshLoader;
        meshLoader.load(path, m_positions, m_normals, m_texCoords, m_faces);

        if (m_normals.empty())
            computeVertexNormals();
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

        MeshLoader meshLoader;
        meshLoader.load(path, m_positions, m_normals, m_texCoords, m_faces);
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
            if (attrib.type == VertexAttribute::Tangent || attrib.type == VertexAttribute::Bitangent)
            {
                computeTangentVectors();
                break;
            }
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

    InterleavedVertexBuffer TriangleMesh::interleaveAttributes() const
    {
        InterleavedVertexBuffer interleavedBuffer;

        interleavedBuffer.vertexSize = 0;
        for (const auto& attrib : m_interleavedFormat)
            interleavedBuffer.vertexSize += attrib.size;

        uint32_t numVertices = getNumVertices();
        interleavedBuffer.buffer.resize(interleavedBuffer.vertexSize * numVertices);

        uint32_t currOffset = 0;
        for (const auto& attrib : m_interleavedFormat)
        {
            if (attrib.type == VertexAttribute::Position)
                fillInterleaved(interleavedBuffer.buffer, interleavedBuffer.vertexSize, currOffset, m_positions);
            else if (attrib.type == VertexAttribute::Normal)
                fillInterleaved(interleavedBuffer.buffer, interleavedBuffer.vertexSize, currOffset, m_normals);
            else if (attrib.type == VertexAttribute::TexCoord)
                fillInterleaved(interleavedBuffer.buffer, interleavedBuffer.vertexSize, currOffset, m_texCoords);
            else if (attrib.type == VertexAttribute::Tangent)
                fillInterleaved(interleavedBuffer.buffer, interleavedBuffer.vertexSize, currOffset, m_tangents);
            else if (attrib.type == VertexAttribute::Bitangent)
                fillInterleaved(interleavedBuffer.buffer, interleavedBuffer.vertexSize, currOffset, m_bitangents);
            else if (attrib.type == VertexAttribute::Custom)
            {
                auto& attribBuffer = m_customAttributes.at(attrib.name);
                fillInterleaved(interleavedBuffer.buffer, interleavedBuffer.vertexSize, currOffset, attribBuffer.buffer, attribBuffer.descriptor.size);
            }

            currOffset += attrib.size;
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
        m_tangents   = std::vector<glm::vec3>(m_positions.size(), glm::vec3(0.0f));
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
            glm::vec3 sDir = glm::vec3(t.y * e1.x - s.y * e2.x, t.y * e1.y - s.x * e2.y,
                t.y * e1.z - s.y * e2.z) * r;
            glm::vec3 tDir = glm::vec3(s.x * e2.x - t.x * e1.x, s.x * e2.y - t.x * e1.y,
                s.x * e2.z - t.x * e1.z) * r;

            m_tangents[face[0]]   += sDir;
            m_tangents[face[1]]   += sDir;
            m_tangents[face[2]]   += sDir;
            m_bitangents[face[0]] += tDir;
            m_bitangents[face[1]] += tDir;
            m_bitangents[face[2]] += tDir;
        }

        for (std::size_t i = 0; i < m_tangents.size(); ++i)
        {
            const glm::vec3& n = m_normals[i];
            const glm::vec3& t = m_tangents[i];

            m_tangents[i]   = glm::normalize(t - n * glm::dot(n, t));
            // handedness = glm::dot(glm::cross(n, t), m_bitangents[i]) < 0.0f ? -1.0f : 1.0f;

            m_bitangents[i] = glm::normalize(glm::cross(n, t));
        }
    }

    void TriangleMesh::computeVertexNormals()
    {
        m_normals = computeVertexNormals(m_positions, m_faces);
    }

    const std::vector<GeometryPart> TriangleMesh::getGeometryParts() const
    {
        return m_parts;
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

        for (const auto& p : m_positions)
            m_boundingBox.expandBy(p);
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