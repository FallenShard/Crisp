#version 450 core

// Input: particle positions
layout(set = 0, binding = 0) buffer Positions
{
    vec4 positions[];
};

// Output: number of particles in each cell
layout(set = 0, binding = 1) buffer CellCounts
{
    uint cellCounts[];
};

// Output: intra-cell index of a particle
layout(set = 0, binding = 2) buffer CellIds
{
    uint cellIds[];
};

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

uvec3 calculateGridPosition(vec3 position, float cellSize)
{
    uvec3 gridPosition;
    gridPosition.x = uint(position.x / cellSize);
    gridPosition.y = uint(position.y / cellSize);
    gridPosition.z = uint(position.z / cellSize);
    return gridPosition;
}

uint getGridLinearIndex(uvec3 gridPosition, uvec3 gridDims)
{
    return gridPosition.z * gridDims.x * gridDims.y +
           gridPosition.y * gridDims.x +
           gridPosition.x;
}

uint getGlobalIndex()
{
    return getGridLinearIndex(gl_GlobalInvocationID, gl_WorkGroupSize * gl_NumWorkGroups);
}

layout(push_constant) uniform PushConstant
{
    layout(offset = 0)  uvec3 dim;
    layout(offset = 12) float cellSize;
    layout(offset = 16) uint numParticles;
};

void main()
{
    uint threadIdx = getGlobalIndex();
    if (threadIdx >= numParticles)
        return;

    vec3 particlePosition = positions[threadIdx].xyz;
    uvec3 gridPosition = calculateGridPosition(particlePosition, cellSize);
    uint linearGridIdx = getGridLinearIndex(gridPosition, dim);

    uint intraCellIdx = atomicAdd(cellCounts[linearGridIdx], 1);
    cellIds[threadIdx] = intraCellIdx;
}