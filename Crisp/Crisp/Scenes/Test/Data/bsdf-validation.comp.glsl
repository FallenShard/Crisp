#version 460 core
#extension GL_GOOGLE_include_directive : require

#include "Common/math-constants.part.glsl"
#include "Common/warp.part.glsl"
#include "Brdf/lambertian.part.glsl"
#include "Brdf/oren-nayar.part.glsl"
#include "Brdf/microfacet.part.glsl"
#include "Brdf/fresnel.part.glsl"
#include "Brdf/smooth-conductor.part.glsl"
#include "Brdf/rough-conductor.part.glsl"

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

const uint kModelLambertian = 0u;
const uint kModelOrenNayar = 1u;
const uint kModelMicrofacet = 2u;
const uint kModelDielectricFresnel = 3u;
const uint kModelConductorFresnel = 4u;
const uint kModelSmoothConductor = 5u;
const uint kModelMicrofacetNormal = 6u;
const uint kModelRoughConductor = 7u;

const uint kOperationEvaluate = 0u;
const uint kOperationSample = 1u;
const uint kOperationLimit = 2u;

struct ValidationResult {
    vec4 wiAndAux;
    vec4 woAndPdf;
    vec4 value;
    vec4 reverseValue;
};

layout(set = 0, binding = 0, std430) writeonly buffer Results {
    ValidationResult results[];
};

layout(push_constant) uniform PushConstants {
    uint sampleCount;
    uint model;
    uint operation;
    int microfacetType;
};

uint hashUint(uint value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float unitFloat(uint value) {
    return (float(hashUint(value)) + 0.5f) * (1.0f / 4294967296.0f);
}

void main() {
    const uint index = gl_GlobalInvocationID.x;
    if (index >= sampleCount) {
        return;
    }

    const vec2 integrationSample =
        vec2(unitFloat(2u * index), (float(index) + 0.5f) / float(sampleCount));
    const vec2 bsdfSample = vec2(unitFloat(2u * index + 0x68bc21ebu), unitFloat(2u * index + 0x967a889bu));
    const float incidentCosines[4] = float[](1.0f, 0.8f, 0.4f, 0.1f);
    const float cosThetaI = incidentCosines[index & 3u];
    const vec3 wi = vec3(sqrt(1.0f - cosThetaI * cosThetaI), 0.0f, cosThetaI);
    const vec3 uniformWo = squareToUniformHemisphere(integrationSample);

    results[index].wiAndAux = vec4(wi, 0.0f);
    results[index].woAndPdf = vec4(0.0f);
    results[index].value = vec4(0.0f);
    results[index].reverseValue = vec4(0.0f);

    if (model == kModelLambertian || model == kModelOrenNayar || model == kModelMicrofacet ||
        model == kModelRoughConductor) {
        vec3 wo = uniformWo;
        if (operation == kOperationSample) {
            if (model == kModelRoughConductor) {
                wo = sampleRoughConductor(bsdfSample, wi, microfacetType, 0.3f);
            } else if (model == kModelMicrofacet) {
                bool sampledSpecular;
                wo = sampleMicrofacet(bsdfSample, wi, 0.6f, microfacetType, 0.3f, sampledSpecular);
            } else {
                wo = sampleLambertian(bsdfSample);
            }
        }

        if (model == kModelLambertian) {
            results[index].value = vec4(evaluateLambertian(vec3(0.8f, 0.6f, 0.4f), wi, wo), 0.0f);
            results[index].reverseValue = vec4(evaluateLambertian(vec3(0.8f, 0.6f, 0.4f), wo, wi), 0.0f);
            results[index].woAndPdf = vec4(wo, lambertianPdf(wi, wo));
            return;
        }

        if (model == kModelOrenNayar) {
            const float roughness = operation == kOperationLimit ? 0.0f : radians(45.0f);
            results[index].value = vec4(evaluateOrenNayar(vec3(0.8f, 0.6f, 0.4f), roughness, wi, wo), 0.0f);
            results[index].reverseValue =
                vec4(evaluateOrenNayar(vec3(0.8f, 0.6f, 0.4f), roughness, wo, wi), 0.0f);
            results[index].woAndPdf = vec4(wo, lambertianPdf(wi, wo));
            return;
        }

        if (model == kModelRoughConductor) {
            const vec3 goldEta = vec3(0.1431189557f, 0.3749570432f, 1.4424785571f);
            const vec3 goldK = vec3(3.9831604247f, 2.3857207478f, 1.6032152899f);
            results[index].value =
                vec4(evaluateRoughConductor(goldEta, goldK, microfacetType, 0.3f, wi, wo), 0.0f);
            results[index].reverseValue =
                vec4(evaluateRoughConductor(goldEta, goldK, microfacetType, 0.3f, wo, wi), 0.0f);
            results[index].woAndPdf = vec4(wo, roughConductorPdf(wi, wo, microfacetType, 0.3f));
            return;
        }

        results[index].value = vec4(
            evaluateMicrofacet(vec3(0.4f, 0.3f, 0.2f), 0.6f, 1.0f, 1.5046f, microfacetType, 0.3f, wi, wo),
            0.0f);
        results[index].reverseValue = vec4(
            evaluateMicrofacet(vec3(0.4f, 0.3f, 0.2f), 0.6f, 1.0f, 1.5046f, microfacetType, 0.3f, wo, wi),
            0.0f);
        results[index].woAndPdf = vec4(wo, microfacetPdf(wi, wo, 0.6f, microfacetType, 0.3f));
        return;
    }

    const float cosTheta = (float(index) + 0.5f) / float(sampleCount);
    if (model == kModelDielectricFresnel) {
        float cosThetaExternal;
        float cosThetaInternal;
        results[index].wiAndAux.w = cosTheta;
        results[index].value = vec4(
            fresnelDielectric(cosTheta, 1.0f, 1.5046f, cosThetaExternal),
            fresnelDielectric(-cosTheta, 1.0f, 1.5046f, cosThetaInternal),
            cosThetaExternal,
            cosThetaInternal);
        return;
    }

    const vec3 goldEta = vec3(0.1431189557f, 0.3749570432f, 1.4424785571f);
    const vec3 goldK = vec3(3.9831604247f, 2.3857207478f, 1.6032152899f);
    if (model == kModelConductorFresnel) {
        results[index].wiAndAux.w = cosTheta;
        results[index].value = vec4(fresnelConductor(cosTheta, goldEta, goldK), 0.0f);
        return;
    }

    if (model == kModelSmoothConductor) {
        vec3 wo;
        vec3 weight;
        float pdf;
        sampleSmoothConductor(vec3(0.0f, 0.0f, 1.0f), wi, goldEta, goldK, wo, weight, pdf);
        results[index].woAndPdf = vec4(wo, pdf);
        results[index].value = vec4(weight, 0.0f);
        return;
    }

    if (model == kModelMicrofacetNormal) {
        const vec3 microfacetNormal = operation == kOperationSample
            ? sampleMicrofacetNormal(bsdfSample, microfacetType, 0.3f)
            : uniformWo;
        results[index].woAndPdf =
            vec4(microfacetNormal, microfacetNormalPdf(microfacetNormal, microfacetType, 0.3f));
        results[index].value.x = microfacetDistribution(microfacetNormal, microfacetType, 0.3f);
    }
}
