#version 460 core
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 localPos;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 0) uniform sampler2D equirectangularMap;

#include "../Common/math-constants.part.glsl"

// Crisp's equirect convention: u = 0.5 looks down -Z, u increases turning right, v = 0 is the zenith, so the
// source image is loaded unflipped. Must match environmentDirectionToUv in
// PathTracer/Lights/environment-distribution.part.glsl. See docs/environment-maps.md.
vec2 sampleSphericalMap(vec3 v) {
    return vec2(atan(v.x, -v.z) * InvTwoPI + 0.5f, acos(clamp(v.y, -1.0f, 1.0f)) * InvPI);
}

void main() {
    vec2 uv = sampleSphericalMap(normalize(localPos));
    vec3 color = texture(equirectangularMap, uv).rgb;

    finalColor = vec4(color, 1.0f);
}