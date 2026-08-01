#include <Crisp/PathTracer/BSDFs/OrenNayar.hpp>

#include <algorithm>

#include <Crisp/Math/CoordinateFrame.hpp>
#include <Crisp/Math/Warp.hpp>

namespace crisp {
OrenNayarBSDF::OrenNayarBSDF(const VariantMap& params)
    : BSDF(Lobe::Diffuse)
    , m_reflectance(params.get<Spectrum>("reflectance", Spectrum(1.0f))) {
    const float roughnessDegrees = std::clamp(params.get<float>("roughnessDegrees", 0.0f), 0.0f, 90.0f);
    const float roughness = glm::radians(roughnessDegrees);
    const float roughnessSquared = roughness * roughness;
    m_a = 1.0f - roughnessSquared / (2.0f * (roughnessSquared + 0.33f));
    m_b = 0.45f * roughnessSquared / (roughnessSquared + 0.09f);
}

void OrenNayarBSDF::setTexture(std::shared_ptr<Texture<Spectrum>> texture) {
    m_reflectanceTexture = std::move(texture);
}

Spectrum OrenNayarBSDF::evaluateReflectance(const glm::vec2& uv) const {
    return m_reflectanceTexture ? m_reflectanceTexture->eval(uv) : m_reflectance;
}

float OrenNayarBSDF::evaluateRoughnessFactor(const glm::vec3& wi, const glm::vec3& wo) const {
    const float cosThetaI = CoordinateFrame::cosTheta(wi);
    const float cosThetaO = CoordinateFrame::cosTheta(wo);
    const float sinThetaI = CoordinateFrame::sinTheta(wi);
    const float sinThetaO = CoordinateFrame::sinTheta(wo);

    float maxCosPhiDifference = 0.0f;
    if (sinThetaI > 0.0f && sinThetaO > 0.0f) {
        maxCosPhiDifference = std::clamp((wi.x * wo.x + wi.y * wo.y) / (sinThetaI * sinThetaO), 0.0f, 1.0f);
    }

    float sinAlpha;
    float tanBeta;
    if (cosThetaI > cosThetaO) {
        sinAlpha = sinThetaO;
        tanBeta = sinThetaI / cosThetaI;
    } else {
        sinAlpha = sinThetaI;
        tanBeta = sinThetaO / cosThetaO;
    }

    return m_a + m_b * maxCosPhiDifference * sinAlpha * tanBeta;
}

Spectrum OrenNayarBSDF::eval(const BSDF::Sample& bsdfSample) const {
    const float cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
    const float cosThetaO = CoordinateFrame::cosTheta(bsdfSample.wo);
    if (bsdfSample.measure != Measure::SolidAngle || cosThetaI <= 0.0f || cosThetaO <= 0.0f) {
        return 0.0f;
    }

    return evaluateReflectance(bsdfSample.uv) * InvPI<> * evaluateRoughnessFactor(bsdfSample.wi, bsdfSample.wo) *
           cosThetaO;
}

Spectrum OrenNayarBSDF::sample(BSDF::Sample& bsdfSample, Sampler& sampler) const {
    bsdfSample.wo = warp::squareToCosineHemisphere(sampler.next2D());
    bsdfSample.pdf = warp::squareToCosineHemispherePdf(bsdfSample.wo);
    bsdfSample.measure = Measure::SolidAngle;
    bsdfSample.sampledLobe = Lobe::Diffuse;
    bsdfSample.eta = 1.0f;

    if (CoordinateFrame::cosTheta(bsdfSample.wi) <= 0.0f || bsdfSample.pdf <= 0.0f) {
        return 0.0f;
    }

    return evaluateReflectance(bsdfSample.uv) * evaluateRoughnessFactor(bsdfSample.wi, bsdfSample.wo);
}

float OrenNayarBSDF::pdf(const BSDF::Sample& bsdfSample) const {
    const float cosThetaO = CoordinateFrame::cosTheta(bsdfSample.wo);
    const float cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
    if (bsdfSample.measure != Measure::SolidAngle || cosThetaO <= 0.0f || cosThetaI <= 0.0f) {
        return 0.0f;
    }

    return InvPI<> * cosThetaO;
}
} // namespace crisp
