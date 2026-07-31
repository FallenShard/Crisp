#ifndef CRISP_TONEMAP_GLSL_H
#define CRISP_TONEMAP_GLSL_H

struct TonemapParams {
    float exposure;
    int operatorIndex;
    float whitePoint;
    float contrast;

    float linearStart;
    float linearLength;
    float blackTightness;
    float pedestal;
};

// Must match kTonemapOperatorNames in Models/Tonemap.hpp.
const int kTonemapNone = 0;
const int kTonemapReinhard = 1;
const int kTonemapAces = 2;
const int kTonemapUchimura = 3;

// Reinhard et al. 2002, equation 4. whitePoint is the radiance that maps to 1, so anything brighter is allowed to
// clip instead of compressing the entire range towards grey the way the basic x / (1 + x) form does.
vec3 tonemapReinhard(const vec3 color, const float whitePoint) {
    const float w = max(1e-4f, whitePoint);
    return color * (1.0f + color / (w * w)) / (1.0f + color);
}

// Stephen Hill's fit of the ACES RRT + ODT. Preferred over the cheaper Narkowicz fit because it keeps the
// saturation rolloff, which is what lets a bright sky read as bright rather than clipping to white.
//
// Written transposed against the published matrices: GLSL constructors take columns, HLSL braces take rows.
const mat3 kAcesInput =
    mat3(0.59719f, 0.07600f, 0.02840f, 0.35458f, 0.90834f, 0.13383f, 0.04823f, 0.01566f, 0.83777f);

const mat3 kAcesOutput =
    mat3(1.60475f, -0.10208f, -0.00327f, -0.53108f, 1.10813f, -0.07276f, -0.07367f, -0.00605f, 1.07602f);

vec3 rrtAndOdtFit(const vec3 v) {
    const vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    const vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

vec3 tonemapAces(const vec3 color) {
    return clamp(kAcesOutput * rrtAndOdtFit(kAcesInput * color), 0.0f, 1.0f);
}

// Uchimura 2017, "HDR Theory and Practice", CEDEC. A toe, a straight midsection and a shoulder, with the linear
// part given directly rather than implied by a fit - which is the whole reason to prefer it when tuning by eye.
vec3 tonemapUchimura(const vec3 x, const TonemapParams params) {
    // Fixed at 1 because this pass outputs to an SDR display range; the sRGB encode downstream expects [0, 1].
    const float maxBrightness = 1.0f;

    const float a = params.contrast;
    const float m = max(1e-4f, params.linearStart);
    const float l = params.linearLength;
    const float c = params.blackTightness;
    const float b = params.pedestal;

    const float l0 = ((maxBrightness - m) * l) / max(1e-4f, a);
    const float shoulderStart = m + l0;
    const float shoulderBreak = m + a * l0;
    const float shoulderScale = (a * maxBrightness) / max(1e-4f, maxBrightness - shoulderBreak);

    const vec3 toeWeight = 1.0f - smoothstep(0.0f, m, x);
    const vec3 shoulderWeight = step(shoulderStart, x);
    const vec3 linearWeight = 1.0f - toeWeight - shoulderWeight;

    const vec3 toe = m * pow(max(x, vec3(1e-6f)) / m, vec3(c)) + b;
    const vec3 linear = m + a * (x - m);
    const vec3 shoulder =
        maxBrightness - (maxBrightness - shoulderBreak) * exp((-shoulderScale / maxBrightness) * (x - shoulderStart));

    return toe * toeWeight + linear * linearWeight + shoulder * shoulderWeight;
}

vec3 applyTonemap(const vec3 radiance, const TonemapParams params) {
    const vec3 exposed = max(vec3(0.0f), radiance * params.exposure);

    switch (params.operatorIndex) {
    case kTonemapReinhard:
        return tonemapReinhard(exposed, params.whitePoint);
    case kTonemapAces:
        return tonemapAces(exposed);
    case kTonemapUchimura:
        return tonemapUchimura(exposed, params);
    default:
        return clamp(exposed, 0.0f, 1.0f);
    }
}

#endif // CRISP_TONEMAP_GLSL_H
