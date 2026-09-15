#version 460 core
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "Common/math-constants.part.glsl"
#include "PathTracer/Lights/point-light.part.glsl"

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct PointLightResult {
    vec4 directionAndDistance;
    vec4 radianceAndPdf;
};

layout(set = 0, binding = 0, scalar) writeonly buffer Results {
    PointLightResult data[];
}
results;

layout(push_constant, scalar) uniform PushConstants {
    vec3 position;
    float pad0;
    vec3 power;
    float pad1;
    vec3 reference;
    float pad2;
}
pc;

void main() {
    const LightSample ls = samplePointLight(pc.position, pc.power, pc.reference);
    results.data[0].directionAndDistance = vec4(ls.direction, ls.distance);
    results.data[0].radianceAndPdf = vec4(ls.weight, ls.pdf);

    const LightSample singular = samplePointLight(pc.position, pc.power, pc.position);
    results.data[1].directionAndDistance = vec4(singular.direction, singular.distance);
    results.data[1].radianceAndPdf = vec4(singular.weight, singular.pdf);
}
