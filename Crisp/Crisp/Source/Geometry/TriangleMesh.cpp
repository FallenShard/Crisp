#include "TriangleMesh.hpp"

#include <Vesper/Shapes/MeshLoader.hpp>

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

    TriangleMesh::TriangleMesh(std::string filename)
        : m_meshName(filename.substr(0, filename.length() - 4))
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;

        vesper::MeshLoader meshLoader;
        meshLoader.load(filename, positions, normals, texCoords, m_faces);
        m_numVertices = static_cast<uint32_t>(positions.size());

        if (normals.empty())
            normals = calculateVertexNormals(positions, m_faces);

        insertAttribute<VertexAttribute::Position>(m_vertexAttributes, positions);
        insertAttribute<VertexAttribute::Normal>(m_vertexAttributes, normals);
        insertAttribute<VertexAttribute::TexCoord>(m_vertexAttributes, texCoords);
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

    uint32_t TriangleMesh::getNumVertices() const
    {
        return m_numVertices;
    }

    std::string TriangleMesh::getMeshName() const
    {
        return m_meshName;
    }

    std::vector<glm::vec3> TriangleMesh::calculateVertexNormals(const std::vector<glm::vec3>& positions, const std::vector<glm::uvec3>& faces) const
    {
        std::vector<glm::vec3> normals(positions.size());

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
}