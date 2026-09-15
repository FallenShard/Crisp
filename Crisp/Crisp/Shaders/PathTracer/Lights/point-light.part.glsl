#ifndef CRISP_PATH_TRACER_POINT_LIGHT_GLSL
#define CRISP_PATH_TRACER_POINT_LIGHT_GLSL

#include "light-types.part.glsl"

LightSample samplePointLight(const vec3 position, const vec3 power, const vec3 refPoint) {
    LightSample ls;
    ls.isDelta = true;

    const vec3 lightVector = position - refPoint;
    const float squaredDistance = dot(lightVector, lightVector);
    if (squaredDistance <= 0.0f) {
        ls.direction = vec3(0.0f);
        ls.distance = 0.0f;
        ls.pdf = 0.0f;
        ls.weight = vec3(0.0f);
        return ls;
    }

    ls.distance = sqrt(squaredDistance);
    ls.direction = lightVector / ls.distance;
    ls.pdf = 1.0f; // Unit discrete mass; the continuous directional PDF is zero.
    ls.weight = power * (0.25f * InvPI) / squaredDistance;
    return ls;
}

vec3 evaluatePointLight() {
    return vec3(0.0f);
}

float computePointLightPdf() {
    return 0.0f;
}

#endif // CRISP_PATH_TRACER_POINT_LIGHT_GLSL
