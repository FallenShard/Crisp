#version 460 core
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

struct hitPayload
{
    vec3 hitValue;
    float tHit;
};

layout(location = 0) rayPayloadInEXT hitPayload prd;
// hitAttributeNV vec3 attribs;

void main()
{
    if (gl_PrimitiveID == 0) {
        prd.hitValue = vec3(0.7, 0.5, 0.7);
    }
    else {
        prd.hitValue = vec3(0, 0, 0.7);
    }
    
    prd.tHit = gl_HitTEXT;
}
