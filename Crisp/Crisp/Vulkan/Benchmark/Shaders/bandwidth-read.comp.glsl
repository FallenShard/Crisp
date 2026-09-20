#version 460 core

layout(local_size_x = 256) in;

layout(std430, set = 0, binding = 0) readonly buffer SrcBuffer {
    vec4 src[];
};

layout(std430, set = 0, binding = 1) writeonly buffer SinkBuffer {
    vec4 sink[];
};

layout(push_constant) uniform Params {
    uint elementCount;
}
params;

void main() {
    const uint stride = gl_NumWorkGroups.x * gl_WorkGroupSize.x;

    vec4 acc0 = vec4(0.0f);
    vec4 acc1 = vec4(0.0f);
    vec4 acc2 = vec4(0.0f);
    vec4 acc3 = vec4(0.0f);

    uint i = gl_GlobalInvocationID.x;
    for (; i + 3u * stride < params.elementCount; i += 4u * stride) {
        acc0 += src[i];
        acc1 += src[i + stride];
        acc2 += src[i + 2u * stride];
        acc3 += src[i + 3u * stride];
    }
    for (; i < params.elementCount; i += stride) {
        acc0 += src[i];
    }

    const vec4 total = acc0 + acc1 + acc2 + acc3;
    if (total.x == 1e30f) {
        sink[gl_GlobalInvocationID.x] = total;
    }
}
