#version 460 core

layout(std430, set = 0, binding = 0) buffer Elements {
    uint elements[];
};

layout(std430, set = 0, binding = 1) buffer BlockSums {
    uint blockSums[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

shared uint temp[gl_WorkGroupSize.x * 2];

layout(push_constant) uniform PushConstant {
    int storeSumBlocks;
    uint elementCount;
}
pushConst;

uint getGlobalIndex() {
    uvec3 dim = gl_WorkGroupSize * gl_NumWorkGroups;
    return gl_GlobalInvocationID.z * dim.x * dim.y + gl_GlobalInvocationID.y * dim.x + gl_GlobalInvocationID.x;
}

void main() {
    uint globalIdx = getGlobalIndex();

    uint n = gl_WorkGroupSize.x * 2;
    uint localIdx = gl_LocalInvocationIndex;

    uint lo = 2 * globalIdx;
    uint hi = lo + 1;
    temp[2 * localIdx] = lo < pushConst.elementCount ? elements[lo] : 0;
    temp[2 * localIdx + 1] = hi < pushConst.elementCount ? elements[hi] : 0;

    uint offset = 1;
    for (uint i = n >> 1; i > 0; i >>= 1) {
        memoryBarrierShared();
        barrier();
        if (localIdx < i) {
            uint ai = offset * (2 * localIdx + 1) - 1;
            uint bi = offset * (2 * localIdx + 2) - 1;
            temp[bi] += temp[ai];
        }
        offset *= 2;
    }

    if (localIdx == 0) {
        if (pushConst.storeSumBlocks == 1) {
            blockSums[gl_WorkGroupID.x] = temp[n - 1];
        }
        temp[n - 1] = 0;
    }

    for (uint i = 1; i < n; i *= 2) {
        offset >>= 1;
        memoryBarrierShared();
        barrier();
        if (localIdx < i) {
            uint ai = offset * (2 * localIdx + 1) - 1;
            uint bi = offset * (2 * localIdx + 2) - 1;
            uint t = temp[ai];
            temp[ai] = temp[bi];
            temp[bi] += t;
        }
    }

    memoryBarrierShared();
    barrier();
    if (lo < pushConst.elementCount) {
        elements[lo] = temp[2 * localIdx];
    }
    if (hi < pushConst.elementCount) {
        elements[hi] = temp[2 * localIdx + 1];
    }
}
