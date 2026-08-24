#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "pbf.part.glsl"

layout(std430, set = 0, binding = 0) buffer CellCounts {
    uint cellCounts[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(push_constant) uniform PushConstant {
    PbfParams pc;
};

void main() {
    uint i = particleGlobalIndex();
    if (i >= pc.numCells) {
        return;
    }

    cellCounts[i] = 0;
}
