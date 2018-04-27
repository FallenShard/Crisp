#pragma once

#include <vector>
#include <unordered_map>
#include "Math/Headers.hpp"

#include "Geometry/VertexAttributeTraits.hpp"

namespace crisp
{
    class TriangleMesh
    {
    public:
        TriangleMesh(std::string filename);
        ~TriangleMesh();

        const std::vector<float>& getAttributeBuffer(VertexAttribute attribute) const;
        const std::vector<glm::uvec3>& getFaces() const;
        uint32_t getNumFaces() const;
        uint32_t getNumVertices() const;

        std::string getMeshName() const;

    private:
        std::vector<glm::vec3> calculateVertexNormals(const std::vector<glm::vec3>& positions, const std::vector<glm::uvec3>& faces) const;

        uint32_t m_numVertices;
        std::unordered_map<VertexAttribute, std::vector<float>> m_vertexAttributes;
        std::vector<glm::uvec3> m_faces;

        std::string m_meshName;
    };
}