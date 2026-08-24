#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "sph.part.glsl"

layout(std430, set = 0, binding = 0) buffer Positions {
    vec4 positions[];
};

layout(std430, set = 0, binding = 1) buffer CellCounts {
    uint cellCounts[];
};

layout(std430, set = 0, binding = 2) buffer CellIds {
    uint cellIds[];
};

layout(std430, set = 0, binding = 3) buffer ReorderedIndices {
    uint reorderedIndices[];
};

layout(std430, set = 0, binding = 4) buffer TempPos {
    vec4 tempPos[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

// Must match ParticlePushConstants in Crisp/Crisp/Models/SPH.cpp, field for field. Nothing checks it.
layout(push_constant) uniform PushConstant {
    SphGridParams grid;
    uint numParticles;
}
pc;

void main() {
    uint i = particleGlobalIndex();
    if (i >= pc.numParticles) {
        return;
    }

    vec3 particlePosition = positions[i].xyz;
    uvec3 gridPosition = particleGridPosition(particlePosition, pc.grid.cellSize, pc.grid.dim);
    uint linearGridIdx = particleGridLinearIndex(gridPosition, pc.grid.dim);

    uint slot = cellCounts[linearGridIdx] + cellIds[i];
    reorderedIndices[slot] = i;
    tempPos[slot] = vec4(particlePosition, 1.0f);
}
