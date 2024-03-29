#pragma once

#include <Crisp/Mesh/TriangleMesh.hpp>

namespace crisp {
TriangleMesh createPlaneMesh(const std::vector<VertexAttributeDescriptor>& vertexAttributes, float size = 1.0f);
TriangleMesh createGridMesh(
    const std::vector<VertexAttributeDescriptor>& vertexAttributes, float size, int tessellation);
TriangleMesh createGrassBlade(const std::vector<VertexAttributeDescriptor>& vertexAttributes);
TriangleMesh createSphereMesh(const std::vector<VertexAttributeDescriptor>& vertexAttributes);
TriangleMesh createCubeMesh(const std::vector<VertexAttributeDescriptor>& vertexAttributes);
} // namespace crisp