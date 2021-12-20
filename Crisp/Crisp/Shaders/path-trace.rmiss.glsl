#version 460
#extension GL_EXT_ray_tracing : enable

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

layout(location = 0) rayPayloadInEXT hitPayload prd;

void main()
{
    prd.hitPos = vec3(0.0);
    prd.tHit = -1;
    prd.debugValue = vec4(0.0);
}