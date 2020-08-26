#version 450 core

layout(set = 0, binding = 0) buffer CellCounts
{
    uint cellCounts[];
};

layout(set = 0, binding = 1) buffer BlockSums
{
    uint blockSums[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(push_constant) uniform PushConstant
{
    uint numCells;
} pushConst;

uint getGlobalIndex()
{
    uvec3 dim = gl_WorkGroupSize * gl_NumWorkGroups;
    return gl_GlobalInvocationID.z * dim.x * dim.y +
           gl_GlobalInvocationID.y * dim.x +
           gl_GlobalInvocationID.x;
}

void main()
{
    uint globalIdx = getGlobalIndex();
    if (globalIdx >= pushConst.numCells)
        return;

    cellCounts[globalIdx] += blockSums[gl_WorkGroupID.x];
}