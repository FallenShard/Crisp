#version 450 core

layout(set = 0, binding = 0) buffer PrevPositions
{
    vec4 prevPositions[];
};

layout(set = 0, binding = 1) buffer PrevVelocities
{
    vec4 prevVelocities[];
};

layout(set = 0, binding = 2) buffer Forces
{
    vec4 forces[];
};

layout(set = 0, binding = 3) buffer Positions
{
    vec4 positions[];
};

layout(set = 0, binding = 4) buffer Velocities
{
    vec4 velocities[];
};

layout(set = 0, binding = 5) buffer Colors
{
    vec4 colors[];
};

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(push_constant) uniform PushConstant
{
    layout(offset = 0)  uvec3 dim;
    layout(offset = 12) uint  numCells;
    layout(offset = 16) vec3  spaceSize;
    layout(offset = 28) float cellSize;
    layout(offset = 32) float timeDelta;
    layout(offset = 36) uint numParticles;
} pushConst;


uint getGlobalIndex()
{
    uvec3 dim = gl_WorkGroupSize * gl_NumWorkGroups;
    return gl_GlobalInvocationID.z * dim.x * dim.y +
           gl_GlobalInvocationID.y * dim.x +
           gl_GlobalInvocationID.x;
}

const float particleRadius = 0.01f;
layout (constant_id = 0) const float timeStep = 0;

void main()
{
    uint threadIdx = getGlobalIndex();
    if (threadIdx >= pushConst.numParticles)
        return;

    vec3 fluidSpace = pushConst.spaceSize;

    vec4 force = forces[threadIdx];
    vec3 a = force.xyz / force.w;
    vec3 newVelocity = prevVelocities[threadIdx].xyz + pushConst.timeDelta * a;
    vec3 newPosition = prevPositions[threadIdx].xyz  + pushConst.timeDelta * newVelocity;

    const float damping = 0.5f;
    if (newPosition.x < particleRadius)
    {
        newPosition.x = particleRadius;
        newVelocity.x = damping * -newVelocity.x;
    }

    if (newPosition.x > fluidSpace.x - particleRadius)
    {
        newPosition.x = fluidSpace.x - particleRadius;
        newVelocity.x = damping * -newVelocity.x;
    }

    if (newPosition.y < particleRadius)
    {
        newPosition.y = particleRadius;
        newVelocity.y = damping * -newVelocity.y;
    }

    if (newPosition.y > fluidSpace.y - particleRadius)
    {
        newPosition.y = fluidSpace.y - particleRadius;
        newVelocity.y = damping * -newVelocity.y;
    }

    if (newPosition.z < particleRadius)
    {
        newPosition.z = particleRadius;
        newVelocity.z = damping * -newVelocity.z;
    }

    if (newPosition.z > fluidSpace.z - particleRadius)
    {
        newPosition.z = fluidSpace.z - particleRadius;
        newVelocity.z = damping * -newVelocity.z;
    }

    velocities[threadIdx] = vec4(newVelocity, 1.0f);
    positions[threadIdx] = vec4(newPosition, 1.0f);

    colors[threadIdx].xyz = vec3(force.w / 1000.0f);
}