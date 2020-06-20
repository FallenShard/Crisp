#include "MeshGenerators.hpp"

#include <CrispCore/Math/Constants.hpp>
#include <CrispCore/Log.hpp>

namespace crisp
{
    TriangleMesh createPlaneMesh(const std::vector<VertexAttributeDescriptor>& vertexAttributes, float size)
    {
        std::vector<glm::vec3> positions =
        {
            glm::vec3(-size, 0.0f, +size),
            glm::vec3(+size, 0.0f, +size),
            glm::vec3(+size, 0.0f, -size),
            glm::vec3(-size, 0.0f, -size)
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
            glm::vec2(1.0f, 0.0f),
            glm::vec2(1.0f, 1.0f),
            glm::vec2(0.0f, 1.0f)
        };

        std::vector<glm::uvec3> faces =
        {
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 2, 3)
        };

        return TriangleMesh(positions, normals, texCoords, faces, vertexAttributes);
    }

    TriangleMesh createGrassBlade(const std::vector<VertexAttributeDescriptor>& vertexAttributes)
    {
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

    TriangleMesh createSphereMesh(const std::vector<VertexAttributeDescriptor>& vertexAttributes)
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;
        std::vector<glm::vec3> tangents;
        std::vector<glm::vec3> bitangents;
        std::vector<glm::uvec3> faces;

        int numRings  = 64;
        int numSlices = 64;

        for (int j = 0; j <= numRings; j++)
        {
            float v = j / static_cast<float>(numRings);
            float phi  = glm::radians(90.0f - v * 180.0f);
            float pSin = std::sinf(phi);
            float pCos = std::cosf(phi);

            for (int i = 0; i <= numSlices; i++)
            {
                float u = i / static_cast<float>(numSlices);
                float theta = glm::radians(u * 360.0f);
                float tSin = std::sinf(theta);
                float tCos = std::cosf(theta);
                positions.emplace_back(tCos * pCos, pSin, tSin * pCos);
                normals.push_back(glm::normalize(positions.back()));
                texCoords.emplace_back(1.0f - u, 1.0f - v);

                if (j > 0 && i < numSlices)
                {
                    int prevCurr = (j - 1) * (numSlices + 1) + i;
                    int prevNext = (j - 1) * (numSlices + 1) + i + 1;
                    int curr = j * (numSlices + 1) + i;
                    int next = j * (numSlices + 1) + i + 1;

                    faces.emplace_back(prevCurr, next, curr);
                    faces.emplace_back(prevCurr, prevNext, next);
                }
            }
        }

        return TriangleMesh(positions, normals, texCoords, faces, vertexAttributes);
    }

    TriangleMesh createCubeMesh(const std::vector<VertexAttributeDescriptor>& vertexAttributes)
    {
        static std::vector<glm::vec3> positions =
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

        static std::vector<glm::vec3> normals =
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

        static std::vector<glm::vec2> texCoords =
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

        static std::vector<glm::uvec3> faces =
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