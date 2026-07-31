#version 460 core

#extension GL_GOOGLE_include_directive : require

#define PI 3.1415926535897932384626433832795

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

#include "Common/atmosphere.part.glsl"

struct SingleScatteringResult {
    vec3 L;
    vec3 opticalDepth;
    vec3 transmittance;
    vec3 multiScatAs1;
};

layout(set = 0, binding = 0) uniform AtmosphereParamsBlock {
    AtmosphereParams atmosphere;
};

layout(set = 1, binding = 0, rgba16f) uniform writeonly image2D multiScatteringLut;
layout(set = 1, binding = 1) uniform sampler2D transmittanceLut;

SingleScatteringResult integrateScatteredRadiance(
    in vec2 ndcPos,
    in vec3 worldPos,
    in vec3 worldDir,
    in vec3 sunDir,
    in AtmosphereParams atmosphere,
    in bool ground,
    in float sampleCountIni,
    in float depthBufferValue,
    in bool mieRayPhase,
    float tMaxMax) {
    SingleScatteringResult result;
    result.L = vec3(0.0f);
    result.multiScatAs1 = vec3(0.0f);

    vec3 clipSpace = vec3(ndcPos, 1.0);

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

    depthBufferValue = 0.0f;
    tMax = min(tMax, tMaxMax);

    float sampleCount = sampleCountIni;
    float sampleCountFloor = sampleCountIni;
    float tMaxFloor = tMax;
    float dt = tMax / sampleCount;

    const float cosTheta = dot(sunDir, worldDir);
    const float uniformPhaseValue = 1.0f / (4.0f * PI);
    const float miePhaseValue = computeMiePhaseFunction(atmosphere.miePhaseG, -cosTheta);
    const float rayleighPhaseValue = computeRayleighPhaseFunction(cosTheta);

    vec3 globalL = atmosphere.sunIrradiance.rgb;

    vec3 L = vec3(0.0f);
    vec3 throughput = vec3(1.0);
    vec3 opticalDepth = vec3(0.0);
    float t = 0.0f;
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

        const float pHeight = length(P);
        const vec3 upVector = P / pHeight;
        const float sunZenithCosAngle = dot(sunDir, upVector);
        const vec3 transmittanceToSun = sampleTransmittanceLut(transmittanceLut, atmosphere, pHeight, sunZenithCosAngle);

        // Orders 2..inf only collapse into a geometric series under the assumption that they scatter
        // isotropically, so a directional phase here would compute a different quantity entirely.
        const vec3 phaseTimesScattering =
            mieRayPhase ? medium.scatteringMie * miePhaseValue + medium.scatteringRay * rayleighPhaseValue
                        : medium.scattering * uniformPhaseValue;

        const float tEarth =
            raySphereIntersectNearest(P, sunDir, earthO + kPlanetRadiusOffset * upVector, atmosphere.bottomRadius);
        const float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

        const float shadow = getShadow(atmosphere, P);

        // phaseTimesScattering already carries sigma_s; multiplying by medium.scattering again would square it.
        const vec3 S = globalL * (earthShadow * shadow * transmittanceToSun * phaseTimesScattering);

        const vec3 MS = medium.scattering;
        const vec3 integralMS = (MS - MS * sampleTransmittance) / medium.extinction;
        result.multiScatAs1 += throughput * integralMS;

        const vec3 integralS = (S - S * sampleTransmittance) / medium.extinction;
        L += throughput * integralS;
        throughput *= sampleTransmittance;
    }

    if (ground && tMax == tBottom && tBottom > 0.0) {
        // Account for bounced light off the earth
        vec3 P = worldPos + tBottom * worldDir;

        const float pHeight = length(P);
        const vec3 upVector = P / pHeight;
        const float sunZenithCosAngle = dot(sunDir, upVector);
        const vec3 transmittanceToSun = sampleTransmittanceLut(transmittanceLut, atmosphere, pHeight, sunZenithCosAngle);

        const float nDotL = clamp(dot(normalize(upVector), normalize(sunDir)), 0, 1);
        L += globalL * transmittanceToSun * throughput * nDotL * atmosphere.groundAlbedo / PI;
    }

    result.L = L;
    return result;
}

