#include "MeshGenerators.hpp"

namespace crisp
{
    TriangleMesh makePlaneMesh(std::initializer_list<VertexAttribute> vertexAttributes)
    {
        std::vector<glm::vec3> positions =
        {
            glm::vec3(-1.0f, 0.0f, +1.0f),
            glm::vec3(+1.0f, 0.0f, +1.0f),
            glm::vec3(+1.0f, 0.0f, -1.0f),
            glm::vec3(-1.0f, 0.0f, -1.0f)
        };

        std::vector<glm::vec3> normals =
        {
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        };

        std::vector<glm::vec2> texCoords =
        {
            glm::vec2(0.0f, 0.0f),
            glm::vec2(0.0f, 1.0f),
            glm::vec2(1.0f, 1.0f),
            glm::vec2(1.0f, 0.0f)
        };

        std::vector<glm::uvec3> faces =
        {
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 2, 3)
        };

        return TriangleMesh(positions, normals, texCoords, faces, vertexAttributes);
    }
}