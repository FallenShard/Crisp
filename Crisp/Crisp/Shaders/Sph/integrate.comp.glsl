#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "sph.part.glsl"

layout(std430, set = 0, binding = 0) buffer PrevPositions {
    vec4 prevPositions[];
};

layout(std430, set = 0, binding = 1) buffer PrevVelocities {
    vec4 prevVelocities[];
};

layout(std430, set = 0, binding = 2) buffer Forces {
    vec4 forces[];
};

layout(std430, set = 0, binding = 3) buffer Positions {
    vec4 positions[];
};

layout(std430, set = 0, binding = 4) buffer Velocities {
    vec4 velocities[];
};

layout(std430, set = 0, binding = 5) buffer Colors {
    vec4 colors[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

// Must match IntegratePushConstants in Crisp/Crisp/Models/SPH.cpp, field for field. Nothing checks it.
layout(push_constant) uniform PushConstant {
    SphGridParams grid;
    float timeDelta;
    uint numParticles;
}
pushConst;

const float particleRadius = 0.01f;

void main() {
    uint threadIdx = particleGlobalIndex();
    if (threadIdx >= pushConst.numParticles) {
        return;
    }

    vec3 fluidSpace = pushConst.grid.spaceSize;

    vec4 force = forces[threadIdx];
    vec3 a = force.xyz / force.w;
    vec3 newVelocity = prevVelocities[threadIdx].xyz + pushConst.timeDelta * a;
    vec3 newPosition = prevPositions[threadIdx].xyz + pushConst.timeDelta * newVelocity;

    const float damping = 0.5f;
    if (newPosition.x < particleRadius) {
        newPosition.x = particleRadius;
        newVelocity.x = damping * -newVelocity.x;
    }

    if (newPosition.x > fluidSpace.x - particleRadius) {
        newPosition.x = fluidSpace.x - particleRadius;
        newVelocity.x = damping * -newVelocity.x;
    }

    if (newPosition.y < particleRadius) {
        newPosition.y = particleRadius;
        newVelocity.y = damping * -newVelocity.y;
    }

    if (newPosition.y > fluidSpace.y - particleRadius) {
        newPosition.y = fluidSpace.y - particleRadius;
        newVelocity.y = damping * -newVelocity.y;
    }

    if (newPosition.z < particleRadius) {
        newPosition.z = particleRadius;
        newVelocity.z = damping * -newVelocity.z;
    }

    if (newPosition.z > fluidSpace.z - particleRadius) {
        newPosition.z = fluidSpace.z - particleRadius;
        newVelocity.z = damping * -newVelocity.z;
    }

    velocities[threadIdx] = vec4(newVelocity, 1.0f);
    positions[threadIdx] = vec4(newPosition, 1.0f);

    colors[threadIdx].xyz = vec3(force.w / 1000.0f);
}
