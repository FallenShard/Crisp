#version 460 core

#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec4 finalColor;

layout(location = 0) in vec3 eyePosition;
layout(location = 1) in vec2 oceanWorldXZ;
layout(location = 2) in float vertexSpacing;

#include "BSDFs/microfacet.part.glsl"
#include "Common/ocean-atmosphere.part.glsl"
#include "Common/ocean-draw.part.glsl"
#include "Common/view.part.glsl"

layout(set = 0, binding = 1) uniform sampler2DArray packedDisplacementMap; // height, dispX, dispZ, slopeX.
layout(set = 0, binding = 2) uniform sampler2DArray packedJacobianMap;    // slopeZ, dDx/dx, dDz/dz, dDx/dz.
// Accumulated whitewater in the coarsest cascade's reference space; layer selected by frame parity.
layout(set = 0, binding = 3) uniform sampler2DArray foamMap;
// Tileable fBm value noise, sampled at several scales to erode the foam boundary.
layout(set = 0, binding = 4) uniform sampler2D foamNoiseMap;

layout(set = 1, binding = 0) uniform View {
    ViewParameters view;
};

layout(set = 1, binding = 1) uniform sampler2D brdfLut;

layout(set = 2, binding = 0) uniform Atmosphere {
    AtmosphereParams atmosphere;
};

layout(set = 2, binding = 1) uniform sampler2D transmittanceLut;
layout(set = 2, binding = 2) uniform sampler2D skyViewLut;
layout(set = 2, binding = 3) uniform sampler2DArray cameraVolumeLut;

const float kScatterFraction = 0.35f;

// Metres per tile, deliberately not in integer ratios so the three together have no visible period.
const vec3 kFoamNoiseScales = vec3(1.0f / 17.3f, 1.0f / 5.1f, 1.0f / 1.7f);
const vec3 kFoamNoiseWeights = vec3(0.5f, 0.32f, 0.18f);

float sampleFoamNoise(const vec2 worldXZ) {
    return kFoamNoiseWeights.x * texture(foamNoiseMap, worldXZ * kFoamNoiseScales.x).r +
           kFoamNoiseWeights.y * texture(foamNoiseMap, worldXZ * kFoamNoiseScales.y).r +
           kFoamNoiseWeights.z * texture(foamNoiseMap, worldXZ * kFoamNoiseScales.z).r;
}

vec3 computeSkyReflection(
    const vec3 worldN, const vec3 worldV, const vec3 worldPosKm, const float roughness, const vec3 F0) {
    const vec3 worldR = reflect(-worldV, worldN);
    const vec3 blurredR = normalize(mix(worldR, worldN, roughness * roughness));
    const vec3 reflection = sampleSkyRadiance(skyViewLut, atmosphere, worldPosKm, blurredR);

    const float NdotV = max(dot(worldN, worldV), 0.0f);
    const vec2 brdf = texture(brdfLut, vec2(NdotV, roughness)).xy;
    return reflection * (F0 * brdf.x + brdf.y);
}

struct SurfaceFields {
    vec2 slope;
    float waveHeight;
    // dDx/dx, dDz/dz, dDx/dz. The fourth Jacobian entry, dDz/dx, is the same field as the third.
    vec3 displacementGradient;
    float unresolvedSlopeVariance;
};

SurfaceFields sampleCascades(const float pixelSpacing, const float vertexSpacing) {
    SurfaceFields fields;
    fields.slope = vec2(0.0f);
    fields.waveHeight = 0.0f;
    fields.displacementGradient = vec3(0.0f);
    fields.unresolvedSlopeVariance = 0.0f;

    const float fftSize = float(textureSize(packedDisplacementMap, 0).x);

    for (int c = 0; c < OCEAN_CASCADE_COUNT; ++c) {
        const vec3 uv = oceanCascadeUv(oceanWorldXZ, cascadeSizes[c], fftSize, c);
        const float slopeWeight = oceanBandResolveWeight(cascadeWavelengths[c], pixelSpacing);

        const vec4 displacement = texture(packedDisplacementMap, uv);
        const vec4 jacobian = texture(packedJacobianMap, uv);
        fields.slope += slopeWeight * vec2(displacement.a, jacobian.r);
        // Slope the footprint swallowed is not gone: scaling the field by w scales its variance by
        // w^2, and the missing remainder widens the specular lobe below instead.
        fields.unresolvedSlopeVariance += (1.0f - slopeWeight * slopeWeight) * cascadeSlopeVariances[c];

        // Must match the weight ocean.vert applied, or the Jacobian describes a surface that was
        // never displaced. Uniform across the draw: it is a function of push constants alone.
        const float dispWeight = oceanBandResolveWeight(cascadeWavelengths[c], vertexSpacing);
        if (dispWeight <= 0.0f) {
            continue;
        }

        fields.waveHeight += dispWeight * displacement.r;
        // Transformed from the spectrum, so no finite differences and no derivative span to scale by.
        fields.displacementGradient += dispWeight * jacobian.gba;
    }

    return fields;
}

