#version 450 core

#extension GL_GOOGLE_include_directive : require

#define PI 3.14159265358979323846

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 finalColor;

#include "Common/atmosphere.part.glsl"

layout(set = 0, binding = 0) uniform AtmosphereParamsBlock {
    AtmosphereParams atmosphere;
};

layout(set = 1, binding = 0) uniform sampler2D transmittanceLut;
layout(set = 1, binding = 1) uniform sampler2D multiScatteringLut;

vec3 integrateScatteredLuminance(
    const vec3 worldPos,
    const vec3 worldDir,
    const vec3 sunDir,
    in AtmosphereParams atmosphere,
    const float sampleCountIni,
    const bool variableSampleCount,
    const bool ground) {
    // Compute next intersection with atmosphere or ground
    const vec3 earthCenter = vec3(0.0f, 0.0f, 0.0f);
    float tMax = intersectAtmosphere(worldPos, worldDir, atmosphere.bottomRadius, atmosphere.topRadius);
    if (tMax < 0.0f) {
        return vec3(0.0f);
    }

    // The ground sits inside the atmosphere shell, so reaching it at all means tMax is that hit.
    const float tGround = raySphereIntersectNearest(worldPos, worldDir, earthCenter, atmosphere.bottomRadius);

    tMax = min(tMax, 9000000.0f);

    // Sample count
    float sampleCount = sampleCountIni;
    float sampleCountFloor = sampleCountIni;
    float tMaxFloor = tMax;
    if (variableSampleCount) {
        sampleCount =
            mix(atmosphere.minRayMarchingSamples, atmosphere.maxRayMarchingSamples, clamp(tMax * 0.01f, 0.0f, 1.0f));
        sampleCountFloor = floor(sampleCount);
        tMaxFloor = tMax * sampleCountFloor / sampleCount; // rescale tMax to map to the last entire step segment.
    }
    float dt = tMax / sampleCount;

    // Phase functions
    const float uniformPhase = 1.0 / (4.0 * PI);
    const vec3 wi = sunDir;
    const vec3 wo = worldDir;
    const float cosTheta = dot(wi, wo);

    // negate cosTheta because due to worldDir being a "in" direction.
    const float miePhaseValue = computeMiePhaseFunction(atmosphere.miePhaseG, -cosTheta);
    const float rayleighPhaseValue = computeRayleighPhaseFunction(cosTheta);
    const vec3 globalL = atmosphere.sunIlluminance.rgb;

    // Ray march the atmosphere to integrate luminance.
    vec3 L = vec3(0.0f);
    vec3 throughput = vec3(1.0f);
    float t = 0.0f;
    const float segmentBias = 0.3f;
    for (float s = 0.0f; s < sampleCount; s += 1.0f) {
        if (variableSampleCount) {
            // More expensive, but artifact-free.
            float t0 = (s) / sampleCountFloor;
            float t1 = (s + 1.0f) / sampleCountFloor;
            // Non linear distribution of sample within the range.
            t0 = t0 * t0;
            t1 = t1 * t1;

            // Make t0 and t1 world space distances.
            t0 = tMaxFloor * t0;
            if (t1 > 1.0) {
                t1 = tMax;
            } else {
                t1 = tMaxFloor * t1;
            }
            dt = t1 - t0;
            t = t0 + (t1 - t0) * segmentBias;
        } else {
            const float tNext = tMax * (s + segmentBias) / sampleCount;
            dt = tNext - t;
            t = tNext;
        }

        // Sample the transmittance for the current segment part.
        const vec3 pos = worldPos + t * worldDir;
        const MediumSample medium = sampleMedium(pos, atmosphere);
        const vec3 stepTransmittance = exp(-medium.extinction * dt);

        const float posHeight = length(pos);
        const vec3 upVec = pos / posHeight;
        const float sunZenithCosAngle = dot(sunDir, upVec);
        const vec3 transmittanceToSun =
            sampleTransmittanceLut(transmittanceLut, atmosphere, posHeight, sunZenithCosAngle);

        const vec3 phaseTimesScattering =
            medium.scatteringMie * miePhaseValue + medium.scatteringRay * rayleighPhaseValue;

        // Earth shadow
        const float tEarth =
            raySphereIntersectNearest(pos, sunDir, earthCenter + kPlanetRadiusOffset * upVec, atmosphere.bottomRadius);
        const float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

        // First evaluate opaque shadow
        const float shadow = getShadow(atmosphere, pos);

        const vec3 multiScatteredL =
            atmosphere.multipleScatteringFactor *
            sampleMultipleScattering(multiScatteringLut, atmosphere, posHeight, sunZenithCosAngle);
        const vec3 S =
            globalL *
            (earthShadow * shadow * transmittanceToSun * phaseTimesScattering + multiScatteredL * medium.scattering);

        // See slide 28 at http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/.
        // Integrate along the current step segment.
        const vec3 integralS = (S - S * stepTransmittance) / medium.extinction;

        // Accumulate and also take into account the transmittance from previous steps.
        L += throughput * integralS;
        throughput *= stepTransmittance;
    }

    if (ground && tGround >= 0.0f) {
        const vec3 pos = worldPos + tGround * worldDir;
        const float posHeight = length(pos);
        const vec3 upVec = pos / posHeight;
        const float sunZenithCosAngle = dot(sunDir, upVec);
        const vec3 transmittanceToSun =
            sampleTransmittanceLut(transmittanceLut, atmosphere, posHeight, sunZenithCosAngle);

        const float nDotL = clamp(sunZenithCosAngle, 0.0f, 1.0f);
        L += globalL * transmittanceToSun * throughput * nDotL * atmosphere.groundAlbedo / PI;
    }

    return L;
}

