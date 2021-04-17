#version 460 core

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(set = 0, binding = 0, rg32f) uniform readonly image2D srcImg;
layout(set = 0, binding = 1, rg32f) uniform writeonly image2D dstImg;

layout(push_constant) uniform PushConstant
{
    layout(offset = 0)  int passId;
    layout(offset = 4)  float t;
    layout(offset = 8)  int numN;
};

#define PI 3.1415926535897932384626433832795

vec2 compMul(vec2 z, vec2 w)
{
    return vec2(z[0] * w[0] - z[1] * w[1], z[0] * w[1] + z[1] * w[0]);
}

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
    // if (gl_GlobalInvocationID.y >= 256 || gl_GlobalInvocationID.x >= 256)
    //     return;

    // ivec2 coords = ivec2(gl_GlobalInvocationID.xy);

    // float phi = gl_GlobalInvocationID.x / 255.0;

    // float w = passId * 2 * 3.141592 * gl_GlobalInvocationID.x / 255.0;
    // float wt = w + passId * t * 8;

    // float offset = 0.0f;// sin(wt);

    //vec4 data = imageLoad(srcImg, coords);
    //imageStore(dstImg, coords, data + vec4(offset));

    int p = passId - 1;

   

    int m = (1 << passId);

    int k = int(gl_GlobalInvocationID.x);

    int j = k % (1 << p);
    int leftIdx = k / (1 << p);
    leftIdx = m * leftIdx + j;
    int rightIdx = leftIdx + m / 2; 

    // bit reverse in the first pass
    int readLeftIdx = leftIdx;
    int readRightIdx = rightIdx;
    float factor = 1.0;
    if (p == 0)
    {
        factor = 1.0 / numN;
        //readLeftIdx = int(reverseBits(uint(leftIdx), numN));
        //readRightIdx = int(reverseBits(uint(rightIdx), numN));
    }

    vec2 ww = vec2(1.0f, 0.0f);
    ww[0] = +cos(2.0f * PI / m * j);
    ww[1] = +sin(2.0f * PI / m * j);
    vec4 aa1 = imageLoad(srcImg, ivec2(readRightIdx, gl_GlobalInvocationID.y)) * factor;
    vec4 aa2 = imageLoad(srcImg, ivec2(readLeftIdx, gl_GlobalInvocationID.y)) * factor;
    vec2 t = compMul(ww, aa1.xy);
    vec2 u = aa2.xy;
    //A[k + j] = u + t;
    //A[k + j + m / 2] = u - t;
    imageStore(dstImg, ivec2(rightIdx, gl_GlobalInvocationID.y), vec4(u - t, 0.0f, 0.0f));
    imageStore(dstImg, ivec2(leftIdx, gl_GlobalInvocationID.y), vec4(u + t, 0.0f, 0.0f));
}
