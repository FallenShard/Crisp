#ifndef CRISP_PATH_TRACER_POINT_LIGHT_GLSL
#define CRISP_PATH_TRACER_POINT_LIGHT_GLSL

vec3 samplePointLight(
    const vec3 position,
    const vec3 power,
    const vec3 refPoint,
    out vec3 shadowRayDir,
    out float shadowRayLen,
    out float lightPdf) {
    const vec3 lightVector = position - refPoint;
    const float squaredDistance = dot(lightVector, lightVector);
    if (squaredDistance <= 0.0f) {
        shadowRayDir = vec3(0.0f);
        shadowRayLen = 0.0f;
        lightPdf = 0.0f;
        return vec3(0.0f);
    }

    shadowRayLen = sqrt(squaredDistance);
    shadowRayDir = lightVector / shadowRayLen;
    lightPdf = 1.0f; // Unit discrete mass; the continuous directional PDF is zero.
    return power * (0.25f * InvPI) / squaredDistance;
}

#endif // CRISP_PATH_TRACER_POINT_LIGHT_GLSL