void main() {
    const vec2 pixPos = vec2(gl_FragCoord.xy);
    const vec2 uv = pixPos / vec2(kSkyViewLutWidth, kSkyViewLutHeight);

    vec3 worldPos = atmosphere.cameraPosition + vec3(0, atmosphere.bottomRadius, 0);
    const float viewHeight = length(worldPos);

    float viewZenithCosAngle;
    float lightViewCosAngle;
    uvToSkyViewLutParams(uv, atmosphere.bottomRadius, viewHeight, viewZenithCosAngle, lightViewCosAngle);

    vec3 sunDir;
    {
        const vec3 upVec = worldPos / viewHeight;
        const float sunZenithCosAngle = dot(upVec, atmosphere.sunDirection);
        const float sunZenithSinAngle = sqrt(1.0 - sunZenithCosAngle * sunZenithCosAngle);
        sunDir = normalize(vec3(sunZenithSinAngle, sunZenithCosAngle, 0.0f));
    }

    worldPos = vec3(0.0f, viewHeight, 0.0f);

    const float viewZenithSinAngle = sqrt(max(0.0f, 1.0f - viewZenithCosAngle * viewZenithCosAngle));
    const vec3 worldDir = vec3(
        viewZenithSinAngle * lightViewCosAngle,
        viewZenithCosAngle,
        -viewZenithSinAngle * sqrt(max(0, 1.0 - lightViewCosAngle * lightViewCosAngle)));

    // Move to top atmospehre
    if (!moveToTopAtmosphere(worldPos, worldDir, atmosphere.topRadius)) {
        // Ray is not intersecting the atmosphere
        finalColor = vec4(0, 0, 0, 1);
        return;
    }

    const float sampleCountIni = 30;
    const bool variableSampleCount = true;

    // Must match the final ray march, or the two disagree below the horizon once the fast sky path takes over.
    const bool ground = atmosphere.renderGround != 0;
    const vec3 L = integrateScatteredLuminance(
        worldPos, worldDir, sunDir, atmosphere, sampleCountIni, variableSampleCount, ground);
    finalColor = vec4(L, 1.0f);
}