#version 460 core

// Whole-column IFFT in shared memory: one workgroup per column, N/2 threads cooperating on that
// column. Replaces the separate bit-reverse + logN per-stage dispatches with one dispatch that
// loads the column bit-reversed, runs all logN butterfly stages with barrier() between them, and
// stores once.

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(set = 0, binding = 0, rg32f) uniform readonly image2D srcImg;
layout(set = 0, binding = 1, rg32f) uniform writeonly image2D dstImg;

layout(push_constant) uniform PushConstant {
    int N;
    int logN;
};

#define PI 3.1415926535897932384626433832795
#define MAX_N 512

shared vec2 data[MAX_N];

vec2 compMul(vec2 z, vec2 w) {
    return vec2(z[0] * w[0] - z[1] * w[1], z[0] * w[1] + z[1] * w[0]);
}

uint reverseBits(uint k, int bitCount) {
    uint r = 0;
    for (int i = 0; i < bitCount; ++i) {
        r = (r << 1) | (k & 1u);
        k >>= 1;
    }
    return r;
}

void main() {
    const int col = int(gl_WorkGroupID.x);
    const int tid = int(gl_LocalInvocationID.x);
    const int half_ = N / 2;

    data[tid] = imageLoad(srcImg, ivec2(col, int(reverseBits(uint(tid), logN)))).xy;
    data[tid + half_] = imageLoad(srcImg, ivec2(col, int(reverseBits(uint(tid + half_), logN)))).xy;
    barrier();

    for (int passIdx = 1; passIdx <= logN; ++passIdx) {
        const int p = passIdx - 1;
        const int m = 1 << passIdx;
        const int j = tid % (1 << p);
        const int leftIdx = m * (tid / (1 << p)) + j;
        const int rightIdx = leftIdx + m / 2;

        const float factor = p == 0 ? 1.0f / float(N) : 1.0f;
        const vec2 ww = vec2(cos(2.0f * PI / float(m) * float(j)), sin(2.0f * PI / float(m) * float(j)));
        const vec2 t = compMul(ww, data[rightIdx] * factor);
        const vec2 u = data[leftIdx] * factor;

        data[rightIdx] = u - t;
        data[leftIdx] = u + t;
        barrier();
    }

    imageStore(dstImg, ivec2(col, tid), vec4(data[tid], 0.0, 0.0));
    imageStore(dstImg, ivec2(col, tid + half_), vec4(data[tid + half_], 0.0, 0.0));
}
