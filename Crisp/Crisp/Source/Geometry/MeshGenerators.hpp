#pragma once

#include <vector>

#include "TriangleMesh.hpp"

namespace crisp
{
    TriangleMesh createPlaneMesh(const std::vector<VertexAttributeDescriptor>& vertexAttributes);
    TriangleMesh createGrassBlade(const std::vector<VertexAttributeDescriptor>& vertexAttributes);
    TriangleMesh createSphereMesh(const std::vector<VertexAttributeDescriptor>& vertexAttributes);
    TriangleMesh createCubeMesh(const std::vector<VertexAttributeDescriptor>& vertexAttributes);
}