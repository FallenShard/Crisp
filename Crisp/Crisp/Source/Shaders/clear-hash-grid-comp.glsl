#version 450 core

// Output: all cell counts will be 0
layout(set = 0, binding = 0) buffer CellCounts
{
    uint cellCounts[];
};

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

uint getGlobalIndex()
{
    uvec3 dim = gl_WorkGroupSize * gl_NumWorkGroups;
    return gl_GlobalInvocationID.z * dim.x * dim.y +
           gl_GlobalInvocationID.y * dim.x +
           gl_GlobalInvocationID.x;
}

layout(push_constant) uniform PushConstant
{
    uint numCells;
};

void main()
{
    uint threadIdx = getGlobalIndex();
    if (threadIdx >= numCells)
        return;

    cellCounts[threadIdx] = 0;
}