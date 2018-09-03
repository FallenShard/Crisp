#include "MeshGenerators.hpp"

namespace crisp
{
    TriangleMesh createPlaneMesh(std::initializer_list<VertexAttribute> vertexAttributes)
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

    TriangleMesh createGrassBlade(std::initializer_list<VertexAttribute> vertexAttributes) {
        std::vector<glm::vec3> positions =
        {
            glm::vec3(-0.05f, 0.0f, +0.0f),
            glm::vec3(+0.05f, 0.0f, +0.0f),
            glm::vec3(-0.05f, 0.6f, -0.2f),
            glm::vec3(+0.05f, 0.6f, -0.2f),
            glm::vec3(-0.05f, 1.0f, -0.5f),
            glm::vec3(+0.05f, 1.0f, -0.5f)
        };

        std::vector<glm::vec3> normals =
        {
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        };

        std::vector<glm::vec2> texCoords =
        {
            glm::vec2(0.0f, 0.0f),
            glm::vec2(1.0f, 0.0f),
            glm::vec2(0.0f, 0.5f),
            glm::vec2(1.0f, 0.5f),
            glm::vec2(0.0f, 1.0f),
            glm::vec2(1.0f, 1.0f)
        };

        std::vector<glm::uvec3> faces =
        {
            glm::uvec3(0, 1, 3),
            glm::uvec3(0, 3, 2),
            glm::uvec3(2, 3, 5),
            glm::uvec3(2, 5, 4)
        };

        return TriangleMesh(positions, normals, texCoords, faces, vertexAttributes);
    }

    TriangleMesh createSphereMesh(std::initializer_list<VertexAttribute> vertexAttributes)
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;
        std::vector<glm::uvec3> faces;

        for (int i = 0; i < 100; i++)
        {
            float angle = glm::radians(i / 100.0f * 360.0f);
            float sin = std::sinf(angle);
            float cos = std::cosf(angle);
            positions.emplace_back(5.0f * cos, -2.0f, -5.0f * sin);
            positions.emplace_back(5.0f * cos, +2.0f, -5.0f * sin);

            normals.emplace_back(cos, 0.0f, -sin);
            normals.emplace_back(cos, 0.0f, -sin);

            float u = i % 2 == 0 ? 0.0f : 1.0f;

            texCoords.emplace_back(abs(cos), 0.0f);
            texCoords.emplace_back(abs(cos), 1.0f);


            uint32_t next = (i + 1) % 100;
            faces.emplace_back(2 * i, 2 * next, 2 * next + 1);
            faces.emplace_back(2 * i, 2 * next + 1, 2 * i + 1);
        }


        return TriangleMesh(positions, normals, texCoords, faces, vertexAttributes);
    }

    TriangleMesh createCubeMesh(std::initializer_list<VertexAttribute> vertexAttributes)
    {
        std::vector<glm::vec3> positions =
        {
            // Front
            glm::vec3(-0.5f, -0.5f, +0.5f),
            glm::vec3(+0.5f, -0.5f, +0.5f),
            glm::vec3(+0.5f, +0.5f, +0.5f),
            glm::vec3(-0.5f, +0.5f, +0.5f),

            // Right
            glm::vec3(+0.5f, -0.5f, +0.5f),
            glm::vec3(+0.5f, -0.5f, -0.5f),
            glm::vec3(+0.5f, +0.5f, -0.5f),
            glm::vec3(+0.5f, +0.5f, +0.5f),

            // Back
            glm::vec3(+0.5f, -0.5f, -0.5f),
            glm::vec3(-0.5f, -0.5f, -0.5f),
            glm::vec3(-0.5f, +0.5f, -0.5f),
            glm::vec3(+0.5f, +0.5f, -0.5f),

            // Left
            glm::vec3(-0.5f, -0.5f, -0.5f),
            glm::vec3(-0.5f, -0.5f, +0.5f),
            glm::vec3(-0.5f, +0.5f, +0.5f),
            glm::vec3(-0.5f, +0.5f, -0.5f),

            // Top
            glm::vec3(-0.5f, +0.5f, +0.5f),
            glm::vec3(+0.5f, +0.5f, +0.5f),
            glm::vec3(+0.5f, +0.5f, -0.5f),
            glm::vec3(-0.5f, +0.5f, -0.5f),

            // Bottom
            glm::vec3(-0.5f, -0.5f, -0.5f),
            glm::vec3(+0.5f, -0.5f, -0.5f),
            glm::vec3(+0.5f, -0.5f, +0.5f),
            glm::vec3(-0.5f, -0.5f, +0.5f)
        };

        std::vector<glm::vec3> normals =
        {
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),

            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),

            glm::vec3(0.0f, 0.0f, -1.0f),
            glm::vec3(0.0f, 0.0f, -1.0f),
            glm::vec3(0.0f, 0.0f, -1.0f),
            glm::vec3(0.0f, 0.0f, -1.0f),

            glm::vec3(-1.0f, 0.0f, 0.0f),
            glm::vec3(-1.0f, 0.0f, 0.0f),
            glm::vec3(-1.0f, 0.0f, 0.0f),
            glm::vec3(-1.0f, 0.0f, 0.0f),

            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),

            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f)
        };

        std::vector<glm::vec2> texCoords =
        {
            glm::vec2(0.0f, 0.0f),
            glm::vec2(0.0f, 1.0f),
            glm::vec2(1.0f, 1.0f),
            glm::vec2(1.0f, 0.0f),

            glm::vec2(0.0f, 0.0f),
            glm::vec2(0.0f, 1.0f),
            glm::vec2(1.0f, 1.0f),
            glm::vec2(1.0f, 0.0f),

            glm::vec2(0.0f, 0.0f),
            glm::vec2(0.0f, 1.0f),
            glm::vec2(1.0f, 1.0f),
            glm::vec2(1.0f, 0.0f),

            glm::vec2(0.0f, 0.0f),
            glm::vec2(0.0f, 1.0f),
            glm::vec2(1.0f, 1.0f),
            glm::vec2(1.0f, 0.0f),

            glm::vec2(0.0f, 0.0f),
            glm::vec2(0.0f, 1.0f),
            glm::vec2(1.0f, 1.0f),
            glm::vec2(1.0f, 0.0f),

            glm::vec2(0.0f, 0.0f),
            glm::vec2(0.0f, 1.0f),
            glm::vec2(1.0f, 1.0f),
            glm::vec2(1.0f, 0.0f),
        };

        std::vector<glm::uvec3> faces =
        {
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 2, 3),

            glm::uvec3(4, 5, 6),
            glm::uvec3(4, 6, 7),

            glm::uvec3(8,  9, 10),
            glm::uvec3(8, 10, 11),

            glm::uvec3(12, 13, 14),
            glm::uvec3(12, 14, 15),

            glm::uvec3(16, 17, 18),
            glm::uvec3(16, 18, 19),

            glm::uvec3(20, 21, 22),
            glm::uvec3(20, 22, 23),
        };

        return TriangleMesh(positions, normals, texCoords, faces, vertexAttributes);
    }
}