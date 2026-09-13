#ifndef CRISP_MICROFACET_BECKMANN_GLSL
#define CRISP_MICROFACET_BECKMANN_GLSL

#include "common.part.glsl"

vec3 sampleBeckmannNormal(vec2 unitSample, float alpha) {
    const float tanThetaSquared = -alpha * alpha * log(max(1.0f - unitSample.y, 1e-7f));
    const float cosTheta = inversesqrt(1.0f + tanThetaSquared);
    const float phi = 2.0f * PI * unitSample.x;
    const float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float beckmannErf(const float value) {
    const float signValue = value < 0.0f ? -1.0f : 1.0f;
    const float x = abs(value);
    const float t = 1.0f / (1.0f + 0.3275911f * x);
    const float polynomial =
        (((((1.061405429f * t - 1.453152027f) * t) + 1.421413741f) * t - 0.284496736f) * t +
          0.254829592f) *
         t;
    return signValue * (1.0f - polynomial * exp(-x * x));
}

float beckmannInverseErf(const float value) {
    const float x = clamp(value, -0.999999f, 0.999999f);
    float w = -log((1.0f - x) * (1.0f + x));
    float polynomial;
    if (w < 5.0f) {
        w -= 2.5f;
        polynomial = 2.81022636e-8f;
        polynomial = 3.43273939e-7f + polynomial * w;
        polynomial = -3.5233877e-6f + polynomial * w;
        polynomial = -4.39150654e-6f + polynomial * w;
        polynomial = 2.18580870e-4f + polynomial * w;
        polynomial = -1.25372503e-3f + polynomial * w;
        polynomial = -4.17768164e-3f + polynomial * w;
        polynomial = 2.46640727e-1f + polynomial * w;
        polynomial = 1.50140941f + polynomial * w;
    } else {
        w = sqrt(w) - 3.0f;
        polynomial = -2.00214257e-4f;
        polynomial = 1.00950558e-4f + polynomial * w;
        polynomial = 1.34934322e-3f + polynomial * w;
        polynomial = -3.67342844e-3f + polynomial * w;
        polynomial = 5.73950773e-3f + polynomial * w;
        polynomial = -7.62246130e-3f + polynomial * w;
        polynomial = 9.43887047e-3f + polynomial * w;
        polynomial = 1.00167406f + polynomial * w;
        polynomial = 2.83297682f + polynomial * w;
    }
    return polynomial * x;
}

vec2 sampleBeckmannVisibleSlope(const float cosThetaI, const vec2 unitSample) {
    const vec2 sampleValue = clamp(unitSample, vec2(1e-6f), vec2(1.0f - 1e-6f));
    if (cosThetaI > 0.9999f) {
        const float radius = sqrt(-log(1.0f - sampleValue.x));
        const float phi = 2.0f * PI * sampleValue.y;
        return radius * vec2(cos(phi), sin(phi));
    }

    const float sinThetaI = sqrt(max(0.0f, 1.0f - cosThetaI * cosThetaI));
    const float tanThetaI = sinThetaI / cosThetaI;
    const float cotThetaI = 1.0f / tanThetaI;
    float lower = -1.0f;
    float upper = beckmannErf(cotThetaI);

    const float thetaI = acos(clamp(cosThetaI, 0.0f, 1.0f));
    const float fit = 1.0f + thetaI * (-0.876f + thetaI * (0.4265f - 0.0594f * thetaI));
    float value = upper - (1.0f + upper) * pow(1.0f - sampleValue.x, fit);
    const float invSqrtPi = 0.5641895835477563f;
    const float normalization =
        1.0f / (1.0f + upper + invSqrtPi * tanThetaI * exp(-cotThetaI * cotThetaI));

    for (int iteration = 0; iteration < 9; ++iteration) {
        if (!(value >= lower && value <= upper)) {
            value = 0.5f * (lower + upper);
        }
        const float inverseErf = beckmannInverseErf(value);
        const float cdf = normalization *
                (1.0f + value + invSqrtPi * tanThetaI * exp(-inverseErf * inverseErf)) -
            sampleValue.x;
        if (abs(cdf) < 1e-5f) {
            break;
        }

        if (cdf > 0.0f) {
            upper = value;
        } else {
            lower = value;
        }
        const float derivative = normalization * (1.0f - inverseErf * tanThetaI);
        value = abs(derivative) > 1e-6f ? value - cdf / derivative : 0.5f * (lower + upper);
    }

    return vec2(beckmannInverseErf(value), beckmannInverseErf(2.0f * sampleValue.y - 1.0f));
}

vec3 sampleBeckmannVisibleNormal(const vec2 unitSample, const vec3 wi, const float alpha) {
    const vec3 upperWi = wi.z < 0.0f ? -wi : wi;
    const vec3 stretchedWi = normalize(vec3(alpha * upperWi.xy, upperWi.z));
    vec2 slope = sampleBeckmannVisibleSlope(stretchedWi.z, unitSample);

    const float xyLength = length(stretchedWi.xy);
    if (xyLength > 0.0f) {
        const vec2 direction = stretchedWi.xy / xyLength;
        slope = vec2(
            direction.x * slope.x - direction.y * slope.y,
            direction.y * slope.x + direction.x * slope.y);
    }
    slope *= alpha;
    return normalize(vec3(-slope, 1.0f));
}

float beckmannDistribution(vec3 normal, float alpha) {
    if (normal.z <= 0.0f) {
        return 0.0f;
    }

    const float cosThetaSquared = normal.z * normal.z;
    const float tanThetaSquared = max(0.0f, 1.0f - cosThetaSquared) / cosThetaSquared;
    const float alphaSquared = alpha * alpha;
    return exp(-tanThetaSquared / alphaSquared) /
        (PI * alphaSquared * cosThetaSquared * cosThetaSquared);
}

float beckmannSmithG1(vec3 v, vec3 microfacetNormal, float alpha) {
    if (dot(v, microfacetNormal) * v.z <= 0.0f) {
        return 0.0f;
    }

    const float absTanTheta = abs(microfacetTanTheta(v));
    if (absTanTheta == 0.0f) {
        return 1.0f;
    }

    const float a = 1.0f / (alpha * absTanTheta);
    if (a >= 1.6f) {
        return 1.0f;
    }
    const float aSquared = a * a;
    return (3.535f * a + 2.181f * aSquared) /
        (1.0f + 2.276f * a + 2.577f * aSquared);
}

float beckmannGeometry(vec3 wi, vec3 wo, vec3 microfacetNormal, float alpha) {
    return beckmannSmithG1(wi, microfacetNormal, alpha) * beckmannSmithG1(wo, microfacetNormal, alpha);
}

float computeBeckmannNormalPdf(vec3 microfacetNormal, float alpha) {
    return beckmannDistribution(microfacetNormal, alpha) * abs(microfacetNormal.z);
}

float computeBeckmannVisibleNormalPdf(const vec3 wi, const vec3 microfacetNormal, const float alpha) {
    if (wi.z == 0.0f || microfacetNormal.z <= 0.0f) {
        return 0.0f;
    }
    return beckmannDistribution(microfacetNormal, alpha) * beckmannSmithG1(wi, microfacetNormal, alpha) *
        abs(dot(wi, microfacetNormal)) / abs(wi.z);
}

#endif // CRISP_MICROFACET_BECKMANN_GLSL
