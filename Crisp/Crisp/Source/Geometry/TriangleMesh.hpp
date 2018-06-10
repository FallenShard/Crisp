#pragma once

#include <vector>
#include <unordered_map>
#include "Math/Headers.hpp"

#include "Geometry/VertexAttributeTraits.hpp"

namespace crisp
{
    std::vector<glm::vec3> calculateVertexNormals(const std::vector<glm::vec3>& positions, const std::vector<glm::uvec3>& faces);
    std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>> calculateTangentVectors(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals, const std::vector<glm::vec2>& texCoords, const std::vector<glm::uvec3>& faces);

    class TriangleMesh
    {
    public:
        TriangleMesh(std::string filename);
        TriangleMesh(const std::string& filename, std::initializer_list<VertexAttribute> vertexAttributes);
        TriangleMesh(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals,
            const std::vector<glm::vec2>& texCoords, const std::vector<glm::uvec3>& faces, std::initializer_list<VertexAttribute> vertexAttributes);
        ~TriangleMesh();

        const std::vector<float>& getAttributeBuffer(VertexAttribute attribute) const;
        const std::vector<glm::uvec3>& getFaces() const;
        uint32_t getNumFaces() const;
        uint32_t getNumIndices() const;
        uint32_t getNumVertices() const;

        std::vector<float> interleaveAttributes() const;

        std::string getMeshName() const;

    private:
        void insertAttributes(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals,
            const std::vector<glm::vec2>& texCoords, const std::vector<glm::uvec3>& faces, const std::vector<VertexAttribute>& interleavedFormat);

        uint32_t m_numVertices;
        std::unordered_map<VertexAttribute, std::vector<float>> m_vertexAttributes;
        std::vector<glm::uvec3> m_faces;

        std::vector<VertexAttribute> m_interleavedFormat;

        std::string m_meshName;
    };
}