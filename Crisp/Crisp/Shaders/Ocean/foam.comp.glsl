#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "../Common/ocean.part.glsl"

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

// slopeZ, dDx/dx, dDz/dz, dDx/dz per cascade.
layout(set = 0, binding = 0) uniform sampler2DArray packedJacobianMap;
// Two layers ping-ponged by frame parity. Reading and writing different layers of one image keeps
// this a single graph resource, and graph images are allocated once at compile time, so the
// accumulated foam survives across frames.
layout(set = 0, binding = 1, r32f) uniform image2DArray foamImg;

layout(push_constant) uniform PushConstant {
    int foamGridSize;
    float foamWindowSize;
    float vertexSpacing;
    float choppiness;

    float deltaTime;
    float halfLife;
    float injectionThreshold;
    float injectionGain;

    vec2 driftVelocity;
    int readLayer;
    int writeLayer;

    vec2 anchor;
    vec2 previousAnchor;

    vec4 cascadeSizes;
    vec4 cascadeWavelengths;
};

// Texels that scrolled in this frame hold water the window has never covered. Two cells of slack
// cover the bilinear tap and a frame of drift reaching back past the edge.
const float kFoamHistoryMargin = 2.0f;

vec2 foamWindowCell(const ivec2 texel, const vec2 anchorCell) {
    const float gridSize = float(foamGridSize);
    const vec2 delta = mod(vec2(texel) - anchorCell + 0.5f * gridSize, gridSize) - 0.5f * gridSize;
    return anchorCell + delta;
}

// Bilinear by hand: the image is bound as storage in this pass, so there is no sampler to do it.
float sampleFoamBilinear(const vec2 texelCoord) {
    const vec2 base = floor(texelCoord - 0.5f) + 0.5f;
    const vec2 frac = texelCoord - base;
    const ivec2 baseTexel = ivec2(floor(base));

    float result = 0.0f;
    for (int j = 0; j <= 1; ++j) {
        for (int i = 0; i <= 1; ++i) {
            // Wrapping is the torus, not a periodic ocean: the neighbour across the seam is the
            // one on the far side of the window.
            const ivec2 wrapped = (baseTexel + ivec2(i, j) + ivec2(foamGridSize)) % ivec2(foamGridSize);
            const float weight = (i == 0 ? 1.0f - frac.x : frac.x) * (j == 0 ? 1.0f - frac.y : frac.y);
            result += weight * imageLoad(foamImg, ivec3(wrapped, readLayer)).r;
        }
    }
    return result;
}

void main() {
    const ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
    if (texel.x >= foamGridSize || texel.y >= foamGridSize) {
        return;
    }

    const float cellSize = foamWindowSize / float(foamGridSize);
    const vec2 anchorCell = floor(anchor / cellSize);
    const vec2 cell = foamWindowCell(texel, anchorCell);
    const vec2 worldXZ = (cell + 0.5f) * cellSize;

    // The same combined, weighted Jacobian ocean.frag builds, so foam is injected exactly where the
    // shaded surface is compressed.
    vec3 displacementGradient = vec3(0.0f);
    const float fftSize = float(textureSize(packedJacobianMap, 0).x);
    for (int c = 0; c < OCEAN_CASCADE_COUNT; ++c) {
        const float weight = oceanBandResolveWeight(cascadeWavelengths[c], vertexSpacing);
        if (weight <= 0.0f) {
            continue;
        }
        const vec3 uv = oceanCascadeUv(worldXZ, cascadeSizes[c], fftSize, c);
        displacementGradient += weight * texture(packedJacobianMap, uv).gba;
    }

    const float dDxDx = displacementGradient.x;
    const float dDzDz = displacementGradient.y;
    const float dDxDz = displacementGradient.z;
    const float jacobianDeterminant =
        (1.0f - choppiness * dDxDx) * (1.0f - choppiness * dDzDz) - choppiness * choppiness * dDxDz * dDxDz;

    // Advect before accumulating: the previous frame's foam has drifted by now. Only net drift is
    // gathered here -- foam riding its own wave already comes out of storing this against the world.
    const vec2 drift = driftVelocity * deltaTime / foamWindowSize * float(foamGridSize);
    const vec2 fromPreviousAnchor = abs(cell - floor(previousAnchor / cellSize));
    const bool hasHistory =
        max(fromPreviousAnchor.x, fromPreviousAnchor.y) <= 0.5f * float(foamGridSize) - kFoamHistoryMargin;
    const float previous = hasHistory ? sampleFoamBilinear(vec2(texel) + 0.5f - drift) : 0.0f;

    // Exponential decay expressed as a half-life, so the slider means something in seconds.
    const float decay = exp2(-deltaTime / max(halfLife, 1e-3f));

    // Whitewater is generated where the surface folds, and the more negative the determinant the
    // more violently. Below the threshold nothing is breaking.
    const float compression = max(injectionThreshold - jacobianDeterminant, 0.0f);
    const float injected = injectionGain * compression * deltaTime;

    imageStore(foamImg, ivec3(texel, writeLayer), vec4(previous * decay + injected, 0.0f, 0.0f, 0.0f));
}
