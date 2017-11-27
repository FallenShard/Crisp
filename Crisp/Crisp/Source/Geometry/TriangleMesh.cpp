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
    }

    TriangleMesh::TriangleMesh(std::string filename, std::vector<VertexAttribute> interleaved, std::vector<VertexAttribute> separate)
        : m_meshName(filename.substr(0, filename.length() - 4))
        , m_verticesByteSize(0)
        , m_indicesByteSize(0)
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;

        vesper::MeshLoader meshLoader;
        meshLoader.load(filename, positions, normals, texCoords, m_faces);

        if (normals.empty())
            normals = calculateVertexNormals(positions, m_faces);

        size_t interleavedVertexFloats = 0;
        for (auto& attrib : interleaved)
        {
            if (attrib == VertexAttribute::Position)
                interleavedVertexFloats += positions.empty() ? 0 : 3;
            else if (attrib == VertexAttribute::Normal)
                interleavedVertexFloats += normals.empty() ? 0 : 3;
            else if (attrib == VertexAttribute::TexCoord)
                interleavedVertexFloats += texCoords.empty() ? 0 : 2;
        }

        size_t numBuffers = interleaved.empty() ? 0 : 1;
        for (auto& separateAttrib : separate)
        {
            if (separateAttrib == VertexAttribute::Position && !positions.empty())
                numBuffers++;
            else if (separateAttrib == VertexAttribute::Normal && !normals.empty())
                numBuffers++;
            else if (separateAttrib == VertexAttribute::TexCoord && !texCoords.empty())
                numBuffers++;
        }

        m_vertices.resize(numBuffers);
        
        size_t currentBuffer = 0;
        if (!interleaved.empty())
        {
            m_vertices[currentBuffer].resize(interleavedVertexFloats * positions.size());

            size_t currentOffset = 0;
            for (auto& attrib : interleaved)
            {
                if (attrib == VertexAttribute::Position)
                {
                    fillInterleaved(m_vertices[currentBuffer], positions, interleavedVertexFloats, currentOffset);
                    currentOffset += 3;
                }
                else if (attrib == VertexAttribute::Normal)
                {
                    fillInterleaved(m_vertices[currentBuffer], normals, interleavedVertexFloats, currentOffset);
                    currentOffset += 3;
                }
                else if (attrib == VertexAttribute::TexCoord)
                {
                    fillInterleaved(m_vertices[currentBuffer], texCoords, interleavedVertexFloats, currentOffset);
                    currentOffset += 2;
                }
            }

            currentBuffer++;
        }

        for (auto& attrib : separate)
        {
            if (attrib == VertexAttribute::Position && !positions.empty())
                fill(m_vertices[currentBuffer++], positions);
            else if (attrib == VertexAttribute::Normal && !normals.empty())
                fill(m_vertices[currentBuffer++], normals);
            else if (attrib == VertexAttribute::TexCoord && !texCoords.empty())
                fill(m_vertices[currentBuffer++], texCoords);
        }

        for (auto& vertBuffer : m_vertices)
            m_verticesByteSize += vertBuffer.size() * sizeof(float);

        m_indicesByteSize += m_faces.size() * 3 * sizeof(unsigned int);
    }

    TriangleMesh::~TriangleMesh()
    {
    }

    const std::vector<float>& TriangleMesh::getBuffer(unsigned int index) const
    {
        return m_vertices.at(index);
    }

    const std::vector<glm::uvec3>& TriangleMesh::getFaces() const
    {
        return m_faces;
    }

    uint32_t TriangleMesh::getNumFaces() const
    {
        return static_cast<uint32_t>(m_faces.size());
    }

    size_t TriangleMesh::getVerticesByteSize() const
    {
        return m_verticesByteSize;
    }

    size_t TriangleMesh::getIndicesByteSize() const
    {
        return m_indicesByteSize;
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