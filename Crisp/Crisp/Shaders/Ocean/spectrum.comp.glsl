#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "../Common/math-constants.part.glsl"

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

// One array layer per cascade; the dispatch runs once per cascade with its own band limits.
layout(set = 0, binding = 0, rg32f) uniform readonly image2DArray initialSpectrumImg;
// IFFT is linear, so two real fields ride in one complex transform, and an rgba texel carries two
// independent transforms in .rg and .ba. Eight real fields -- height, two displacements, two slopes,
// three displacement gradients -- fill two textures exactly.
//
// After the transform: displacement = (height, dispX, dispZ, slopeX)
//                      jacobian     = (slopeZ, dDx/dx, dDz/dz, dDx/dz)
// The split is chosen so ocean.vert reads height and both displacements in a single fetch.
layout(set = 0, binding = 1, rgba32f) uniform writeonly image2DArray packedDisplacementImg;
layout(set = 0, binding = 2, rgba32f) uniform writeonly image2DArray packedJacobianImg;

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

    // kMin is the previous cascade's Nyquist, so the bands neither overlap nor gap.
    float kMin;
    float kMax;
    int cascade;

    int spectrumModel;
    float fetch;
    float peakEnhancement;
    float directionalSpread;
    // Computed on the CPU: GLSL has no gamma function, and it only changes with the spread.
    float directionalSpreadNormalization;
};

// Must match OceanSpectrumModel in Crisp/Models/Ocean.hpp.
#define OCEAN_SPECTRUM_PHILLIPS 0
#define OCEAN_SPECTRUM_PIERSON_MOSKOWITZ 1
#define OCEAN_SPECTRUM_JONSWAP 2

const float g = 9.81;

vec2 complexMul(vec2 z1, vec2 z2) {
    return vec2(z1[0] * z2[0] - z1[1] * z2[1], z1[0] * z2[1] + z1[1] * z2[0]);
}

// Tessendorf's 1986 placeholder. Its cos^2 term is symmetric in k, so waves travelling against the
// wind carry exactly as much energy as waves travelling with it.
float phillipsDensity(const vec2 kDir, const float kLen2) {
    const float expTerm = exp(-1.0 / (kLen2 * Lw * Lw)) / (kLen2 * kLen2);
    const float kDotW = dot(kDir, windDirection);
    // Squared by hand: pow(x, y) is undefined in GLSL for x < 0, and kDotW < 0 upwind.
    return expTerm * (kDotW * kDotW);
}

// A frequency spectrum becomes a wavenumber one through the deep-water dispersion Jacobian:
// S(k) = S(w) * (dw/dk) / k. Horvath, Empirical Directional Wave Spectra for Computer Graphics,2015.
float dispersionJacobian(const float kLen, const float omega) {
    return 0.5f * sqrt(g / kLen) / kLen;
}

// Fully developed sea: the peak depends on wind alone, with no fetch limit.
float piersonMoskowitzDensity(const float kLen) {
    const float omega = sqrt(g * kLen);
    const float omegaPeak = 0.855f * g / max(windSpeed, 0.1f);
    const float omega5 = omega * omega * omega * omega * omega;
    const float shape = 0.0081f * g * g / omega5 * exp(-1.25f * pow(omegaPeak / omega, 4.0f));
    return shape * dispersionJacobian(kLen, omega);
}

// Fetch-limited, and sharper at the peak than Pierson-Moskowitz by the gamma enhancement.
float jonswapDensity(const float kLen) {
    const float omega = sqrt(g * kLen);
    const float speed = max(windSpeed, 0.1f);
    const float dimensionlessFetch = g * max(fetch, 1.0f) / (speed * speed);

    const float alpha = 0.076f * pow(dimensionlessFetch, -0.22f);
    const float omegaPeak = 22.0f * pow(g * g / (speed * max(fetch, 1.0f)), 1.0f / 3.0f);

    const float sigma = omega <= omegaPeak ? 0.07f : 0.09f;
    const float relative = (omega - omegaPeak) / (sigma * omegaPeak);
    const float peak = pow(peakEnhancement, exp(-0.5f * relative * relative));

    const float omega5 = omega * omega * omega * omega * omega;
    const float shape = alpha * g * g / omega5 * exp(-1.25f * pow(omegaPeak / omega, 4.0f));
    return shape * peak * dispersionJacobian(kLen, omega);
}

// cos^2s(theta/2) is one-sided, unlike Phillips' cos^2, so waves running against the wind carry
// almost nothing. Normalised to integrate to one over all directions, so it redistributes the
// frequency spectrum's energy rather than adding to it.
float directionalSpreadDensity(const vec2 kDir) {
    const float cosHalfSquared = max(0.5f * (1.0f + dot(kDir, windDirection)), 0.0f);
    return directionalSpreadNormalization * pow(cosHalfSquared, directionalSpread);
}

