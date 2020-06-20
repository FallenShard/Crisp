#version 460
#extension GL_NV_ray_tracing : require

struct hitPayload
{
    vec3 hitValue;
    float tHit;
};

layout(location = 0) rayPayloadInNV hitPayload prd;

void main()
{
    prd.hitValue = vec3(0.0, 0.1, 0.3);
    prd.tHit = -1;
}