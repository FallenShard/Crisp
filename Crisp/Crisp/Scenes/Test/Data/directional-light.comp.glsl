#version 460 core
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "PathTracer/Lights/directional-light.part.glsl"

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct DirectionalLightResult {
    vec4 directionAndDistance;
    vec4 irradianceAndPdf;
};

layout(set = 0, binding = 0, scalar) writeonly buffer Results {
    DirectionalLightResult data[];
}
results;

layout(push_constant, scalar) uniform PushConstants {
    vec3 direction;
    float pad0;
    vec3 irradiance;
    float pad1;
}
pc;

void main() {
    const LightSample ls = sampleDirectionalLight(pc.direction, pc.irradiance);
    results.data[0].directionAndDistance = vec4(ls.direction, ls.distance);
    results.data[0].irradianceAndPdf = vec4(ls.weight, ls.pdf);
}
