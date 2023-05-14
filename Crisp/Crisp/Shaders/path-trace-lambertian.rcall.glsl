#version 460 core
#extension GL_EXT_ray_tracing : require

#define PI 3.1415926535897932384626433832795
#define InvPI 0.31830988618379067154
#define InvTwoPI 0.15915494309189533577f

struct BsdfSample
{
    // Input: rng sample.
    vec2 unitSample;
    vec2 pad0;

    // Input: normal at the hit location.
    vec3 normal;
    float pad1;

    // Output: new direction.
    vec3 sampleDirection;
    float samplePdf;

    vec3 eval;
    float pad2;
};

layout(location = 0) callableDataInEXT BsdfSample bsdfSample;

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
    const vec3 cwhSample = squareToCosineHemisphere(bsdfSample.unitSample);

    const vec3 normal = bsdfSample.normal;

    const vec3 upVector = abs(normal.y) < 0.999 ? vec3(0, 1, 0) : vec3(0, 0, 1);
    const vec3 tangentX = normalize(cross(upVector, normal));
    const vec3 tangentY = cross(normal, tangentX);

    bsdfSample.sampleDirection = tangentX * cwhSample.x + tangentY * cwhSample.y + normal * cwhSample.z;
    bsdfSample.samplePdf = InvTwoPI;
    bsdfSample.eval = vec3(InvPI);
}
