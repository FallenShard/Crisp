#version 460 core
#extension GL_EXT_ray_tracing : enable

struct hitPayload
{
    vec3 hitValue;
    float tHit;
};

layout(location = 0) rayPayloadInEXT hitPayload prd;

void main()
{
    prd.hitValue = vec3(0.0, 0.1, 1.3);
    prd.tHit = -1;
}