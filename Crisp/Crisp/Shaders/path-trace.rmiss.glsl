#version 460
#extension GL_NV_ray_tracing : require

struct hitPayload
{
    vec3 hitPos;
    float tHit;
    vec3 sampleDir;
    float pdf;
    vec3 Le;
    uint bounceCount;
    vec3 bsdf;
    float pad2;
    vec4 debugValue;
};

layout(location = 0) rayPayloadInNV hitPayload prd;

void main()
{
    prd.hitPos = vec3(0.0);
    prd.tHit = -1;
    prd.debugValue = vec4(0.0);
}