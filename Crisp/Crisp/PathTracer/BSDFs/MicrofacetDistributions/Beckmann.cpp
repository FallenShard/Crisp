#include <Crisp/PathTracer/BSDFs/MicrofacetDistributions/Beckmann.hpp>

#include <Crisp/Math/CoordinateFrame.hpp>
#include <Crisp/Math/Operations.hpp>

namespace crisp {
BeckmannDistribution::BeckmannDistribution(const VariantMap& params) {
    m_alpha = params.get<float>("alpha", 0.1f);

    m_alpha = clamp(m_alpha, 0.0f, 1.0f);
}

BeckmannDistribution::~BeckmannDistribution() {}

glm::vec3 BeckmannDistribution::sampleNormal(const glm::vec2& sample) const {
    float denominator = 1.0f - m_alpha * m_alpha * log(1.0f - sample.y);
    float cosTheta = sqrtf(fabs(1.0f / denominator));
    float theta = acosf(cosTheta);
    float phi = 2.0f * PI<> * sample.x;
    float sinTheta = sinf(theta);

    return glm::vec3(sinTheta * cosf(phi), sinTheta * sinf(phi), cosTheta);
}

float BeckmannDistribution::pdf(const glm::vec3& m) const {
    return D(m) * std::abs(CoordinateFrame::cosTheta(m));
}

float BeckmannDistribution::D(const glm::vec3& m) const {
    float cosTheta = CoordinateFrame::cosTheta(m);
    if (cosTheta < 0.0f) {
        return 0.0f;
    }

    float temp = CoordinateFrame::tanTheta(m) / m_alpha;
    float alphaCosTheta2 = m_alpha * cosTheta * cosTheta;

    return std::exp(-temp * temp) * InvPI<> / (alphaCosTheta2 * alphaCosTheta2);
}

float BeckmannDistribution::G(const glm::vec3& wi, const glm::vec3& wo, const glm::vec3& m) const {
    return smithBeckmannG1(wi, m) * smithBeckmannG1(wo, m);
}

float BeckmannDistribution::smithBeckmannG1(const glm::vec3& v, const glm::vec3& m) const {
    // Back-surface check
    if (glm::dot(v, m) * CoordinateFrame::cosTheta(v) <= 0) {
        return 0.0f;
    }

    float tanTheta = std::abs(CoordinateFrame::tanTheta(v));
    if (tanTheta == 0.0f) {
        return 1.0f;
    }

    float a = 1.0f / (m_alpha * tanTheta);
    if (a >= 1.6f) {
        return 1.0f;
    }

    float a2 = a * a;

    return (3.535f * a + 2.181f * a2) / (1.0f + 2.276f * a + 2.577f * a2);
}
} // namespace crisp