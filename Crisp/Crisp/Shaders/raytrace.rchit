#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

struct hitPayload
{
    vec3 hitValue;
    float tHit;
};

layout(location = 0) rayPayloadInNV hitPayload prd;
hitAttributeNV vec3 attribs;

void main()
{
    prd.hitValue = vec3(0.7, 0.5, 0.5);
    prd.tHit = gl_HitTNV;
}
