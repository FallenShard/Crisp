#version 460 core

layout(local_size_x = 256) in;

layout(std430, set = 0, binding = 0) writeonly buffer SinkBuffer {
    float sink[];
};

layout(push_constant) uniform Params {
    uint iterations;
} params;

const uint kAccumulators = 8;
const uint kFmasPerIteration = kAccumulators * 4;

void main() {
    const float seed = float(gl_GlobalInvocationID.x) * 1e-6f;

    float a0 = seed + 0.1f;
    float a1 = seed + 0.2f;
    float a2 = seed + 0.3f;
    float a3 = seed + 0.4f;
    float a4 = seed + 0.5f;
    float a5 = seed + 0.6f;
    float a6 = seed + 0.7f;
    float a7 = seed + 0.8f;

    const float m = 1.0000001f;
    const float b = 1e-7f;

    for (uint i = 0; i < params.iterations; ++i) {
        a0 = fma(a0, m, b);
        a1 = fma(a1, m, b);
        a2 = fma(a2, m, b);
        a3 = fma(a3, m, b);
        a4 = fma(a4, m, b);
        a5 = fma(a5, m, b);
        a6 = fma(a6, m, b);
        a7 = fma(a7, m, b);

        a0 = fma(a0, m, b);
        a1 = fma(a1, m, b);
        a2 = fma(a2, m, b);
        a3 = fma(a3, m, b);
        a4 = fma(a4, m, b);
        a5 = fma(a5, m, b);
        a6 = fma(a6, m, b);
        a7 = fma(a7, m, b);

        a0 = fma(a0, m, b);
        a1 = fma(a1, m, b);
        a2 = fma(a2, m, b);
        a3 = fma(a3, m, b);
        a4 = fma(a4, m, b);
        a5 = fma(a5, m, b);
        a6 = fma(a6, m, b);
        a7 = fma(a7, m, b);

        a0 = fma(a0, m, b);
        a1 = fma(a1, m, b);
        a2 = fma(a2, m, b);
        a3 = fma(a3, m, b);
        a4 = fma(a4, m, b);
        a5 = fma(a5, m, b);
        a6 = fma(a6, m, b);
        a7 = fma(a7, m, b);
    }

    const float total = a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7;
    if (total < 0.0f) {
        sink[gl_GlobalInvocationID.x] = total;
    }
}
