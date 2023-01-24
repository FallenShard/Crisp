#version 460 core

#define PI 3.1415926535897932384626433832795

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(set = 0, binding = 0, rgba32f) uniform readonly image2D initialSpectrumImg;
layout(set = 0, binding = 1, rg32f) uniform writeonly image2D dispYImg;
layout(set = 0, binding = 2, rg32f) uniform writeonly image2D dispXImg;
layout(set = 0, binding = 3, rg32f) uniform writeonly image2D dispZImg;
layout(set = 0, binding = 4, rg32f) uniform writeonly image2D normalXImg;
layout(set = 0, binding = 5, rg32f) uniform writeonly image2D normalZImg;

layout(push_constant) uniform PushConstant
{
    int N;
    int M;
    float Lx;
    float Lz;

    vec2 windDirection;
    float windSpeed;
    float Lw;

    float A;
    float smallWaves;

    float time;
    float pad0;
};

const float g = 9.81;

vec2 complexMul(vec2 z1, vec2 z2)
{
    return vec2(z1[0] * z2[0] - z1[1] * z2[1],
                z1[0] * z2[1] + z1[1] * z2[0]);
}

float calculatePhillipsSpectrum(const vec2 k)
{
    const float kLen2 = dot(k, k) + 0.000001f;
    const vec2 kDir = kLen2 == 0.0f ? vec2(0.0f) : k / sqrt(kLen2);

    const float expTerm = exp(-1.0 / (kLen2 * Lw * Lw)) / pow(kLen2, 2);
    const float kDotW = dot(kDir, windDirection);

    const float tail = exp(-kLen2 * smallWaves * smallWaves);

    return A * expTerm * pow(kDotW, 2) * tail;
}

void main()
{
    const ivec2 idx = ivec2(gl_GlobalInvocationID.xy) - ivec2(N, M) / 2;
    const vec2 k = vec2(idx) * 2.0f * PI / vec2(Lx, Lz);
    const float kLen = sqrt(dot(k, k)) + 0.000001f;

    // Initial spectrum contains uniform gaussian samples, 2 + 2 components.
    const vec4 initialSpectrum = imageLoad(initialSpectrumImg, ivec2(gl_GlobalInvocationID.xy));
    const float sqrtFactor = sqrt(2.0f) * 0.5f;
    const vec2 h0 = initialSpectrum.xy * sqrtFactor * sqrt(calculatePhillipsSpectrum(k));
    vec2 h0_star = initialSpectrum.zw * sqrtFactor * sqrt(calculatePhillipsSpectrum(-k));
    h0_star.y = -h0_star.y;

    // The dispersion relation.
    const float wk = sqrt(g * kLen);
    const float phase = mod(wk * time, 2.0f * PI);
    const vec2 phaseVec = vec2(cos(phase), sin(phase));

    const vec2 hkt = complexMul(h0, phaseVec) + complexMul(h0_star, vec2(phaseVec.x, -phaseVec.y));
    imageStore(dispYImg, ivec2(gl_GlobalInvocationID.xy), vec4(hkt, 0.0, 0.0f));

    const vec2 dispX = complexMul(hkt, vec2(0, -k.x / kLen));
    const vec2 dispZ = complexMul(hkt, vec2(0, -k.y / kLen));
    imageStore(dispXImg, ivec2(gl_GlobalInvocationID.xy), vec4(dispX, 0.0, 0.0f));
    imageStore(dispZImg, ivec2(gl_GlobalInvocationID.xy), vec4(dispZ, 0.0, 0.0f));

    const vec2 normalX = complexMul(hkt, vec2(0, k.x));
    const vec2 normalZ = complexMul(hkt, vec2(0, k.y));
    imageStore(normalXImg, ivec2(gl_GlobalInvocationID.xy), vec4(normalX, 0.0, 0.0f));
    imageStore(normalZImg, ivec2(gl_GlobalInvocationID.xy), vec4(normalZ, 0.0, 0.0f));
}