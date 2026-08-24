#ifndef CRISP_PARTICLE_GRID_GLSL
#define CRISP_PARTICLE_GRID_GLSL

uint particleGridLinearIndex(uvec3 gridPosition, uvec3 gridDims) {
    return gridPosition.z * gridDims.x * gridDims.y + gridPosition.y * gridDims.x + gridPosition.x;
}

uvec3 particleGridPosition(vec3 position, float cellSize, uvec3 gridDims) {
    return min(uvec3(max(position, vec3(0.0f)) / cellSize), gridDims - uvec3(1));
}

uint particleGlobalIndex() {
    return gl_GlobalInvocationID.x;
}

#endif // CRISP_PARTICLE_GRID_GLSL
