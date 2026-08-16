#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "Common/math-constants.part.glsl"

// One workgroup per line: all logN butterfly stages run in shared memory instead of logN dispatches.

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;
layout(constant_id = 3) const uint kMaxN = 512; // For shared memory sizing.
layout(constant_id = 4) const bool kApplyOriginShift = false;
layout(constant_id = 5) const bool kTransposed = false;

// Layered: one cascade per array slice, transformed by its own slice of the dispatch.
// Each texel carries two independent complex transforms, in .rg and .ba.
layout(set = 0, binding = 0, rgba32f) uniform readonly image2DArray srcImg;
layout(set = 0, binding = 1, rgba32f) uniform writeonly image2DArray dstImg;

layout(push_constant) uniform PushConstant {
    int N;
    int logN;
};

shared vec4 data[kMaxN];

vec2 compMul(vec2 z, vec2 w) {
    return vec2(z[0] * w[0] - z[1] * w[1], z[0] * w[1] + z[1] * w[0]);
}

vec4 compMulPair(vec4 z, vec2 w) {
    return vec4(compMul(z.xy, w), compMul(z.zw, w));
}

uint reverseBits(uint k, int bitCount) {
    uint r = 0;
    for (int i = 0; i < bitCount; ++i) {
        r = (r << 1) | (k & 1u);
        k >>= 1;
    }
    return r;
}

// The separable transform is the same maths along either axis, so only the indexing is swapped.
ivec3 texel(int layer, int line, int idx) {
    return kTransposed ? ivec3(line, idx, layer) : ivec3(idx, line, layer);
}

void main() {
    const int layer = int(gl_WorkGroupID.z);
    const int line = int(gl_WorkGroupID.y);
    const int tid = int(gl_LocalInvocationID.x);
    const int halfN = N / 2;

    data[tid] = imageLoad(srcImg, texel(layer, line, int(reverseBits(uint(tid), logN))));
    data[tid + halfN] = imageLoad(srcImg, texel(layer, line, int(reverseBits(uint(tid + halfN), logN))));
    barrier();

    for (int passIdx = 1; passIdx <= logN; ++passIdx) {
        const int p = passIdx - 1;
        const int m = 1 << passIdx;
        const int j = tid % (1 << p);
        const int leftIdx = m * (tid / (1 << p)) + j;
        const int rightIdx = leftIdx + m / 2;

        const float factor = p == 0 ? 1.0f / float(N) : 1.0f;
        const vec2 ww = vec2(cos(2.0f * PI / float(m) * float(j)), sin(2.0f * PI / float(m) * float(j)));
        const vec4 t = compMulPair(data[rightIdx] * factor, ww);
        const vec4 u = data[leftIdx] * factor;

        data[rightIdx] = u - t;
        data[leftIdx] = u + t;
        barrier();
    }

    const float lowerSign = kApplyOriginShift && ((line + tid) & 1) != 0 ? -1.0f : 1.0f;
    const float upperSign = kApplyOriginShift && ((line + tid + halfN) & 1) != 0 ? -1.0f : 1.0f;
    imageStore(dstImg, texel(layer, line, tid), data[tid] * lowerSign);
    imageStore(dstImg, texel(layer, line, tid + halfN), data[tid + halfN] * upperSign);
}
