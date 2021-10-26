#version 460 core

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(set = 0, binding = 0, rg32f) uniform readonly image2D srcImg;
layout(set = 0, binding = 1, rg32f) uniform writeonly image2D dstImg;

uint getGlobalIndex()
{
    uvec3 dim = gl_WorkGroupSize * gl_NumWorkGroups;
    return gl_GlobalInvocationID.z * dim.x * dim.y +
           gl_GlobalInvocationID.y * dim.x +
           gl_GlobalInvocationID.x;
}

layout(push_constant) uniform PushConstant
{
    int reverseHorizontal;
    int numN;
    float t;
};

#define PI 3.1415926535897932384626433832795

uint reverseBits(uint k, int N)
{
    uint r = 0;
    while (N > 0) {
        r = (r << 1) | (k & 1);
        k >>= 1;
        N--;
    }

    return r;
}

void main()
{
    //if (t > 0)
    //    return;

    if (reverseHorizontal == 1)
    {
        uint bitRev = reverseBits(uint(gl_GlobalInvocationID.x), numN);
        vec4 v = imageLoad(srcImg, ivec2(gl_GlobalInvocationID.xy));
        imageStore(dstImg, ivec2(bitRev, gl_GlobalInvocationID.y), v);
    }
    else
    {
        uint bitRev = reverseBits(uint(gl_GlobalInvocationID.y), numN);
        vec4 v = imageLoad(srcImg, ivec2(gl_GlobalInvocationID.xy));
        imageStore(dstImg, ivec2(gl_GlobalInvocationID.x, bitRev), v);

    }
}