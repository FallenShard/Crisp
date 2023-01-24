#version 460 core

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(set = 0, binding = 0, rg32f) uniform readonly image2D srcImg;
layout(set = 0, binding = 1, rg32f) uniform writeonly image2D dstImg;

layout(push_constant) uniform PushConstant
{
    int reverseHorizontal;
    int logN;
};

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
    if (reverseHorizontal == 0)
    {
        uint bitRev = reverseBits(uint(gl_GlobalInvocationID.x), logN);
        vec4 v = imageLoad(srcImg, ivec2(gl_GlobalInvocationID.xy));
        imageStore(dstImg, ivec2(bitRev, gl_GlobalInvocationID.y), v);
    }
    else
    {
        uint bitRev = reverseBits(uint(gl_GlobalInvocationID.y), logN);
        vec4 v = imageLoad(srcImg, ivec2(gl_GlobalInvocationID.xy));
        imageStore(dstImg, ivec2(gl_GlobalInvocationID.x, bitRev), v);
    }
}