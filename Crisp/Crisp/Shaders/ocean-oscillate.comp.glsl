#version 460 core

#define PI 3.1415926535897932384626433832795

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(set = 0, binding = 0, rgba32f) uniform readonly image2D initialSpectrumImg;
layout(set = 0, binding = 1, rg32f) uniform writeonly image2D dstImg;
layout(set = 0, binding = 2, rg32f) uniform writeonly image2D dispXImg;
layout(set = 0, binding = 3, rg32f) uniform writeonly image2D dispZImg;
layout(set = 0, binding = 4, rg32f) uniform writeonly image2D normalXImg;
layout(set = 0, binding = 5, rg32f) uniform writeonly image2D normalZImg;

layout(push_constant) uniform PushConstant
{
    float time;
    int N;
    int M;
    float Lx;
    float Lz;
    float windSpeedX;
    float windSpeedZ;
    float windSpeedNormX;
    float windSpeedNormZ;
    float windMagnitude;
    float Lw;
    float A;
    float smallWaves;
};

const int logTable32[32] =
{
     0,  9,  1, 10, 13, 21,  2, 29,
    11, 14, 16, 18, 22, 25,  3, 30,
     8, 12, 20, 28, 15, 17, 24,  7,
    19, 27, 23,  6, 26,  5,  4, 31
};

int log2_32(uint value)
{
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return logTable32[uint(value * 0x07C4ACDD) >> 27];
}

uint reverseBits(uint k, int numBits)
{
    uint r = 0;
    while (numBits > 0) {
        r = (r << 1) | (k & 1);
        k >>= 1;
        numBits--;
    }

    return r;
}

const float g = 9.81;

vec2 complexMul(vec2 z1, vec2 z2)
{
    return vec2(z1[0] * z2[0] - z1[1] * z2[1],
                z1[0] * z2[1] + z1[1] * z2[0]);
}

struct Wind
{
    vec2 speed;
    vec2 speedNorm;
    float magnitude;
    float Lw;
};

float calculatePhillipsSpectrum(const Wind wind, const vec2 k)
{
    const float kLen2 = dot(k, k) + 0.000001;
    const vec2 kNorm = kLen2 == 0.0f ? vec2(0.0f) : k / sqrt(kLen2);

    const float midterm = exp(-1.0 / (kLen2 * wind.Lw * wind.Lw)) / pow(kLen2, 2);
    const float kDotW = dot(kNorm, wind.speedNorm);

    const float tail = exp(-kLen2 * smallWaves * smallWaves);

    return A * midterm * pow(kDotW, 2) * tail;
}

void main()
{
    ivec2 idx = ivec2(gl_GlobalInvocationID.xy) - ivec2(N, M) / 2;
    vec2 k = vec2(idx) * 2.0f * PI / vec2(Lx, Lz);
    const float kLen = sqrt(dot(k, k)) + 0.00001;

    // Initial spectrum packed as h0(k) and h0_star(-k), 2 + 2 components
    const vec4 initialSpectrum = imageLoad(initialSpectrumImg, ivec2(gl_GlobalInvocationID.xy));

    vec2 h0 = initialSpectrum.xy;
    vec2 h0_star = initialSpectrum.zw;

    Wind wind;
    wind.speed = vec2( windSpeedX, windSpeedZ );
    wind.magnitude = windMagnitude;
    wind.speedNorm = wind.speed / wind.magnitude;
    wind.Lw = wind.magnitude * wind.magnitude / g;

    h0 = h0 * 1.0f / sqrt(2.0f) * sqrt(calculatePhillipsSpectrum(wind, k));
    h0_star = h0_star * 1.0f / sqrt(2.0f) * sqrt(calculatePhillipsSpectrum(wind, -k));
    h0_star.y = -h0_star.y;

    // The dispersion relation
    const float wk = sqrt(g * kLen);
    const float phase = mod(wk * time, 2.0f * PI);
    const vec2 phaseVec = vec2(cos(phase), sin(phase));

    const vec2 hkt = complexMul(h0, phaseVec) + complexMul(h0_star, vec2(phaseVec.x, -phaseVec.y));
    imageStore(dstImg, ivec2(gl_GlobalInvocationID.xy), vec4(hkt, 0.0, 0.0f));

    vec2 dispX = complexMul(hkt * k.x / kLen, vec2(0, -1));
    vec2 dispZ = complexMul(hkt * k.y / kLen, vec2(0, -1));
    imageStore(dispXImg, ivec2(gl_GlobalInvocationID.xy), vec4(dispX, 0.0, 0.0f));
    imageStore(dispZImg, ivec2(gl_GlobalInvocationID.xy), vec4(dispZ, 0.0, 0.0f));

    vec2 normalX = complexMul(hkt * k.x, vec2(0, 1));
    vec2 normalZ = complexMul(hkt * k.y, vec2(0, 1));
    imageStore(normalXImg, ivec2(gl_GlobalInvocationID.xy), vec4(normalX, 0.0, 0.0f));
    imageStore(normalZImg, ivec2(gl_GlobalInvocationID.xy), vec4(normalZ, 0.0, 0.0f));
}