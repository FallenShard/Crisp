#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Parts/path-tracer-payload.part.glsl"
#include "Parts/math-constants.part.glsl"

layout(location = 0) callableDataInEXT LambertianBsdfSample bsdf;

vec3 squareToCosineHemisphere(vec2 unitSample)
{
    vec3 dir;
    float radius = sqrt(unitSample.y);
    float theta = 2.0f * PI * unitSample.x;
    dir.x = radius * cos(theta);
    dir.y = radius * sin(theta);
    dir.z = sqrt(max(0.0f, 1.0f - dir.x * dir.x - dir.y  * dir.y ));
    return dir;
}

void main()
{
    const vec3 cwhSample = squareToCosineHemisphere(bsdf.unitSample);

    const vec3 normal = bsdf.normal;

    const vec3 upVector = abs(normal.y) < 0.999 ? vec3(0, 1, 0) : vec3(0, 0, 1);
    const vec3 tangentX = normalize(cross(upVector, normal));
    const vec3 tangentY = cross(normal, tangentX);

    bsdf.sampleDirection = tangentX * cwhSample.x + tangentY * cwhSample.y + normal * cwhSample.z;
    bsdf.samplePdf = InvTwoPI;
    bsdf.eval = vec3(InvPI);
}
