#include <Crisp/Mesh/TriangleMeshUtils.hpp>

#include <Crisp/Math/Constants.hpp>

namespace crisp {
TriangleMesh createPlaneMesh(const float sizeX, const float sizeZ) {
    std::vector<glm::vec3> positions = {
        glm::vec3(-sizeX, 0.0f, +sizeZ),
        glm::vec3(+sizeX, 0.0f, +sizeZ),
        glm::vec3(+sizeX, 0.0f, -sizeZ),
        glm::vec3(-sizeX, 0.0f, -sizeZ),
    };

    std::vector<glm::vec3> normals = {
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
    };

    std::vector<glm::vec2> texCoords = {
        glm::vec2(0.0f, 0.0f),
        glm::vec2(1.0f, 0.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec2(0.0f, 1.0f),
    };

    std::vector<glm::uvec3> faces = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(0, 2, 3),
    };

    return TriangleMesh{std::move(positions), std::move(normals), std::move(texCoords), std::move(faces)};
}

TriangleMesh createGridMesh(const float size, const int tessellation) {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::uvec3> faces;

    const float scale = size / static_cast<float>(tessellation);
    for (int32_t i = 0; i <= tessellation; ++i) {
        for (int32_t j = 0; j <= tessellation; ++j) {
            const float z = static_cast<float>(-i) * scale + size * 0.5f;
            const float x = static_cast<float>(+j) * scale - size * 0.5f;
            positions.emplace_back(x, 0.0f, z);
            normals.emplace_back(0.0f, 1.0f, 0.0f);
            texCoords.emplace_back(x, z);

            if (i < tessellation && j < tessellation) {
                const uint32_t curr = i * (tessellation + 1) + j;
                const uint32_t nextX = curr + 1;
                const uint32_t nextZ = curr + tessellation + 1;
                const uint32_t nextXZ = curr + tessellation + 2;

                faces.emplace_back(curr, nextX, nextXZ);
                faces.emplace_back(curr, nextXZ, nextZ);
            }
        }
    }

    return TriangleMesh{std::move(positions), std::move(normals), std::move(texCoords), std::move(faces)};
}

TriangleMesh createGrassBlade() {
    std::vector<glm::vec3> positions = {
        glm::vec3(-0.05f, 0.0f, +0.0f),
        glm::vec3(+0.05f, 0.0f, +0.0f),
        glm::vec3(-0.05f, 0.6f, -0.2f),
        glm::vec3(+0.05f, 0.6f, -0.2f),
        glm::vec3(-0.05f, 1.0f, -0.5f),
        glm::vec3(+0.05f, 1.0f, -0.5f),
    };

    std::vector<glm::vec3> normals = {
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
    };

    std::vector<glm::vec2> texCoords = {
        glm::vec2(0.0f, 0.0f),
        glm::vec2(1.0f, 0.0f),
        glm::vec2(0.0f, 0.5f),
        glm::vec2(1.0f, 0.5f),
        glm::vec2(0.0f, 1.0f),
        glm::vec2(1.0f, 1.0f),
    };

    std::vector<glm::uvec3> faces = {
        glm::uvec3(0, 1, 3),
        glm::uvec3(0, 3, 2),
        glm::uvec3(2, 3, 5),
        glm::uvec3(2, 5, 4),
    };

    return TriangleMesh{std::move(positions), std::move(normals), std::move(texCoords), std::move(faces)};
}

TriangleMesh createSphereMesh() {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::uvec3> faces;

    constexpr int numRings = 64;
    constexpr int numSlices = 64;

    for (int j = 0; j <= numRings; j++) {
        const float v = static_cast<float>(j) / static_cast<float>(numRings);
        const float phi = glm::radians(90.0f - v * 180.0f);
        const float pSin = std::sinf(phi);
        const float pCos = std::cosf(phi);

        for (int i = 0; i <= numSlices; i++) {
            const float u = static_cast<float>(i) / static_cast<float>(numSlices);
            const float theta = glm::radians(u * 360.0f);
            const float tSin = std::sinf(theta);
            const float tCos = std::cosf(theta);
            positions.emplace_back(tCos * pCos, pSin, tSin * pCos);
            normals.push_back(glm::normalize(positions.back()));
            texCoords.emplace_back(1.0f - u, 1.0f - v);

            if (j > 0 && i < numSlices) {
                const int prevCurr = (j - 1) * (numSlices + 1) + i;
                const int prevNext = (j - 1) * (numSlices + 1) + i + 1;
                const int curr = j * (numSlices + 1) + i;
                const int next = j * (numSlices + 1) + i + 1;

                faces.emplace_back(prevCurr, next, curr);
                faces.emplace_back(prevCurr, prevNext, next);
            }
        }
    }

    return TriangleMesh{std::move(positions), std::move(normals), std::move(texCoords), std::move(faces)};
}

TriangleMesh createCubeMesh() {
    std::vector<glm::vec3> positions = {
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
        glm::vec3(-0.5f, -0.5f, +0.5f),
    };

    std::vector<glm::vec3> normals = {
        glm::vec3(0.0f, 0.0f, 1.0f),  glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),  glm::vec3(0.0f, 0.0f, 1.0f),

        glm::vec3(1.0f, 0.0f, 0.0f),  glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),  glm::vec3(1.0f, 0.0f, 0.0f),

        glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, -1.0f),

        glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
        glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),

        glm::vec3(0.0f, 1.0f, 0.0f),  glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),  glm::vec3(0.0f, 1.0f, 0.0f),

        glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
    };

    std::vector<glm::vec2> texCoords = {
        glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f),

        glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f),

        glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f),

        glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f),

        glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f),

        glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f),
    };

    std::vector<glm::uvec3> faces = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(0, 2, 3),

        glm::uvec3(4, 5, 6),
        glm::uvec3(4, 6, 7),

        glm::uvec3(8, 9, 10),
        glm::uvec3(8, 10, 11),

        glm::uvec3(12, 13, 14),
        glm::uvec3(12, 14, 15),

        glm::uvec3(16, 17, 18),
        glm::uvec3(16, 18, 19),

        glm::uvec3(20, 21, 22),
        glm::uvec3(20, 22, 23),
    };

    return {std::move(positions), std::move(normals), std::move(texCoords), std::move(faces)};
}
} // namespace crisp