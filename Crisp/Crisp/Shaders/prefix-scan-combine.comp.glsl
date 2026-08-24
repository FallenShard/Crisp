#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "Common/particle-grid.part.glsl"

layout(std430, set = 0, binding = 0) buffer CellCounts {
    uint cellCounts[];
};

layout(std430, set = 0, binding = 1) buffer BlockSums {
    uint blockSums[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(push_constant) uniform PushConstant {
    uint numCells;
    uint elementsPerBlock;
}
pushConst;

void main() {
    uint globalIdx = particleGlobalIndex();
    if (globalIdx >= pushConst.numCells) {
        return;
    }

    cellCounts[globalIdx] += blockSums[globalIdx / pushConst.elementsPerBlock];
}
