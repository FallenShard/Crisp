#ifndef CRISP_PATH_TRACER_ISOTROPIC_PHASE_GLSL
#define CRISP_PATH_TRACER_ISOTROPIC_PHASE_GLSL

#include "../../Common/math-constants.part.glsl"
#include "../../Common/warp.part.glsl"

vec3 sampleIsotropicPhase(const vec2 unitSample) {
    return squareToUniformSphere(unitSample);
}

float computeIsotropicPhasePdf() {
    return squareToUniformSpherePdf();
}

float evaluateIsotropicPhase() {
    return computeIsotropicPhasePdf();
}

#endif // CRISP_PATH_TRACER_ISOTROPIC_PHASE_GLSL
