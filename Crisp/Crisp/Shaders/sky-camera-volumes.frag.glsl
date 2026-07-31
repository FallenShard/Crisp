#version 450 core

#extension GL_GOOGLE_include_directive : require

#define PI 3.1415926535897932384626433832795

layout(location = 0) out vec4 finalColor;

#include "Common/atmosphere.part.glsl"

layout(set = 0, binding = 0) uniform AtmosphereParamsBlock {
    AtmosphereParams atmosphere;
};

layout(set = 1, binding = 0) uniform sampler2D transmittanceLut;
layout(set = 1, binding = 1) uniform sampler2D multiScatteringLut;

struct SingleScatteringResult {
    vec3 L;
    vec3 opticalDepth;
    vec3 transmittance;
    vec3 multiScatAs1;
};

SingleScatteringResult integrateScatteredRadiance(
    in vec2 pixPos,
    in vec3 worldPos,
    in vec3 worldDir,
    in vec3 sunDir,
    const in AtmosphereParams atmosphere,
    in bool ground,
    in float sampleCountIni,
    in float depthBufferValue,
    in bool mieRayPhase,
    float tMaxMax) {
    SingleScatteringResult result;
    result.L = vec3(0.0f);
    result.opticalDepth = vec3(0.0f);
    result.transmittance = vec3(0.0f);
    result.multiScatAs1 = vec3(0.0f);

    vec3 clipSpace = vec3(pixPos / vec2(kCameraVolumeLutWidth, kCameraVolumeLutHeight) * 2.0f - 1.0f, kFarPlaneDepth);

    vec3 earthO = vec3(0.0f, 0.0f, 0.0f);
    float tBottom = raySphereIntersectNearest(worldPos, worldDir, earthO, atmosphere.bottomRadius);
    float tTop = raySphereIntersectNearest(worldPos, worldDir, earthO, atmosphere.topRadius);
    float tMax = 0.0f;
    if (tBottom < 0.0f) {
        if (tTop < 0.0f) {
            tMax = 0.0f;
            return result;
        } else {
            tMax = tTop;
        }
    } else {
        if (tTop > 0.0f) {
            tMax = min(tTop, tBottom);
        }
    }

    if (depthBufferValue != kNoDepthBuffer) {
        clipSpace.z = depthBufferValue;
        if (clipSpace.z > kFarPlaneDepth) {
            vec4 depthBufferWorldPos = atmosphere.invVP * vec4(clipSpace, 1.0);
            depthBufferWorldPos /= depthBufferWorldPos.w;

            // Undo the planet offset in worldPos to match; this engine is Y up.
            float tDepth = length(depthBufferWorldPos.xyz - (worldPos + vec3(0.0, -atmosphere.bottomRadius, 0.0)));
            if (tDepth < tMax) {
                tMax = tDepth;
            }
        }
    }
    tMax = min(tMax, tMaxMax);

    float sampleCount = sampleCountIni;
    float sampleCountFloor = sampleCountIni;
    float tMaxFloor = tMax;
    float dt = tMax / sampleCount;

    float cosTheta = dot(sunDir, worldDir);

    // Negated because worldDir is an incoming direction.
    const float miePhaseValue = computeMiePhaseFunction(atmosphere.miePhaseG, -cosTheta);
    const float rayleighPhaseValue = computeRayleighPhaseFunction(cosTheta);
    const vec3 globalL = atmosphere.sunIrradiance.rgb;

    vec3 L = vec3(0.0f);
    vec3 throughput = vec3(1.0);
    vec3 opticalDepth = vec3(0.0);
    float t = 0.0f;
    float tPrev = 0.0;
    const float sampleSegmentT = 0.3f;
    for (float s = 0.0f; s < sampleCount; s += 1.0f) {
        // Stepping by the exact difference rather than the nominal step keeps the integration accurate.
        float newT = tMax * (s + sampleSegmentT) / sampleCount;
        dt = newT - t;
        t = newT;
        vec3 P = worldPos + t * worldDir;

        MediumSample medium = sampleMedium(P, atmosphere);
        const vec3 sampleOpticalDepth = medium.extinction * dt;
        const vec3 sampleTransmittance = exp(-sampleOpticalDepth);
        opticalDepth += sampleOpticalDepth;

        float pHeight = length(P);
        const vec3 upVector = P / pHeight;
        float sunZenithCosAngle = dot(sunDir, upVector);
        const vec3 transmittanceToSun = sampleTransmittanceLut(transmittanceLut, atmosphere, pHeight, sunZenithCosAngle);

        const vec3 phaseTimesScattering =
            medium.scatteringMie * miePhaseValue + medium.scatteringRay * rayleighPhaseValue;

        const float tEarth =
            raySphereIntersectNearest(P, sunDir, earthO + kPlanetRadiusOffset * upVector, atmosphere.bottomRadius);
        const float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

        const vec3 multiScatteredL =
            atmosphere.multipleScatteringFactor *
            sampleMultipleScattering(multiScatteringLut, atmosphere, pHeight, sunZenithCosAngle);

        const float shadow = getShadow(atmosphere, P);

        const vec3 S =
            globalL *
            (earthShadow * shadow * transmittanceToSun * phaseTimesScattering + multiScatteredL * medium.scattering);
        const vec3 integralS = (S - S * sampleTransmittance) / medium.extinction;
        L += throughput * integralS;
        throughput *= sampleTransmittance;

        tPrev = t;
    }

    result.L = L;
    result.opticalDepth = opticalDepth;
    result.transmittance = throughput;
    return result;
}

