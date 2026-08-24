#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "../Common/particle-grid.part.glsl"

layout(std430, set = 0, binding = 0) buffer Positions {
    vec4 positions[];
};

layout(std430, set = 0, binding = 1) buffer CellCounts {
    uint cellCounts[];
};

layout(std430, set = 0, binding = 2) buffer CellIds {
    uint cellIds[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

// Must match CellCountPushConstants in Crisp/Crisp/Models/SPH.cpp, field for field. Nothing checks it.
layout(push_constant) uniform PushConstant {
    uvec3 dim;
    float cellSize;
    uint numParticles;
} pc;

void main() {
    uint i = particleGlobalIndex();
    if (i >= pc.numParticles) {
        return;
    }

    uvec3 gridPosition = particleGridPosition(positions[i].xyz, pc.cellSize, pc.dim);
    uint linearGridIdx = particleGridLinearIndex(gridPosition, pc.dim);

    cellIds[i] = atomicAdd(cellCounts[linearGridIdx], 1);
}
