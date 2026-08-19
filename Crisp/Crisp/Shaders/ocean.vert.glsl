#version 450 core

#extension GL_GOOGLE_include_directive : require

#include "Common/ocean-clipmap.part.glsl"
#include "Common/ocean-draw.part.glsl"

layout(location = 0) in vec3 position;

layout(set = 0, binding = 0) uniform TransformPack {
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

// rgba = height, dispX, dispZ, slopeX. One fetch covers everything the displacement needs.
layout(set = 0, binding = 1) uniform sampler2DArray packedDisplacementMap;

layout(location = 0) out vec3 eyePosition;
layout(location = 1) out vec2 oceanWorldXZ;
layout(location = 2) out float vertexSpacing;

void main() {
    const OceanClipmapVertex clipmapVertex = placeOceanClipmapVertex(
        gl_InstanceIndex, position.xz, clipmapOrigin, clipmapFinestSpacing, clipmapBlockQuads);

    oceanWorldXZ = clipmapVertex.worldXZ;
    vertexSpacing = clipmapVertex.vertexSpacing;

    const float fftSize = float(textureSize(packedDisplacementMap, 0).x);

    vec3 displaced = vec3(oceanWorldXZ.x, 0.0f, oceanWorldXZ.y);
    for (int c = 0; c < OCEAN_CASCADE_COUNT; ++c) {
        const vec3 uv = oceanCascadeUv(oceanWorldXZ, cascadeSizes[c], fftSize, c);
        // A band the vertex grid cannot resolve would land as per-vertex noise, not as waves.
        const float weight = oceanBandResolveWeight(cascadeWavelengths[c], vertexSpacing);
        if (weight <= 0.0f) {
            continue;
        }

        const vec4 displacement = textureLod(packedDisplacementMap, uv, 0.0f);
        // Tessendorf choppy waves; the spectrum's D(k) = -i*(k/|k|)*h~ makes the scale negative.
        displaced += weight * vec3(-choppiness * displacement.g, displacement.r, -choppiness * displacement.b);
    }

    const vec2 fromCamera = oceanWorldXZ - clipmapOrigin;
    displaced.y -= dot(fromCamera, fromCamera) / (2.0f * planetRadius);

    eyePosition = (MV * vec4(displaced, 1.0f)).xyz;
    gl_Position = MVP * vec4(displaced, 1.0f);
}