shared vec3 multiScatShared[64];
shared vec3 radianceShared[64];

void main() {
    const vec2 pixPos = vec2(gl_GlobalInvocationID.xy) + 0.5f;
    const float lutSize = kMultiScatteringLutResolution;
    vec2 uv = pixPos / lutSize;
    uv = vec2(fromSubUvsToUnit(uv.x, lutSize), fromSubUvsToUnit(uv.y, lutSize));

    const vec2 ndcPos = pixPos / vec2(lutSize) * vec2(2.0f, -2.0f) - vec2(1.0f, -1.0f);

    const float cosSunZenithAngle = uv.x * 2.0 - 1.0;
    const float sinSunZenithAngle = sqrt(clamp(1.0 - cosSunZenithAngle * cosSunZenithAngle, 0, 1));
    const vec3 sunDir = vec3(0.0, cosSunZenithAngle, -sinSunZenithAngle);
    // We adjust again viewHeight according to kPlanetRadiusOffset to be in a valid range.
    const float viewHeight =
        atmosphere.bottomRadius +
        clamp(uv.y + kPlanetRadiusOffset, 0, 1) * (atmosphere.topRadius - atmosphere.bottomRadius - kPlanetRadiusOffset);

    const vec3 worldPos = vec3(0.0f, viewHeight, 0.0f);

    const bool ground = true;
    const float sampleCount = 20;
    const float depthBufferValue = kNoDepthBuffer;
    const bool mieRayPhase = false;

    const float sphereSolidAngle = 4.0f * PI;
    const float isotropicPhaseValue = 1.0f / sphereSolidAngle;

    const uint sampleIdx = gl_LocalInvocationID.z;
    const int SQRTSAMPLECOUNT = 8;
    const float sqrtSample = float(SQRTSAMPLECOUNT);
    const float i = 0.5f + float(sampleIdx / SQRTSAMPLECOUNT);
    const float j = 0.5f + float(sampleIdx - (sampleIdx / SQRTSAMPLECOUNT) * SQRTSAMPLECOUNT);
    {
        const float randA = i / sqrtSample;
        const float randB = j / sqrtSample;
        const float theta = 2.0f * PI * randA;
        const float phi = PI * randB;
        const float cosPhi = cos(phi);
        const float sinPhi = sin(phi);
        const float cosTheta = cos(theta);
        const float sinTheta = sin(theta);
        const vec3 worldDir = vec3(cosTheta * sinPhi, cosPhi, -sinTheta * sinPhi);
        SingleScatteringResult result = integrateScatteredRadiance(
            ndcPos, worldPos, worldDir, sunDir, atmosphere, ground, sampleCount, depthBufferValue, mieRayPhase, 9000000.0f);

        const float weight = sphereSolidAngle / (sqrtSample * sqrtSample);
        multiScatShared[sampleIdx] = result.multiScatAs1 * weight;
        radianceShared[sampleIdx] = result.L * weight;
    }

    groupMemoryBarrier();
    barrier();

    for (uint stride = 32; stride >= 1; stride /= 2) {
        if (sampleIdx < stride) {
            multiScatShared[sampleIdx] += multiScatShared[sampleIdx + stride];
            radianceShared[sampleIdx] += radianceShared[sampleIdx + stride];
        }
        groupMemoryBarrier();
        barrier();
    }

    if (sampleIdx > 0) {
        return;
    }

    vec3 multiScatteredRadiance = multiScatShared[0] * isotropicPhaseValue; // Equation 7 f_ms
    vec3 radiance = radianceShared[0] * isotropicPhaseValue;                // Equation 5 L_2ndOrder

    // Each further order scales the previous one by multiScatAs1, so orders 2..inf collapse into the geometric
    // series 1 / (1 - f_ms). Energy conservation puts f_ms below 1, but the ray march only approximates it, so
    // clamp rather than risk a division by zero or a sign flip on the sum.
    const vec3 multiScatteringSum = 1.0f / (1.0 - min(multiScatteredRadiance, vec3(0.999f)));
    const vec3 L = radiance * multiScatteringSum; // Equation 10 Psi_ms
    imageStore(multiScatteringLut, ivec2(gl_GlobalInvocationID.xy), vec4(L, 1.0f));
}
