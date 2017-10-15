#version 450 core

layout(set = 0, binding = 0) buffer FluidData
{
    vec4 positions[];
};

layout(set = 0, binding = 1) buffer GridCells
{
    uvec4 gridCells[];
};

layout(set = 0, binding = 2) buffer CellCounts
{
    uint cellCounts[];
};

layout(set = 0, binding = 3) uniform GridParams
{
    uvec3 dim;
    uint padding;
    vec3 spaceSize;
    float cellSize;
} gridParams;

layout (local_size_x = 2, local_size_y = 2, local_size_z = 2) in;

uint getUniqueIdx(uvec3 indices, uvec3 dim) {
    return indices.y * dim.z * dim.x + indices.z * dim.x + indices.x;
}

void main()
{
    uvec3 dim     = gl_WorkGroupSize * gl_NumWorkGroups;
    uvec3 indices = gl_WorkGroupSize * gl_WorkGroupID + gl_LocalInvocationID;
    uint particleIdx = indices.y * dim.x * dim.z + indices.z * dim.x + indices.x;

    uvec3 gridDim = gridParams.dim;
    vec3 cell = positions[particleIdx].xyz / gridParams.cellSize;
    uvec3 gridIndices = max(uvec3(0), min(gridDim - uvec3(1), uvec3(cell)));
    uint gridIndex = gridIndices.y * gridDim.z * gridDim.x + gridIndices.z * gridDim.x + gridIndices.x;

    uint subIndex = atomicAdd(cellCounts[gridIndex], 1);
    gridCells[gridIndex][subIndex] = particleIdx;
}