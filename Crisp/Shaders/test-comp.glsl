#version 450 core

layout(set = 0, binding = 0) buffer FluidData
{
    vec4 positions[];
};

layout(set = 0, binding = 1) buffer Colors
{
    vec4 colors[];
};

layout(set = 0, binding = 2) buffer GridCells
{
    uvec4 gridCells[];
};

layout(set = 0, binding = 3) buffer CellCounts
{
    uint cellCounts[];
};

layout(set = 0, binding = 5) buffer Pressures
{
    float pressures[];
};

layout(set = 0, binding = 4) uniform GridParams
{
    uvec3 dim;
    uint padding;
    vec3 spaceSize;
    float cellSize;
} gridParams;

layout (local_size_x = 2, local_size_y = 2, local_size_z = 2) in;

layout(push_constant) uniform PushConstant
{
	layout(offset = 0) float value;
} time;

const float particleRadius = 0.02f;

uint getUniqueIdx(uvec3 indices, uvec3 dim) {
    return indices.y * dim.z * dim.x + indices.z * dim.x + indices.x;
}

void main()
{
    uvec3 dim     = gl_WorkGroupSize * gl_NumWorkGroups;
    uvec3 indices = gl_WorkGroupSize * gl_WorkGroupID + gl_LocalInvocationID;
    uint index = indices.y * dim.x * dim.z + indices.z * dim.x + indices.x;
    float phase = float(indices.z + indices.x) / float(dim.z + dim.x) * 2.0f * 3.141592f;

    uvec3 gridDim = gridParams.dim;
    vec3 cell = positions[index].xyz / gridParams.cellSize;
    uvec3 gridIndices = max(uvec3(0), min(gridDim - uvec3(1), uvec3(cell)));

    uvec3 curr = gridIndices;
    uvec3 lowerFrontLeft = max(uvec3(0),           curr + uvec3(-1));
    uvec3 upperBackRight = min(gridDim - uvec3(1), curr + uvec3(+1));

    uvec3 diff = upperBackRight - lowerFrontLeft;

    if (pressures[index] > 0)
        colors[index] = vec4(0, 1, 0, 1);
    else
        colors[index] = vec4(1, 0, 0, 1);

    //uint nbrs = 0;
    //for (uint y = lowerFrontLeft.y; y <= upperBackRight.y; ++y)
    //{
    //    for (uint z = lowerFrontLeft.z; z <= upperBackRight.z; ++z)
    //    {
    //        for (uint x = lowerFrontLeft.x; x <= upperBackRight.x; ++x)
    //        {
    //            uint nbrCellIndex = getUniqueIdx(uvec3(x, y, z), gridParams.dim);
    //            nbrs += cellCounts[nbrCellIndex];
    //        }
    //    }
    //}
//
    //if (nbrs == 8)
    //    colors[index] = vec4(1, 0, 0, 1);
    //else if (nbrs == 12)
    //    colors[index] = vec4(1, 1, 0, 1);
    //else if (nbrs == 18)
    //    colors[index] = vec4(0, 1, 0, 1);
    //else
    //    colors[index] = vec4(0, 0, 1, 1);

    //positions[index].y  = indices.y * 0.04f;// + 0.1f * sin(5.0f * float(time.value) + 5.0f * phase);

    //if (all(equal(gl_WorkGroupID, uvec3(0, 0, 1)))) {
    //    colors[index] = vec4(1, 0, 0, 1);
    //}
    //else {
    //    colors[index] = vec4(0, 0.7, 1, 1);
    //}
}