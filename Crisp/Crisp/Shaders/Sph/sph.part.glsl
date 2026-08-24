#ifndef CRISP_SPH_GLSL
#define CRISP_SPH_GLSL

#include "../Common/particle-grid.part.glsl"

// Must match GridPushConstants in Crisp/Crisp/Models/SPH.cpp, field for field. Nothing checks it.
struct SphGridParams {
    uvec3 dim;
    uint numCells;

    vec3 spaceSize;
    float cellSize;
};

ivec3 sphCellUnclamped(vec3 position, float cellSize) {
    return ivec3(position / cellSize);
}

#endif // CRISP_SPH_GLSL
