#ifndef CRISP_PATH_TRACER_AREA_LIGHT_GLSL
#define CRISP_PATH_TRACER_AREA_LIGHT_GLSL

#include "../../Common/rng.part.glsl"
#include "../../Common/warp.part.glsl"
#include "../Core/intersection.part.glsl"
#include "light-types.part.glsl"

// Uniform by area over the emitter's triangles, through the alias table the CPU builds alongside the mesh.
// Returns the area density; the caller converts it to solid angle.
float sampleAreaLightSurface(inout Sampler rng, const uint meshId, out vec3 position, out vec3 normal) {
    PathTracedInstance instance = scene.instances.data[meshId];
    const uint triCount = instance.aliasTable.data[0].j; // Header entry, written by createAliasTable.

    const uint elemIdx = 1 + nextRange(rng, triCount); // Add 1 to skip the header entry.
    const float rndVal = next1D(rng);

    uint sampledTriIdx = elemIdx - 1;
    if (rndVal > instance.aliasTable.data[elemIdx].tau) {
        sampledTriIdx = instance.aliasTable.data[elemIdx].j;
    }

    const vec3 bary = squareToUniformTriangle(next2D(rng));
    const uvec3 sampledTriangle = instance.triangles.data[sampledTriIdx];

    position = interpolatePosition(instance.positions, sampledTriangle, bary);
    normal = interpolateNormal(instance.attributes, sampledTriangle, bary);

    return instance.aliasTable.data[0].tau;
}

LightSample sampleAreaLight(
    inout Sampler rng, const uint meshId, const vec3 radiance, const vec3 refPoint) {
    LightSample ls;
    ls.isDelta = false;
    ls.pdf = 0.0f;
    ls.weight = vec3(0.0f);

    vec3 samplePos;
    vec3 sampleNormal;
    const float shapePdf = sampleAreaLightSurface(rng, meshId, samplePos, sampleNormal);

    ls.direction = samplePos - refPoint;
    const float squaredDist = dot(ls.direction, ls.direction);
    ls.distance = sqrt(squaredDist);
    if (ls.distance <= 0.0f) {
        ls.direction = vec3(0.0f);
        return ls;
    }
    ls.direction /= ls.distance;

    const float cosThetaO = dot(sampleNormal, -ls.direction);
    if (cosThetaO <= 0.0f) {
        return ls;
    }

    ls.pdf = shapePdf * squaredDist / cosThetaO;
    ls.weight = radiance / ls.pdf;
    return ls;
}

// Emission leaves the front face only, so a ray arriving from behind the emitter sees nothing.
vec3 evaluateAreaLight(const vec3 radiance, const vec3 lightNormal, const vec3 towardViewer) {
    return dot(lightNormal, normalize(towardViewer)) <= 0.0f ? vec3(0.0f) : radiance;
}

// The area density converted to solid angle at the shading point. `toLight` runs from the shading point to the
// point on the emitter and is not normalized.
float computeAreaLightPdf(const uint meshId, const vec3 toLight, const vec3 lightNormal) {
    const float shapePdf = scene.instances.data[meshId].aliasTable.data[0].tau;
    const float squaredDist = dot(toLight, toLight);
    const float cosTheta = dot(lightNormal, -normalize(toLight));
    return cosTheta <= 0.0f ? 0.0f : shapePdf * squaredDist / cosTheta;
}

#endif // CRISP_PATH_TRACER_AREA_LIGHT_GLSL
