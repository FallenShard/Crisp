#version 460 core

layout(local_size_x = 256) in;

layout(std430, set = 0, binding = 0) writeonly buffer DstBuffer {
    vec4 dst[];
};

layout(push_constant) uniform Params {
    uint elementCount;
}
params;

void main() {
    const uint stride = gl_NumWorkGroups.x * gl_WorkGroupSize.x;
    const vec4 value = vec4(float(gl_GlobalInvocationID.x));
    for (uint i = gl_GlobalInvocationID.x; i < params.elementCount; i += stride) {
        dst[i] = value;
    }
}
