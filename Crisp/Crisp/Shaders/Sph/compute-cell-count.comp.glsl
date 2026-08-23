#version 460 core

// Input: particle positions
layout(std430, set = 0, binding = 0) buffer Positions {
    vec4 positions[];
};

// Output: number of particles in each cell
layout(std430, set = 0, binding = 1) buffer CellCounts {
    uint cellCounts[];
};

// Output: intra-cell index of a particle
layout(std430, set = 0, binding = 2) buffer CellIds {
    uint cellIds[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

uvec3 calculateGridPosition(vec3 position, float cellSize, uvec3 gridDims) {
    return min(uvec3(max(position, vec3(0.0f)) / cellSize), gridDims - uvec3(1));
}

uint getGridLinearIndex(uvec3 gridPosition, uvec3 gridDims) {
    return gridPosition.z * gridDims.x * gridDims.y + gridPosition.y * gridDims.x + gridPosition.x;
}

uint getGlobalIndex() {
    return getGridLinearIndex(gl_GlobalInvocationID, gl_WorkGroupSize * gl_NumWorkGroups);
}

layout(push_constant) uniform PushConstant {
    layout(offset = 0) uvec3 dim;
    layout(offset = 12) float cellSize;
    layout(offset = 16) uint numParticles;
};

void main() {
    uint threadIdx = getGlobalIndex();
    if (threadIdx >= numParticles) {
        return;
    }

    vec3 particlePosition = positions[threadIdx].xyz;
    uvec3 gridPosition = calculateGridPosition(particlePosition, cellSize, dim);
    uint linearGridIdx = getGridLinearIndex(gridPosition, dim);

    uint intraCellIdx = atomicAdd(cellCounts[linearGridIdx], 1);
    cellIds[threadIdx] = intraCellIdx;
}