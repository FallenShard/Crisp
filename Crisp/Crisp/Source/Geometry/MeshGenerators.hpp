#pragma once

#include <vector>

#include "TriangleMesh.hpp"

namespace crisp
{
    TriangleMesh createPlaneMesh(std::initializer_list<VertexAttribute> vertexAttributes);
    TriangleMesh createGrassBlade(std::initializer_list<VertexAttribute> vertexAttributes);
    TriangleMesh createSphereMesh(std::initializer_list<VertexAttribute> vertexAttributes);
    TriangleMesh createCubeMesh(std::initializer_list<VertexAttribute> vertexAttributes);
}