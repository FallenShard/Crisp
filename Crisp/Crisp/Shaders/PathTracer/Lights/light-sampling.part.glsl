#ifndef CRISP_PATH_TRACER_LIGHT_SAMPLING_GLSL
#define CRISP_PATH_TRACER_LIGHT_SAMPLING_GLSL

#include "environment-distribution.part.glsl"
#include "point-light.part.glsl"

vec3 evaluateEnvironment(const vec3 direction) {
    if (integrator.environmentEnabled == 0) {
        return vec3(0.0f);
    }
    const vec2 uv = environmentDirectionToUv(direction);
    return integrator.environmentScale *
        textureLod(sampler2D(environmentMap, environmentSampler), uv, 0.0f).rgb;
}

float getEnvironmentLightPdf(const vec3 direction) {
    if (integrator.environmentEnabled == 0 || integrator.lightCount <= 0) {
        return 0.0f;
    }
    return environmentDirectionPdf(
               scene.environmentCdf,
               uint(integrator.environmentWidth),
               uint(integrator.environmentHeight),
               direction) /
        float(integrator.lightCount);
}

vec3 sampleEnvironmentLight(
    inout Sampler rng, out vec3 shadowRayDir, out float shadowRayLen, out float lightPdf) {
    vec2 uv;
    shadowRayDir = sampleEnvironmentDirection(
        scene.environmentCdf,
        uint(integrator.environmentWidth),
        uint(integrator.environmentHeight),
        next2D(rng),
        uv,
        lightPdf);
    shadowRayLen = 1e30f;
    return lightPdf > 0.0f ? evaluateEnvironment(shadowRayDir) / lightPdf : vec3(0.0f);
}

float sampleSurfaceCoord(inout Sampler rng, in uint meshId, out vec3 position, out vec3 normal) {
    const uint aliasTableOffset = scene.instances.data[meshId].aliasTableOffset;
    const uint triCount = scene.aliasTable.data[aliasTableOffset].j;

    const uint elemIdx = 1 + nextRange(rng, triCount); // Add 1 to skip the header entry.
    const float rndVal = next1D(rng);

    uint sampledTriIdx = elemIdx - 1;
    if (rndVal > scene.aliasTable.data[aliasTableOffset + elemIdx].tau) {
        sampledTriIdx = scene.aliasTable.data[aliasTableOffset + elemIdx].j;
    }

    const vec3 bary = squareToUniformTriangle(next2D(rng));

    const uint triangleOffset = scene.instances.data[meshId].indexOffset;
    const uvec3 sampledTriangle = scene.triangles.data[triangleOffset + sampledTriIdx];

    position = interpolatePosition(sampledTriangle, bary);
    normal = interpolateNormal(sampledTriangle, bary);

    return scene.aliasTable.data[aliasTableOffset].tau;
}

vec3 sampleAreaLight(
    inout Sampler rng,
    in uint meshId,
    in vec3 radiance,
    in vec3 refPoint,
    out vec3 shadowRayDir,
    out float shadowRayLen,
    out float lightPdf) {
    lightPdf = 0.0f;

    vec3 samplePos;
    vec3 sampleNormal;
    const float shapePdf = sampleSurfaceCoord(rng, meshId, samplePos, sampleNormal);

    shadowRayDir = samplePos - refPoint;

    const float squaredDist = dot(shadowRayDir, shadowRayDir);
    shadowRayLen = sqrt(squaredDist);
    if (shadowRayLen <= 0.0f) {
        shadowRayDir = vec3(0.0f);
        return vec3(0.0f);
    }
    shadowRayDir /= shadowRayLen;

    const float cosThetaO = dot(sampleNormal, -shadowRayDir);
    if (cosThetaO <= 0.0f) {
        return vec3(0.0f);
    }

    lightPdf = shapePdf * squaredDist / cosThetaO;
    return radiance / lightPdf;
}

vec3 sampleUniformLight(
    inout Sampler rng,
    in vec3 refPoint,
    out vec3 shadowRayDir,
    out float shadowRayLen,
    out float lightPdf,
    out bool lightIsDelta) {
    const uint lightId = nextRange(rng, integrator.lightCount);
    const float uniformPdf = 1.0f / float(integrator.lightCount);

    const uint finiteLightCount = uint(integrator.lightCount - integrator.environmentEnabled);
    vec3 radiance;
    lightIsDelta = false;
    if (lightId < finiteLightCount) {
        const LightParameters light = scene.lights.data[lightId];
        if (light.type == kLightPoint) {
            lightIsDelta = true;
            radiance = samplePointLight(
                light.position, light.emission, refPoint, shadowRayDir, shadowRayLen, lightPdf);
        } else {
            radiance = sampleAreaLight(
                rng,
                light.meshId,
                light.emission,
                refPoint,
                shadowRayDir,
                shadowRayLen,
                lightPdf);
        }
    } else {
        radiance = sampleEnvironmentLight(rng, shadowRayDir, shadowRayLen, lightPdf);
    }
    lightPdf *= uniformPdf;
    return radiance / uniformPdf;
}

float getLightPdf(in int lightId, in vec3 hitVector, in vec3 hitNormal) {
    if (scene.lights.data[lightId].type != kLightArea) {
        return 0.0f;
    }
    const int meshId = scene.lights.data[lightId].meshId;
    const uint aliasTableOffset = scene.instances.data[meshId].aliasTableOffset;
    const float shapePdf = scene.aliasTable.data[aliasTableOffset].tau;

    const float squaredDist = dot(hitVector, hitVector);
    const float cosTheta = dot(hitNormal, -normalize(hitVector));
    if (cosTheta <= 0.0f) {
        return 0.0f;
    }

    const float uniformPdf = 1.0f / float(integrator.lightCount);
    return uniformPdf * shapePdf * squaredDist / cosTheta;
}

#endif // CRISP_PATH_TRACER_LIGHT_SAMPLING_GLSL
