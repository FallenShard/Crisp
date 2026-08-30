#ifndef CRISP_PATH_TRACER_TRACING_GLSL
#define CRISP_PATH_TRACER_TRACING_GLSL

// The BSDF sample is drawn here rather than in the hit shader so the sampler stays entirely in the
// ray generation shader and the payload carries no RNG state. Drawing it up front also costs the
// same dimensions whether or not the ray hits, which is what keeps paths aligned.
void traceRay(
    inout Sampler rng, in uint bounceDimBase, in vec3 rayOrigin, in float tMin, in vec3 rayDirection, in float tMax) {
    setDimension(rng, bounceDimBase + kDimBsdf);
    hitInfo.bsdfSample = next2D(rng);
    hitInfo.bsdfLobeSample = next1D(rng);
    traceRayEXT(sceneBvh, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, rayOrigin, tMin, rayDirection, tMax, kPayloadIndex);
}

bool traceShadowRay(in vec3 rayOrigin, in float tMin, in vec3 rayDirection, in float tMax) {
    rayQueryEXT rayQuery;
    rayQueryInitializeEXT(
        rayQuery,
        sceneBvh,
        gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT,
        0xFF,
        rayOrigin,
        tMin,
        rayDirection,
        tMax);
    rayQueryProceedEXT(rayQuery);
    return rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT;
}

#endif // CRISP_PATH_TRACER_TRACING_GLSL
