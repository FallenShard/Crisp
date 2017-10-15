#version 450 core

layout(set = 0, binding = 0) buffer GridCells
{
    uvec4 gridCells[];
};

layout(set = 0, binding = 1) buffer CellCounts
{
    uint cellCounts[];
};

layout (local_size_x = 2, local_size_y = 2, local_size_z = 2) in;

void main()
{
    uvec3 dim     = gl_WorkGroupSize * gl_NumWorkGroups;
    uvec3 indices = gl_WorkGroupSize * gl_WorkGroupID + gl_LocalInvocationID;
    uint idx = indices.y * dim.x * dim.z + indices.z * dim.x + indices.x;

    //gridCells[idx] = uvec4(0);
    cellCounts[idx] = 0;
}