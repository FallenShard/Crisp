#include "TriangleMesh.hpp"

#include <Vesper/Shapes/MeshLoader.hpp>
#include <CrispCore/Log.hpp>

namespace crisp
{
    namespace
    {
        template <typename T>
        static void fillInterleaved(std::vector<float>& interleavedBuffer, const std::vector<T>& attribute, size_t vertexSize, size_t offset)
        {
            for (size_t i = 0; i < attribute.size(); i++)
            {
                memcpy(&interleavedBuffer[i * vertexSize + offset], &attribute[i], sizeof(T));
            }
        }

        static void fillInterleaved(std::vector<float>& interleavedBuffer, const std::vector<float>& attribute, size_t vertexComponents, size_t attribComponents, size_t offset)
        {
            for (size_t i = 0; i < interleavedBuffer.size() / vertexComponents; i++)
            {
                memcpy(&interleavedBuffer[i * vertexComponents + offset], &attribute[i * attribComponents], attribComponents * sizeof(float));
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

    TriangleMesh::TriangleMesh(const std::filesystem::path& path)
        : m_meshName(path.filename().stem().string())
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;

        MeshLoader meshLoader;
        meshLoader.load(path, positions, normals, texCoords, m_faces);
        m_numVertices = static_cast<uint32_t>(positions.size());

        if (normals.empty())
            normals = calculateVertexNormals(positions, m_faces);

        insertAttribute<VertexAttribute::Position>(m_vertexAttributes, positions);
        insertAttribute<VertexAttribute::Normal>(m_vertexAttributes, normals);
        insertAttribute<VertexAttribute::TexCoord>(m_vertexAttributes, texCoords);
    }

    TriangleMesh::TriangleMesh(const std::filesystem::path& path, std::initializer_list<VertexAttribute> vertexAttributes)
        : m_meshName(path.filename().stem().string())
        , m_interleavedFormat(vertexAttributes)
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;

        MeshLoader meshLoader;
        meshLoader.load(path, positions, normals, texCoords, m_faces);
        m_numVertices = static_cast<uint32_t>(positions.size());
        if (m_numVertices == 0)
        {
            logError("Positions could not be loaded for mesh file ", path);
            return;
        }

        insertAttributes(positions, normals, texCoords, m_faces, m_interleavedFormat);
    }

    TriangleMesh::TriangleMesh(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals, const std::vector<glm::vec2>& texCoords,
        const std::vector<glm::uvec3>& faces, std::initializer_list<VertexAttribute> vertexAttributes)
        : m_interleavedFormat(vertexAttributes)
    {
        m_faces = faces;
        m_numVertices = static_cast<uint32_t>(positions.size());
        insertAttributes(positions, normals, texCoords, m_faces, m_interleavedFormat);
    }

    TriangleMesh::~TriangleMesh()
    {
    }

    const std::vector<float>& TriangleMesh::getAttributeBuffer(VertexAttribute attribute) const
    {
        return m_vertexAttributes.at(attribute);
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
        return m_numVertices;
    }

    std::vector<float> TriangleMesh::interleaveAttributes() const
    {
        std::size_t numVertexComponents = 0;
        for (VertexAttribute attrib : m_interleavedFormat)
            numVertexComponents += getNumComponents(attrib);

        std::vector<float> interleavedBuffer(numVertexComponents * m_numVertices);

        std::size_t offset = 0;
        for (VertexAttribute attrib : m_interleavedFormat)
        {
            fillInterleaved(interleavedBuffer, m_vertexAttributes.at(attrib), numVertexComponents, getNumComponents(attrib), offset);
            offset += getNumComponents(attrib);
        }

        return interleavedBuffer;
    }

    std::string TriangleMesh::getMeshName() const
    {
        return m_meshName;
    }

