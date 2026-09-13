#ifndef CRISP_PBR_SURFACE_GLSL
#define CRISP_PBR_SURFACE_GLSL

#include "../../BSDFs/OpenPbr/surface.part.glsl"

// PathTracedView's view onto the shared OpenPBR surface. The maths lives in BSDFs/OpenPbr/surface.part.glsl so
// that this tracer, the kBsdfOpenPbr callable and the analytic tracer's next-event-estimation switch cannot
// drift apart; these are adapters from the raster material record, nothing more.
//
// They go away with pbr-path-trace.* once PathTracedView moves onto the shared shaders.

#define PbrSurface OpenPbrSurface

PbrSurface createPbrSurface(const PbrMaterialParameters material, const uint energyCompensation) {
    return createOpenPbrSurface(material.surface, energyCompensation);
}

vec3 evaluatePbrSurface(const PbrSurface surface, const vec3 wi, const vec3 wo) {
    return evaluateOpenPbrSurface(surface, wi, wo);
}

float computePbrSurfacePdf(const PbrSurface surface, const vec3 wi, const vec3 wo) {
    return computeOpenPbrSurfacePdf(surface, wi, wo);
}

vec3 samplePbrSurface(
    const PbrSurface surface,
    const vec2 unitSample,
    const vec3 wi,
    out vec3 wo,
    out float pdf,
    out bool sampledSpecular) {
    return sampleOpenPbrSurface(surface, unitSample, wi, wo, pdf, sampledSpecular);
}

#endif // CRISP_PBR_SURFACE_GLSL
