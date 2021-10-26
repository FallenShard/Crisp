#version 450 core

layout(set = 0, binding = 0) buffer Positions
{
    vec4 positions[];
};

layout(set = 0, binding = 1) buffer Deltas
{
    vec4 deltas[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

uint getGlobalIndex()
{
    uvec3 dim = gl_WorkGroupSize * gl_NumWorkGroups;
    return gl_GlobalInvocationID.z * dim.x * dim.y +
           gl_GlobalInvocationID.y * dim.x +
           gl_GlobalInvocationID.x;
}

layout(push_constant) uniform PushConstant
{
    layout(offset = 0) uint numParticles;
} pushConst;

void main()
{
    uint threadIdx = getGlobalIndex();
    if (threadIdx >= pushConst.numParticles)
        return;

    positions[threadIdx] += deltas[threadIdx];
}