#version 460 core

#define PI 3.1415926535897932384626433832795

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(set = 0, binding = 0, rg32f) uniform readonly image2D initialSpectrumImg;
// IFFT is linear, so two real fields ride in one complex transform. normalZ is left unpaired.
layout(set = 0, binding = 1, rg32f) uniform writeonly image2D packedHeightDispXImg;
layout(set = 0, binding = 2, rg32f) uniform writeonly image2D packedDispZNormalXImg;
layout(set = 0, binding = 3, rg32f) uniform writeonly image2D normalZImg;

layout(push_constant) uniform PushConstant {
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

vec2 complexMul(vec2 z1, vec2 z2) {
    return vec2(z1[0] * z2[0] - z1[1] * z2[1], z1[0] * z2[1] + z1[1] * z2[0]);
}

float calculatePhillipsSpectrum(const vec2 k) {
    const float kLen2 = dot(k, k);
    if (kLen2 == 0.0f) {
        return 0.0f;
    }
    const vec2 kDir = k * inversesqrt(kLen2);

    const float expTerm = exp(-1.0 / (kLen2 * Lw * Lw)) / (kLen2 * kLen2);
    const float kDotW = dot(kDir, windDirection);

    const float tail = exp(-kLen2 * smallWaves * smallWaves);

    // Squared by hand: pow(x, y) is undefined in GLSL for x < 0, and kDotW < 0 upwind.
    return A * expTerm * (kDotW * kDotW) * tail;
}

void main() {
    const ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    const ivec2 idx = gid - ivec2(N, M) / 2;
    const vec2 k = vec2(idx) * 2.0f * PI / vec2(Lx, Lz);
    const float kLen = length(k);
    const float invKLen = kLen > 0.0f ? 1.0f / kLen : 0.0f;

    // Cancels the inverse transform's normalisation so `A` stays the Phillips constant.
    const float dkx = 2.0f * PI / Lx;
    const float dkz = 2.0f * PI / Lz;
    const float amplitudeScale = float(N) * float(M) * sqrt(dkx * dkz);

    // The conjugate term must use the pair drawn for -k, or h~ is not Hermitian and the IFFT is complex.
    const ivec2 mirrorGid = ivec2((N - gid.x) % N, (M - gid.y) % M);

    const float sqrtFactor = sqrt(2.0f) * 0.5f * amplitudeScale;
    const vec2 h0 = imageLoad(initialSpectrumImg, gid).xy * sqrtFactor * sqrt(calculatePhillipsSpectrum(k));
    const vec2 h0MinusK = imageLoad(initialSpectrumImg, mirrorGid).xy * sqrtFactor * sqrt(calculatePhillipsSpectrum(-k));
    const vec2 h0Conj = vec2(h0MinusK.x, -h0MinusK.y);

    const float wk = sqrt(g * kLen);
    const float phase = mod(wk * time, 2.0f * PI);
    const vec2 phaseVec = vec2(cos(phase), sin(phase));

    const vec2 hkt = complexMul(h0, phaseVec) + complexMul(h0Conj, vec2(phaseVec.x, -phaseVec.y));

    // The negative Nyquist aliases its positive twin; odd derivatives cannot stay Hermitian there.
    const vec2 dispX = gid.x == 0 ? vec2(0.0f) : complexMul(hkt, vec2(0, -k.x * invKLen));
    const vec2 dispZ = gid.y == 0 ? vec2(0.0f) : complexMul(hkt, vec2(0, -k.y * invKLen));
    const vec2 normalX = gid.x == 0 ? vec2(0.0f) : complexMul(hkt, vec2(0, k.x));
    const vec2 normalZ = gid.y == 0 ? vec2(0.0f) : complexMul(hkt, vec2(0, k.y));

    // i*B = i*(Br + i*Bi) = -Bi + i*Br.
    const vec2 packedHeightDispX = vec2(hkt.x - dispX.y, hkt.y + dispX.x);
    const vec2 packedDispZNormalX = vec2(dispZ.x - normalX.y, dispZ.y + normalX.x);

    imageStore(packedHeightDispXImg, gid, vec4(packedHeightDispX, 0.0, 0.0f));
    imageStore(packedDispZNormalXImg, gid, vec4(packedDispZNormalX, 0.0, 0.0f));
    imageStore(normalZImg, gid, vec4(normalZ, 0.0, 0.0f));
}
