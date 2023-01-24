#version 460 core

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(set = 0, binding = 0, rg32f) uniform readonly image2D srcImg;
layout(set = 0, binding = 1, rg32f) uniform writeonly image2D dstImg;

layout(push_constant) uniform PushConstant
{
    layout(offset = 0) int passIdx;
    layout(offset = 4) int N;
};

#define PI 3.1415926535897932384626433832795

vec2 compMul(vec2 z, vec2 w)
{
    return vec2(z[0] * w[0] - z[1] * w[1], z[0] * w[1] + z[1] * w[0]);
}

void main()
{
    const int p = passIdx - 1;
    const int m = (1 << passIdx);
    const int k = int(gl_GlobalInvocationID.x);

    const int j = k % (1 << p);
    int leftIdx = k / (1 << p);
    leftIdx = m * leftIdx + j;
    const int rightIdx = leftIdx + m / 2; 

    // bit reverse in the first pass
    const int readLeftIdx = leftIdx;
    const int readRightIdx = rightIdx;
    float factor = 1.0;
    if (p == 0)
    {
        factor = 1.0 / N;
    }

    vec2 ww = vec2(1.0f, 0.0f);
    ww[0] = +cos(2.0f * PI / m * j);
    ww[1] = +sin(2.0f * PI / m * j);
    vec4 aa1 = imageLoad(srcImg, ivec2(readRightIdx, gl_GlobalInvocationID.y)) * factor;
    vec4 aa2 = imageLoad(srcImg, ivec2(readLeftIdx, gl_GlobalInvocationID.y)) * factor;
    vec2 t = compMul(ww, aa1.xy);
    vec2 u = aa2.xy;

    imageStore(dstImg, ivec2(rightIdx, gl_GlobalInvocationID.y), vec4(u - t, 0.0f, 0.0f));
    imageStore(dstImg, ivec2(leftIdx, gl_GlobalInvocationID.y), vec4(u + t, 0.0f, 0.0f));
}
