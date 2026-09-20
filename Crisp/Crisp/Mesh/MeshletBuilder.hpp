#pragma once

#include <cstdint>
#include <vector>

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Mesh/Meshlet.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>

namespace crisp {

struct MeshletBounds {
    glm::vec4 centerRadius{0.0f};   // xyz: bounding sphere center, w: its radius.
    glm::vec4 coneApex{0.0f};       // xyz: normal cone apex, w: unused padding.
    glm::vec4 coneAxisCutoff{0.0f}; // xyz: normal cone axis, w: cos(angle / 2).
};

struct MeshletBuildOptions {
    uint32_t maxVertices{64};
    uint32_t maxTriangles{124};

    float coneWeight{0.25f};
};

struct MeshletGeometry {
    std::vector<Meshlet> meshlets;

    std::vector<MeshletBounds> bounds;

    std::vector<uint32_t> vertices;
    std::vector<uint8_t> triangles;

    uint32_t maxVertices{0};
    uint32_t maxTriangles{0};
};

MeshletGeometry buildMeshlets(const TriangleMesh& mesh, const MeshletBuildOptions& options = {});

bool isMeshletConeVisible(const MeshletBounds& bounds, const glm::vec3& cameraPosition);

} // namespace crisp
