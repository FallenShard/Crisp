#version 460
#extension GL_EXT_ray_tracing : enable

struct HitInfo
{
    vec3 position;
    float tHit;

    vec3 sampleDirection;
    float samplePdf;

    vec3 Le;
    uint bounceCount;

    vec3 bsdfEval;
    uint rngSeed;

    vec4 debugValue;
};

layout(location = 0) rayPayloadInEXT HitInfo hitInfo;

void main()
{
    hitInfo.tHit = -1;
}