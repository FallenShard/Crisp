#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "../Common/pbf.part.glsl"

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

layout(push_constant) uniform PushConstant {
    PbfParams pc;
};

void main() {
    uint i = pbfGlobalIndex();
    if (i >= pc.numParticles) {
        return;
    }

    uvec3 gridPosition = pbfGridPosition(positions[i].xyz, pc.cellSize, pc.gridDim);
    uint linearGridIdx = pbfGridLinearIndex(gridPosition, pc.gridDim);

    cellIds[i] = atomicAdd(cellCounts[linearGridIdx], 1);
}
