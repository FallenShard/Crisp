#version 460 core
#extension GL_GOOGLE_include_directive : require

#include "Common/math-constants.part.glsl"
#include "Common/warp.part.glsl"
#include "BSDFs/lambertian.part.glsl"
#include "BSDFs/oren-nayar.part.glsl"
#include "BSDFs/microfacet.part.glsl"
#include "BSDFs/fresnel.part.glsl"
#include "BSDFs/smooth-conductor.part.glsl"
#include "BSDFs/rough-conductor.part.glsl"
#include "BSDFs/rough-dielectric.part.glsl"

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

const uint kModelLambertian = 0u;
const uint kModelOrenNayar = 1u;
const uint kModelMicrofacet = 2u;
const uint kModelDielectricFresnel = 3u;
const uint kModelConductorFresnel = 4u;
const uint kModelSmoothConductor = 5u;
const uint kModelMicrofacetNormal = 6u;
const uint kModelRoughConductor = 7u;
const uint kModelRoughDielectric = 8u;

const uint kOperationEvaluate = 0u;
const uint kOperationSample = 1u;
const uint kOperationLimit = 2u;
const uint kOperationCriticalAngle = 3u;

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
    const float lobeSample = unitFloat(index + 0xd1b54a35u);
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
        return;
    }

    if (model == kModelRoughDielectric) {
        const float extIor = 1.0f;
        const float intIor = 1.5046f;
        vec3 dielectricWi = wi;

        if (operation == kOperationEvaluate) {
            const vec2 sphereSample = vec2(
                unitFloat(index + 0x243f6a88u),
                unitFloat(index + 0x85a308d3u));
            vec3 wo = squareToUniformHemisphere(sphereSample);
            if ((hashUint(index + 0x13198a2eu) & 1u) != 0u) {
                wo.z = -wo.z;
            }
            results[index].woAndPdf = vec4(
                wo,
                roughDielectricPdf(extIor, intIor, microfacetType, 0.3f, dielectricWi, wo));
            results[index].value = vec4(
                evaluateRoughDielectric(extIor, intIor, microfacetType, 0.3f, dielectricWi, wo),
                0.0f);
            results[index].reverseValue = vec4(
                evaluateRoughDielectric(extIor, intIor, microfacetType, 0.3f, wo, dielectricWi),
                0.0f);
            return;
        }

        vec2 dielectricNormalSample = bsdfSample;
        float dielectricLobeSample = lobeSample;
        if (operation == kOperationCriticalAngle) {
            const float criticalSine = extIor / intIor;
            const bool aboveCriticalAngle = (index & 1u) != 0u;
            const float sinThetaI = criticalSine + (aboveCriticalAngle ? 1e-3f : -1e-3f);
            dielectricWi = vec3(sinThetaI, 0.0f, -sqrt(1.0f - sinThetaI * sinThetaI));
            dielectricNormalSample = vec2(0.0f);
            dielectricLobeSample = 0.999999f;
            results[index].wiAndAux.xyz = dielectricWi;
        }

        const vec3 sampledNormal = sampleMicrofacetNormal(dielectricNormalSample, microfacetType, 0.3f);
        const float cosThetaIm = dot(dielectricWi, sampledNormal);
        float cosThetaTm;
        const float fresnel = fresnelDielectric(cosThetaIm, extIor, intIor, cosThetaTm);
        results[index].wiAndAux.w = cosThetaIm * dielectricWi.z > 0.0f ? fresnel : -1.0f;
        const vec3 reflectionWo = 2.0f * cosThetaIm * sampledNormal - dielectricWi;
        const bool validReflection = cosThetaIm * dielectricWi.z > 0.0f && reflectionWo.z * dielectricWi.z > 0.0f;
        const float etaIt = dielectricWi.z > 0.0f ? extIor / intIor : intIor / extIor;
        const vec3 transmissionWo =
            sampledNormal * (etaIt * cosThetaIm - sign(cosThetaIm) * cosThetaTm) - etaIt * dielectricWi;
        const bool validTransmission = cosThetaIm * dielectricWi.z > 0.0f && fresnel < 1.0f &&
            transmissionWo.z * dielectricWi.z < 0.0f;
        const float directionalMass =
            fresnel * float(validReflection) + (1.0f - fresnel) * float(validTransmission);
        const bool selectedReflection = dielectricLobeSample <= fresnel;
        results[index].reverseValue.w = selectedReflection ? directionalMass : -directionalMass;

        vec3 wo;
        vec3 sampledF;
        float pdf;
        sampleRoughDielectric(
            dielectricNormalSample,
            dielectricLobeSample,
            extIor,
            intIor,
            microfacetType,
            0.3f,
            dielectricWi,
            wo,
            sampledF,
            pdf);
        results[index].woAndPdf = vec4(wo, pdf);
        results[index].value = vec4(
            evaluateRoughDielectric(extIor, intIor, microfacetType, 0.3f, dielectricWi, wo),
            roughDielectricPdf(extIor, intIor, microfacetType, 0.3f, dielectricWi, wo));
        results[index].reverseValue.xyz = sampledF;
    }
}