void main() {
    const vec2 pixPos = gl_FragCoord.xy;
    const vec3 ndcPos = vec3(pixPos / vec2(kCameraVolumeLutWidth, kCameraVolumeLutHeight) * 2.0f - 1.0f, 0.5f);
    const vec4 HPos = atmosphere.invVP * vec4(ndcPos, 1.0);
    const vec3 eyePos = HPos.xyz / HPos.w;
    vec3 worldDir = normalize(eyePos - atmosphere.cameraPosition);

    vec3 worldPosEcef = atmosphere.cameraPosition + vec3(0.0f, atmosphere.bottomRadius, 0.0f);
    vec3 worldPos = worldPosEcef;

    float sliceT = (float(gl_Layer) + 0.5f) / kCameraVolumeLutSliceCount;
    sliceT *= sliceT; // Squared distribution puts more slices near the camera.
    sliceT *= kCameraVolumeLutSliceCount;

    float tMax = sliceT * kCameraVolumeKmPerSlice;
    vec3 newWorldPos = worldPos + tMax * worldDir;

    float viewHeight = length(newWorldPos);
    if (viewHeight <= (atmosphere.bottomRadius + kPlanetRadiusOffset)) {
        // Lift a voxel that ended up under the ground back onto it, or large voxels show artifacts at the boundary.
        newWorldPos = normalize(newWorldPos) * (atmosphere.bottomRadius + kPlanetRadiusOffset + 0.001f);
        worldDir = normalize(newWorldPos - worldPosEcef);
        tMax = length(newWorldPos - worldPosEcef);
    }
    float tMaxMax = tMax;

    viewHeight = length(worldPos);
    if (viewHeight >= atmosphere.topRadius) {
        vec3 prevWorldPos = worldPos;
        if (!moveToTopAtmosphere(worldPos, worldDir, atmosphere.topRadius)) {
            finalColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
            return;
        }
        const float lengthToAtmosphere = length(prevWorldPos - worldPos);
        if (tMaxMax < lengthToAtmosphere) {
            // This voxel ends before the atmosphere even starts.
            finalColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
            return;
        }

        // The start moved forward to the atmosphere boundary, so the remaining distance shrinks by as much.
        tMaxMax = max(0.0, tMaxMax - lengthToAtmosphere);
    }

    const bool ground = false;
    const float sampleCountIni = max(1.0, float(gl_Layer + 1.0) * 2.0f);
    const float depthBufferValue = kNoDepthBuffer;
    const bool mieRayPhase = true;
    const vec3 sunDir = atmosphere.sunDirection;
    SingleScatteringResult ss = integrateScatteredRadiance(
        pixPos, worldPos, worldDir, sunDir, atmosphere, ground, sampleCountIni, depthBufferValue, mieRayPhase, tMaxMax);

    const float transmittance = dot(ss.transmittance, vec3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f));
    finalColor = vec4(ss.L, 1.0 - transmittance);
}