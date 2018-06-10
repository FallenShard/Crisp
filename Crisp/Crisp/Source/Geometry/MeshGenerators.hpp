#pragma once

#include <vector>

#include "TriangleMesh.hpp"

namespace crisp
{
    TriangleMesh makePlaneMesh(std::initializer_list<VertexAttribute> vertexAttributes);
}