#ifndef CRISP_PATH_TRACER_AREA_LIGHT_GLSL
#define CRISP_PATH_TRACER_AREA_LIGHT_GLSL

vec3 evalAreaLight(vec3 p, vec3 n, vec3 radiance) {
    const vec3 ref = gl_WorldRayOriginEXT;
    const vec3 wi = p - ref;
    const float cosTheta = dot(n, normalize(-wi));
    return cosTheta <= 0.0f ? vec3(0.0f) : radiance;
}

#endif // CRISP_PATH_TRACER_AREA_LIGHT_GLSL
