#pragma once

#include <cstdint>

#include <Crisp/Mesh/TriangleMesh.hpp>

namespace crisp {

struct MeshOptimizationStats {
    uint32_t vertexCountBefore{0};
    uint32_t vertexCountAfter{0};
    uint32_t indexCount{0};

    // Transformed vertices per triangle. Best case 0.5, worst case 3.0.
    float acmrBefore{0.0f};
    float acmrAfter{0.0f};

    // Transformed vertices per vertex. 1.0 means every vertex is transformed exactly once.
    float atvrBefore{0.0f};
    float atvrAfter{0.0f};

    // Fetched bytes over vertex buffer size. 1.0 means every byte is fetched exactly once.
    float overfetchBefore{0.0f};
    float overfetchAfter{0.0f};

    void accumulate(const MeshOptimizationStats& other);
};

// Reorders a mesh for rendering: duplicate vertices are welded, then triangles are ordered for the post-transform
// vertex cache and for reduced overdraw, and finally the vertices themselves are reordered so that a draw walks
// the vertex buffer close to front to back.
MeshOptimizationStats optimizeMeshIndices(TriangleMesh& mesh);

} // namespace crisp
