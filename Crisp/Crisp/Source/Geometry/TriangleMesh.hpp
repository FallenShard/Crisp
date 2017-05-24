#pragma once

#include <vector>
#include "Math/Headers.hpp"

namespace crisp
{
    enum class VertexAttribute
    {
        Position,
        Normal,
        TexCoord
    };

    class TriangleMesh
    {
    public:
        TriangleMesh(std::string filename, std::vector<VertexAttribute> interleaved, std::vector<VertexAttribute> separate = std::vector<VertexAttribute>());
        ~TriangleMesh();

        const std::vector<float>& getBuffer(unsigned int index = 0) const;
        const std::vector<glm::uvec3>& getFaces() const;
        uint32_t getNumFaces() const;

        size_t getVerticesByteSize() const;
        size_t getIndicesByteSize() const;

        std::string getMeshName() const;


    private:
        std::vector<std::vector<float>> m_vertices;
        std::vector<glm::uvec3> m_faces;

        size_t m_verticesByteSize;
        size_t m_indicesByteSize;

        std::string m_meshName;
    };
}