void main() {
    const vec2 footprint = fwidth(oceanWorldXZ);
    const float pixelSpacing = max(footprint.x, footprint.y);

    const SurfaceFields fields = sampleCascades(pixelSpacing, vertexSpacing);

    const float dDxDx = fields.displacementGradient.x;
    const float dDzDz = fields.displacementGradient.y;
    const float dDxDz = fields.displacementGradient.z; // Also dDz/dx.
    const float jacobianDeterminant =
        (1.0f - choppiness * dDxDx) * (1.0f - choppiness * dDzDz) - choppiness * choppiness * dDxDz * dDxDz;
    const float compression = max(1.0f - jacobianDeterminant, 0.0f);

    // Choppiness shears the tangents sideways; only the Jacobian terms carry that.
    const vec3 dPdx = vec3(1.0f - choppiness * dDxDx, fields.slope.x, -choppiness * dDxDz);
    const vec3 dPdz = vec3(-choppiness * dDxDz, fields.slope.y, 1.0f - choppiness * dDzDz);
    const vec3 worldN = normalize(cross(dPdz, dPdx));

    const vec3 eyeN = normalize((view.V * vec4(worldN, 0.0f)).xyz);
    const vec3 eyeV = normalize(-eyePosition);
    const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    const vec3 eyeL = normalize((view.V * vec4(atmosphere.sunDirection, 0.0f)).xyz);
    const float NdotL = max(dot(eyeN, eyeL), 0.0f);

    const float crestFactor = smoothstep(0.0f, 1.5f, fields.waveHeight * invRmsWaveHeight);
    // Foam is read from the accumulation buffer rather than from this frame's Jacobian, so it
    // persists and decays instead of blinking with the wave that made it. The instantaneous
    // compression still contributes, which keeps the leading edge of a break crisp.
    const vec2 foamUv = oceanWorldXZ / foamWindowSize;
    const float windowFade =
        1.0f - smoothstep(0.75f, 0.95f, distance(oceanWorldXZ, view.invV[3].xz) / (0.5f * foamWindowSize));
    const float accumulated = windowFade * texture(foamMap, vec3(foamUv, float(foamLayer))).r;
    const float foamSignal = compression + accumulated;

    // Zero-mean on purpose: the noise has to eat into the boundary and bulge out of it in equal
    // measure, tearing it into streaks and holes. Biased noise would just subtract coverage
    // everywhere. A bare smoothstep on a smooth field can only ever give a soft blob edge.
    const float erosion = foamErosion * (sampleFoamNoise(oceanWorldXZ) - 0.5f);
    const float eroded = foamSignal - erosion;
    const float coverage = smoothstep(foamThreshold, foamThreshold + foamSoftness, eroded);
    // Fresh foam is thick and opaque. Ageing should thin it toward sparse streaks rather than dim it
    // uniformly, so the accumulated value drives opacity as well as coverage.
    const float freshness = clamp(accumulated * foamFreshness, 0.0f, 1.0f);
    const float foam = clamp(foamIntensity * coverage * mix(0.45f, 1.0f, freshness), 0.0f, 1.0f);

    const float baseRoughness = mix(max(waterRoughness, 0.03f), 0.35f, foam);
    const float baseAlpha = baseRoughness * baseRoughness;
    // Toksvig/LEAN for GGX: convolving the NDF with the slope distribution that fell below the
    // footprint adds 2*sigma^2 to alpha^2. Without it the lost cascades come back as crawling
    // aliasing rather than as a wider lobe.
    const float alpha =
        min(sqrt(baseAlpha * baseAlpha + 2.0f * slopeVarianceScale * fields.unresolvedSlopeVariance), 1.0f);
    const float roughness = sqrt(alpha);

    const vec3 F0 = vec3(0.02f);
    const vec3 F = fresnelSchlick(NdotV, F0);

    const vec3 worldV = normalize((view.invV * vec4(eyeV, 0.0f)).xyz);
    const vec3 worldPosKm = atmosphere.cameraPosition + vec3(0.0f, atmosphere.bottomRadius, 0.0f);
    const vec3 sunIrradiance = sampleSunIrradiance(transmittanceLut, atmosphere, worldPosKm);

    const vec3 eyeH = normalize(eyeL + eyeV);
    const float NdotH = max(dot(eyeN, eyeH), 0.0f);
    const float D = distributionGgx(NdotH, alpha); // GGX takes alpha, not perceptual roughness.
    const float G = geometrySmith(NdotV, NdotL, roughness);
    const vec3 sunFresnel = fresnelSchlick(max(dot(eyeV, eyeH), 0.0f), F0);
    const vec3 sunSpecular = (D * G * sunFresnel / max(4.0f * NdotV * NdotL, 0.001f)) * NdotL * sunIrradiance;
    const vec3 reflection = computeSkyReflection(worldN, worldV, worldPosKm, roughness, F0);
    const vec3 skyIrradiance = PI * sampleSkyRadiance(skyViewLut, atmosphere, worldPosKm, vec3(0.0f, 1.0f, 0.0f));

    const vec3 downwellingIrradiance = skyIrradiance + sunIrradiance * NdotL;

    // Water has no Lambertian albedo, so both scatter terms share one (1 - F).
    const vec3 scatterColor = pow(vec3(12, 120, 167) / 255.0f, vec3(2.2f));
    const vec3 subsurfaceColor = pow(vec3(20, 140, 130) / 255.0f, vec3(2.2f));
    const float backLitFactor = pow(max(dot(eyeV, -eyeL), 0.0f), 4.0f);
    const vec3 body =
        (1.0f - F) * (scatterColor * downwellingIrradiance * kScatterFraction / PI +
                      subsurfaceColor * crestFactor * backLitFactor * sunIrradiance / PI);

    const vec3 waterRadiance = body + reflection + sunSpecular;
    const vec3 foamColor = pow(vec3(235, 244, 238) / 255.0f, vec3(2.2f));
    const vec3 foamRadiance = foamColor * downwellingIrradiance / PI;
    vec3 radiance = mix(waterRadiance, foamRadiance, foam);

    const vec2 screenUv = gl_FragCoord.xy / view.screenSize;
    const float distanceKm = length(eyePosition) / kMetersPerKilometer;
    const vec4 aerialPerspective = sampleOceanAerialPerspective(cameraVolumeLut, screenUv, distanceKm);
    radiance = radiance * (1.0f - aerialPerspective.a) + aerialPerspective.rgb;

    finalColor = vec4(radiance, 1.0f);
}
