#ifndef CRISP_LAMBERTIAN_GLSL
#define CRISP_LAMBERTIAN_GLSL

vec3 sampleLambertian(vec2 unitSample)
{
    const float radius = sqrt(unitSample.y);
    const float theta = 2.0f * PI * unitSample.x;
    return vec3(
        radius * cos(theta),
        radius * sin(theta),
        sqrt(max(0.0f, 1.0f - unitSample.y)));
}

float lambertianPdf(vec3 wi, vec3 wo)
{
    if (wi.z <= 0.0f || wo.z <= 0.0f) {
        return 0.0f;
    }

    return wo.z / PI;
}

vec3 evaluateLambertian(vec3 albedo, vec3 wi, vec3 wo)
{
    if (wi.z <= 0.0f || wo.z <= 0.0f) {
        return vec3(0.0f);
    }

    return albedo / PI * wo.z;
}

#endif // CRISP_LAMBERTIAN_GLSL
