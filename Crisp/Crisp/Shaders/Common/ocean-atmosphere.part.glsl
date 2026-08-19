#ifndef CRISP_OCEAN_ATMOSPHERE_GLSL
#define CRISP_OCEAN_ATMOSPHERE_GLSL

#include "atmosphere.part.glsl"

vec3 sampleSkyRadiance(
    const sampler2D skyViewLut,
    const AtmosphereParams atmosphere,
    const vec3 worldPosKm,
    const vec3 worldDir) {
    const float viewHeight = length(worldPosKm);
    const vec3 upVector = worldPosKm / viewHeight;
    const float viewZenithCosAngle = dot(worldDir, upVector);

    vec3 sideVector = cross(upVector, worldDir);
    const float sideLength = length(sideVector);
    if (sideLength > 1e-6f) {
        sideVector /= sideLength;
    } else {
        const vec3 fallbackAxis = abs(upVector.y) < 0.99f ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f);
        sideVector = normalize(cross(upVector, fallbackAxis));
    }
    const vec3 forwardVector = normalize(cross(sideVector, upVector));

    const vec2 lightOnPlane =
        vec2(dot(atmosphere.sunDirection, forwardVector), dot(atmosphere.sunDirection, sideVector));
    const float lightOnPlaneLength = length(lightOnPlane);
    const float lightViewCosAngle = lightOnPlaneLength > 1e-6f ? lightOnPlane.x / lightOnPlaneLength : 1.0f;

    const bool intersectsGround =
        raySphereIntersectNearest(worldPosKm, worldDir, vec3(0.0f), atmosphere.bottomRadius) >= 0.0f;
    const vec2 uv = skyViewLutParamsToUv(
        intersectsGround, viewZenithCosAngle, lightViewCosAngle, viewHeight, atmosphere.bottomRadius);

    return textureLod(skyViewLut, uv, 0).rgb;
}

vec3 sampleSunIrradiance(
    const sampler2D transmittanceLut, const AtmosphereParams atmosphere, const vec3 worldPosKm) {
    const float viewHeight = length(worldPosKm);
    const float sunZenithCosAngle = dot(worldPosKm / viewHeight, atmosphere.sunDirection);
    return atmosphere.sunIrradiance.rgb *
           sampleTransmittanceLut(transmittanceLut, atmosphere, viewHeight, sunZenithCosAngle);
}

vec4 sampleOceanAerialPerspective(
    const sampler2DArray cameraVolumeLut, const vec2 screenUv, const float distanceKm) {
    float slice = distanceKm / kCameraVolumeKmPerSlice;

    float weight = 1.0f;
    if (slice < 0.5f) {
        weight = clamp(slice * 2.0f, 0.0f, 1.0f);
        slice = 0.5f;
    }

    const float layer = sqrt(slice * kCameraVolumeLutSliceCount) - 0.5f;
    const float layer0 = clamp(floor(layer), 0.0f, kCameraVolumeLutSliceCount - 1.0f);
    const float layer1 = min(layer0 + 1.0f, kCameraVolumeLutSliceCount - 1.0f);

    const vec4 ap0 = textureLod(cameraVolumeLut, vec3(screenUv, layer0), 0);
    const vec4 ap1 = textureLod(cameraVolumeLut, vec3(screenUv, layer1), 0);
    return weight * mix(ap0, ap1, clamp(layer - layer0, 0.0f, 1.0f));
}

#endif // CRISP_OCEAN_ATMOSPHERE_GLSL
