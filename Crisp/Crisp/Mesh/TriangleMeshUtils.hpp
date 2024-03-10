#pragma once

#include <Crisp/Mesh/TriangleMesh.hpp>

namespace crisp {
TriangleMesh createPlaneMesh(float sizeX = 1.0f, float sizeZ = 1.0f);
TriangleMesh createGridMesh(float size, int tessellation);
TriangleMesh createCubeMesh();
TriangleMesh createSphereMesh();

TriangleMesh createGrassBlade();
} // namespace crisp