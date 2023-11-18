#ifndef WARP_PART_GLSL
#define WARP_PART_GLSL

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

float squareToCosineHemispherePdf(vec3 sampleDir)
{
    return sampleDir.z * InvPI;
}

vec3 squareToUniformCylinder(const vec2 unitSample, float cosThetaMin, float cosThetaMax)
{
    float z = cosThetaMin + unitSample.y * (cosThetaMax - cosThetaMin);
    float phi = 2.0f * PI * unitSample.x;
    return vec3(cos(phi), sin(phi), z);
}

vec3 cylinderToSphereSection(const vec2 unitSample, float cosThetaMin, float cosThetaMax)
{
    vec3 cylPt = squareToUniformCylinder(unitSample, cosThetaMin, cosThetaMax);
    cylPt.xy *= sqrt(1.0f - cylPt.z * cylPt.z);
    return cylPt;
}

vec3 squareToUniformHemisphere(const vec2 unitSample)
{
    return cylinderToSphereSection(unitSample, 1.0f, 0.0f);
}

vec3 squareToUniformSphere(const vec2 unitSample)
{
    return cylinderToSphereSection(unitSample, 1.0f, -1.0f);
}

vec3 squareToUniformTriangle(const vec2 unitSample) {
    const float val = sqrt(unitSample.x);
    const float u = 1.0f - val;
    const float v = unitSample.y * val;
    return vec3(u, v, 1.0f - u - v);
}

#endif