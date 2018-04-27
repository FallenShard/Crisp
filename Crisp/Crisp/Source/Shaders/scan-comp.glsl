#version 450 core

layout(set = 0, binding = 0) buffer CellCounts
{
    uint cellCounts[];
};

layout(set = 0, binding = 1) buffer BlockSums
{
    uint blockSums[];
};

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

shared uint temp[gl_WorkGroupSize.x * 2];

layout(push_constant) uniform PushConstant
{
    int storeSumBlocks;
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

    uint n = gl_WorkGroupSize.x * 2;
    uint localIdx = gl_LocalInvocationIndex;

    temp[2 * localIdx]     = 2 * localIdx     < n ? cellCounts[2 * globalIdx]     : 0;
    temp[2 * localIdx + 1] = 2 * localIdx + 1 < n ? cellCounts[2 * globalIdx + 1] : 0;

    uint offset = 1;
    for (uint i = n >> 1; i > 0; i >>= 1)
    {
        memoryBarrierShared();
        barrier();
        if (localIdx < i)
        {
            uint ai = offset * (2 * localIdx + 1) - 1;
            uint bi = offset * (2 * localIdx + 2) - 1;
            temp[bi] += temp[ai];
        }
        offset *= 2;
    }

    if (localIdx == 0)
    {
        if (pushConst.storeSumBlocks == 1)
            blockSums[gl_WorkGroupID.x] = temp[n - 1];
        temp[n - 1] = 0;
    }
        
    for (uint i = 1; i < n; i *= 2)
    {
        offset >>= 1;
        memoryBarrierShared();
        barrier();
        if (localIdx < i)
        {
            uint ai = offset * (2 * localIdx + 1) - 1;
            uint bi = offset * (2 * localIdx + 2) - 1;
            uint t = temp[ai];
            temp[ai] = temp[bi];
            temp[bi] += t;
        }
    }

    memoryBarrierShared();
    barrier();
    cellCounts[2 * globalIdx]     = temp[2 * localIdx];
    cellCounts[2 * globalIdx + 1] = temp[2 * localIdx + 1];
}