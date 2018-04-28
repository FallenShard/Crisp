#version 450 core

layout(set = 0, binding = 0) buffer Positions
{
    vec4 positions[];
};

layout(set = 0, binding = 1) buffer CellCounts
{
    uint cellCounts[];
};

layout(set = 0, binding = 2) buffer CellIds
{
    uint cellIds[];
};

layout(set = 0, binding = 3) buffer ReorderedIndices
{
    uint reorderedIndices[];
};

layout(set = 0, binding = 4) buffer TempPos
{
    vec4 tempPos[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

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
    uvec3 dim = gl_WorkGroupSize * gl_NumWorkGroups;
    return gl_GlobalInvocationID.z * dim.x * dim.y +
           gl_GlobalInvocationID.y * dim.x +
           gl_GlobalInvocationID.x;
}

layout(push_constant) uniform PushConstant
{
    layout(offset = 0)  uvec3 dim;
    layout(offset = 12) uint  numCells;
    layout(offset = 16) vec3  spaceSize;
    layout(offset = 28) float cellSize;
    layout(offset = 32) uint numParticles;
} grid;

void main()
{
    uint threadIdx = getGlobalIndex();
    if (threadIdx >= grid.numParticles)
        return;

    vec3 particlePosition = positions[threadIdx].xyz;
    uvec3 gridPosition = calculateGridPosition(particlePosition, grid.cellSize);
    uint linearGridIdx = getGridLinearIndex(gridPosition, grid.dim);

    uint cellOffset = cellCounts[linearGridIdx];
    uint intraCellOffset = cellIds[threadIdx];
    reorderedIndices[cellOffset + intraCellOffset] = threadIdx;
    tempPos[cellOffset + intraCellOffset] = vec4(particlePosition, 1.0f);
}