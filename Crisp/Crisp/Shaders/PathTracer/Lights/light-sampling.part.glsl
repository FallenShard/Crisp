#ifndef CRISP_PATH_TRACER_LIGHT_SAMPLING_GLSL
#define CRISP_PATH_TRACER_LIGHT_SAMPLING_GLSL

float sampleSurfaceCoord(inout uint seed, in uint meshId, out vec3 position, out vec3 normal) {
    const uint aliasTableOffset = scene.instances.data[meshId].aliasTableOffset;
    const uint triCount = scene.aliasTable.data[aliasTableOffset].j;

    const uint elemIdx = 1 + rndRange(seed, triCount); // Add 1 to skip the header entry.
    const float rndVal = rndFloat(seed);

    uint sampledTriIdx = elemIdx - 1;
    if (rndVal > scene.aliasTable.data[aliasTableOffset + elemIdx].tau) {
        sampledTriIdx = scene.aliasTable.data[aliasTableOffset + elemIdx].j;
    }

    const float r1 = rndFloat(seed);
    const float r2 = rndFloat(seed);
    const vec3 bary = squareToUniformTriangle(vec2(r1, r2));

    const uint triangleOffset = scene.instances.data[meshId].indexOffset;
    const uvec3 sampledTriangle = scene.triangles.data[triangleOffset + sampledTriIdx];

    position = interpolatePosition(sampledTriangle, bary);
    normal = interpolateNormal(sampledTriangle, bary);

    return scene.aliasTable.data[aliasTableOffset].tau;
}

vec3 sampleAreaLight(
    inout uint seed,
    in uint meshId,
    in vec3 radiance,
    in vec3 refPoint,
    out vec3 shadowRayDir,
    out float shadowRayLen,
    out float lightPdf) {
    lightPdf = 0.0f;

    vec3 samplePos;
    vec3 sampleNormal;
    const float shapePdf = sampleSurfaceCoord(seed, meshId, samplePos, sampleNormal);

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
    inout uint seed, in vec3 refPoint, out vec3 shadowRayDir, out float shadowRayLen, out float lightPdf) {
    const uint lightId = rndRange(seed, integrator.lightCount);
    const float uniformPdf = 1.0f / float(integrator.lightCount);

    const vec3 radiance = sampleAreaLight(
        seed,
        scene.lights.data[lightId].meshId,
        scene.lights.data[lightId].radiance,
        refPoint,
        shadowRayDir,
        shadowRayLen,
        lightPdf);
    lightPdf *= uniformPdf;
    return radiance / uniformPdf;
}

float getLightPdf(in int lightId, in vec3 hitVector, in vec3 hitNormal) {
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
