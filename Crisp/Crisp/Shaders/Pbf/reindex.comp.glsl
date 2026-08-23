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

layout(std430, set = 0, binding = 3) buffer SortedIndices {
    uint sortedIndices[];
};

layout(std430, set = 0, binding = 4) buffer SortedPositions {
    vec4 sortedPositions[];
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

    vec3 position = positions[i].xyz;
    uvec3 gridPosition = pbfGridPosition(position, pc.cellSize, pc.gridDim);
    uint linearGridIdx = pbfGridLinearIndex(gridPosition, pc.gridDim);

    uint slot = cellCounts[linearGridIdx] + cellIds[i];
    sortedIndices[slot] = i;
    sortedPositions[slot] = vec4(position, 1.0f);
}
