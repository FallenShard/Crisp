#version 450 core

#extension GL_GOOGLE_include_directive : require

#include "../Common/pbf.part.glsl"

layout(std430, set = 0, binding = 0) buffer Positions {
    vec4 positions[];
};

layout(std430, set = 0, binding = 1) buffer Velocities {
    vec4 velocities[];
};

layout(std430, set = 0, binding = 2) buffer PredictedPositions {
    vec4 predictedPositions[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(push_constant) uniform PushConstant {
    PbfParams pc;
};

void main() {
    uint i = pbfGlobalIndex();
    if (i >= pc.numParticles) {
        return;
    }

    vec3 v = velocities[i].xyz + pc.dt * pc.gravity;
    velocities[i] = vec4(v, 0.0f);
    predictedPositions[i] = vec4(positions[i].xyz + pc.dt * v, 1.0f);
}
