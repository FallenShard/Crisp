#ifndef CRISP_PATH_TRACER_HENYEY_GREENSTEIN_PHASE_GLSL
#define CRISP_PATH_TRACER_HENYEY_GREENSTEIN_PHASE_GLSL

#include "isotropic.part.glsl"

// Uses directions of light travel: positive g scatters forward, around the incoming travel direction.
vec3 sampleHenyeyGreensteinPhase(
    const vec2 unitSample, const vec3 incomingDirection, const float anisotropy) {
    const float g = clamp(anisotropy, -0.999f, 0.999f);
    if (abs(g) < 1e-3f) {
        return sampleIsotropicPhase(unitSample);
    }

    const float ratio = (1.0f - g * g) / (1.0f + g - 2.0f * g * unitSample.x);
    const float cosTheta = clamp((1.0f + g * g - ratio * ratio) / (2.0f * g), -1.0f, 1.0f);
    const float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    const float phi = 2.0f * PI * unitSample.y;
    const vec3 localDirection = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
    return createCoordinateFrame(normalize(incomingDirection)) * localDirection;
}

float computeHenyeyGreensteinPhasePdf(
    const vec3 incomingDirection, const vec3 outgoingDirection, const float anisotropy) {
    const float g = clamp(anisotropy, -0.999f, 0.999f);
    if (abs(g) < 1e-3f) {
        return computeIsotropicPhasePdf();
    }
    const float cosTheta = dot(incomingDirection, outgoingDirection);
    const float denominator = max(1.0f + g * g - 2.0f * g * clamp(cosTheta, -1.0f, 1.0f), 1e-6f);
    return (1.0f - g * g) * (0.25f * InvPI) / (denominator * sqrt(denominator));
}

float evaluateHenyeyGreensteinPhase(
    const vec3 incomingDirection, const vec3 outgoingDirection, const float anisotropy) {
    return computeHenyeyGreensteinPhasePdf(incomingDirection, outgoingDirection, anisotropy);
}

#endif // CRISP_PATH_TRACER_HENYEY_GREENSTEIN_PHASE_GLSL
