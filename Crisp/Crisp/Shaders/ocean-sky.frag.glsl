#version 460 core

#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec2 ndcPosition;

layout(location = 0) out vec4 finalColor;

#include "Common/ocean-atmosphere.part.glsl"
#include "Common/view.part.glsl"

layout(set = 0, binding = 0) uniform View {
    ViewParameters view;
};

layout(set = 1, binding = 0) uniform Atmosphere {
    AtmosphereParams atmosphere;
};

layout(set = 1, binding = 1) uniform sampler2D skyViewLut;

void main() {
    // Reverse-Z, so the near plane is at 1. The engine is Y-up and this reconstruction stays in
    // clip space rather than flipping uvs, which is where ported sky code usually goes wrong.
    const vec4 viewPosition = view.invP * vec4(ndcPosition, 1.0f, 1.0f);
    const vec3 worldDir = normalize((view.invV * vec4(viewPosition.xyz / viewPosition.w, 0.0f)).xyz);
    const vec3 worldPosKm = atmosphere.cameraPosition + vec3(0.0f, atmosphere.bottomRadius, 0.0f);

    finalColor = vec4(sampleSkyRadiance(skyViewLut, atmosphere, worldPosKm, worldDir), 1.0f);
}