float calculateSpectrum(const vec2 k) {
    const float kLen2 = dot(k, k);
    if (kLen2 == 0.0f) {
        return 0.0f;
    }

    // The band limit is symmetric in k, so it leaves the Hermitian pairing intact.
    const float kLen = sqrt(kLen2);
    if (kLen < kMin || kLen >= kMax) {
        return 0.0f;
    }

    const vec2 kDir = k * inversesqrt(kLen2);

    float density = 0.0f;
    if (spectrumModel == OCEAN_SPECTRUM_PIERSON_MOSKOWITZ) {
        density = piersonMoskowitzDensity(kLen) * directionalSpreadDensity(kDir);
    } else if (spectrumModel == OCEAN_SPECTRUM_JONSWAP) {
        density = jonswapDensity(kLen) * directionalSpreadDensity(kDir);
    } else {
        density = phillipsDensity(kDir, kLen2);
    }

    // A stays a pure gain on every model, which is what lets the amplitude slider rescale the
    // integrated moments instead of re-integrating them.
    return A * density * exp(-kLen2 * smallWaves * smallWaves);
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
    const vec2 h0 =
        imageLoad(initialSpectrumImg, ivec3(gid, cascade)).xy * sqrtFactor * sqrt(calculateSpectrum(k));
    const vec2 h0MinusK =
        imageLoad(initialSpectrumImg, ivec3(mirrorGid, cascade)).xy * sqrtFactor * sqrt(calculateSpectrum(-k));
    const vec2 h0Conj = vec2(h0MinusK.x, -h0MinusK.y);

    const float wk = sqrt(g * kLen);
    const float phase = mod(wk * time, 2.0f * PI);
    const vec2 phaseVec = vec2(cos(phase), sin(phase));

    const vec2 hkt = complexMul(h0, phaseVec) + complexMul(h0Conj, vec2(phaseVec.x, -phaseVec.y));

    // The negative Nyquist aliases its positive twin; odd derivatives cannot stay Hermitian there.
    const vec2 dispX = gid.x == 0 ? vec2(0.0f) : complexMul(hkt, vec2(0, -k.x * invKLen));
    const vec2 dispZ = gid.y == 0 ? vec2(0.0f) : complexMul(hkt, vec2(0, -k.y * invKLen));
    const vec2 slopeX = gid.x == 0 ? vec2(0.0f) : complexMul(hkt, vec2(0, k.x));
    const vec2 slopeZ = gid.y == 0 ? vec2(0.0f) : complexMul(hkt, vec2(0, k.y));

    // Displacement gradients, exact where the fragment shader used to finite-difference eight taps.
    // Differentiating D(k) = -i*(k/|k|)*h~ is a multiply by i*k, and the two i's cancel, so each one
    // is a real multiple of h~. Those multipliers are even in k, so unlike the displacements above
    // these stay Hermitian at the Nyquist rows and need no zeroing. dDx/dz and dDz/dx collapse to
    // the same field. See docs/ocean.md item 18.
    // Zeroed wherever the displacement they differentiate was zeroed, or they would describe the
    // gradient of a field that is not the one being applied.
    const vec2 jacobianXx = gid.x == 0 ? vec2(0.0f) : hkt * (k.x * k.x * invKLen);
    const vec2 jacobianZz = gid.y == 0 ? vec2(0.0f) : hkt * (k.y * k.y * invKLen);
    const vec2 jacobianXz = (gid.x == 0 || gid.y == 0) ? vec2(0.0f) : hkt * (k.x * k.y * invKLen);

    // i*B = i*(Br + i*Bi) = -Bi + i*Br.
    const vec2 packedHeightDispX = vec2(hkt.x - dispX.y, hkt.y + dispX.x);
    const vec2 packedDispZSlopeX = vec2(dispZ.x - slopeX.y, dispZ.y + slopeX.x);
    const vec2 packedSlopeZJacobianXx = vec2(slopeZ.x - jacobianXx.y, slopeZ.y + jacobianXx.x);
    const vec2 packedJacobianZzXz = vec2(jacobianZz.x - jacobianXz.y, jacobianZz.y + jacobianXz.x);

    imageStore(packedDisplacementImg, ivec3(gid, cascade), vec4(packedHeightDispX, packedDispZSlopeX));
    imageStore(packedJacobianImg, ivec3(gid, cascade), vec4(packedSlopeZJacobianXx, packedJacobianZzXz));
}
