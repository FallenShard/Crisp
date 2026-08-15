#ifndef CRISP_PATH_TRACER_TRACING_GLSL
#define CRISP_PATH_TRACER_TRACING_GLSL

void traceRay(inout uint seed, in vec3 rayOrigin, in float tMin, in vec3 rayDirection, in float tMax) {
    hitInfo.rngSeed = seed;
    traceRayEXT(sceneBvh, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, rayOrigin, tMin, rayDirection, tMax, kPayloadIndex);
    seed = hitInfo.rngSeed;
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