    void TriangleMesh::insertAttributes(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals,
        const std::vector<glm::vec2>& texCoords, const std::vector<glm::uvec3>& faces, const std::vector<VertexAttribute>& interleavedFormat)
    {
        std::vector<glm::vec3> norms = normals;
        std::vector<glm::vec2> uvs = texCoords;
        std::vector<glm::vec3> tangents;
        std::vector<glm::vec3> bitangents;

        for (auto vertexAttrib : interleavedFormat)
        {
            if (vertexAttrib == VertexAttribute::Position)
            {
                insertAttribute<VertexAttribute::Position>(m_vertexAttributes, positions);
            }
            else if (vertexAttrib == VertexAttribute::Normal)
            {
                if (norms.empty())
                    norms = calculateVertexNormals(positions, m_faces);

                insertAttribute<VertexAttribute::Normal>(m_vertexAttributes, normals);
            }
            else if (vertexAttrib == VertexAttribute::TexCoord)
            {
                if (uvs.empty())
                    uvs.resize(positions.size(), glm::vec2(0.0f));

                insertAttribute<VertexAttribute::TexCoord>(m_vertexAttributes, texCoords);
            }
            else if (vertexAttrib == VertexAttribute::Tangent)
            {
                if (tangents.empty())
                    std::tie(tangents, bitangents) = calculateTangentVectors(positions, normals, texCoords, m_faces);

                insertAttribute<VertexAttribute::Tangent>(m_vertexAttributes, tangents);
            }
            else if (vertexAttrib == VertexAttribute::Bitangent)
            {
                if (bitangents.empty())
                    std::tie(tangents, bitangents) = calculateTangentVectors(positions, normals, texCoords, m_faces);

                insertAttribute<VertexAttribute::Bitangent>(m_vertexAttributes, bitangents);
            }
        }
    }

    std::vector<glm::vec3> calculateVertexNormals(const std::vector<glm::vec3>& positions, const std::vector<glm::uvec3>& faces)
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

    std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>> calculateTangentVectors(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals, const std::vector<glm::vec2>& texCoords, const std::vector<glm::uvec3>& faces)
    {
        std::vector<glm::vec3> tangents(positions.size(), glm::vec3(0.0f));
        std::vector<glm::vec3> bitangents(positions.size(), glm::vec3(0.0f));

        for (const auto& face : faces)
        {
            const glm::vec3& v1 = positions[face[0]];
            const glm::vec3& v2 = positions[face[1]];
            const glm::vec3& v3 = positions[face[2]];

            const glm::vec2& w1 = texCoords[face[0]];
            const glm::vec2& w2 = texCoords[face[1]];
            const glm::vec2& w3 = texCoords[face[2]];

            //glm::vec3 e1 = p1 - p0;
            //glm::vec3 e2 = p2 - p0;
            //
            //glm::vec2 s = tc1 - tc0;
            //glm::vec2 t = tc2 - tc0;
            //float r = 1.0f / (s.x * t.y - s.y * t.x);
            //
            //glm::vec3 sDir = glm::vec3(t.y * e1.x - t.x * e2.x, t.y * e1.y - t.x * e2.y,
            //    t.y * e1.z - t.x * e2.z) * r;
            //
            //glm::vec3 tDir = glm::vec3(s.x * e2.x - s.y * e1.x, s.x * e2.y - s.y * e1.y,
            //    s.x * e2.z - s.y * e1.z) * r;
            float x1 = v2.x - v1.x;
            float x2 = v3.x - v1.x;
            float y1 = v2.y - v1.y;
            float y2 = v3.y - v1.y;
            float z1 = v2.z - v1.z;
            float z2 = v3.z - v1.z;

            float s1 = w2.x - w1.x;
            float s2 = w3.x - w1.x;
            float t1 = w2.y - w1.y;
            float t2 = w3.y - w1.y;

            float r = 1.0F / (s1 * t2 - s2 * t1);
            glm::vec3 sDir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r,
                (t2 * z1 - t1 * z2) * r);
            glm::vec3 tDir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r,
                (s1 * z2 - s2 * z1) * r);

            tangents[face[0]]   += sDir;
            tangents[face[1]]   += sDir;
            tangents[face[2]]   += sDir;
            bitangents[face[0]] += tDir;
            bitangents[face[1]] += tDir;
            bitangents[face[2]] += tDir;
        }

        for (uint32_t i = 0; i < positions.size(); i++)
        {
            const glm::vec3& n = normals[i];
            const glm::vec3& t = tangents[i];

            tangents[i]   = glm::normalize(t - n * glm::dot(n, t));
            //bitangents[i] = glm::normalize(glm::cross(n, t));
        }

        return std::make_pair(tangents, bitangents);
    }
}