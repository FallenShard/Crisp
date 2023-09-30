#include "GGX.hpp"

#include <Crisp/Math/CoordinateFrame.hpp>
#include <Crisp/Math/Operations.hpp>

namespace crisp {
GGXDistribution::GGXDistribution(const VariantMap& params) {
    m_alpha = params.get<float>("alpha", 0.1f);

    m_alpha = clamp(m_alpha, 0.0f, 1.0f);
}

GGXDistribution::~GGXDistribution() {}

glm::vec3 GGXDistribution::sampleNormal(const glm::vec2& sample) const {
    // float denominator = m_alpha * m_alpha * sample.y + 1.0f - sample.y;
    // float cosTheta = sqrtf((1.0f - sample.y) / denominator);
    float tanTheta2 = m_alpha * m_alpha * sample.y / (1.0f - sample.y);
    float cosTheta = 1.0f / std::sqrt(1.0f + tanTheta2);
    float phi = 2.0f * PI<> * sample.x;
    float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);

    return glm::vec3(sinTheta * cosf(phi), sinTheta * sinf(phi), cosTheta);
}

float GGXDistribution::pdf(const glm::vec3& m) const {
    return D(m) * std::abs(CoordinateFrame::cosTheta(m));
}

float GGXDistribution::D(const glm::vec3& m) const {
    float cosTheta = CoordinateFrame::cosTheta(m);
    if (cosTheta <= 0.0f) {
        return 0.0f;
    }

    float cosTheta2 = cosTheta * cosTheta;
    float tanTheta2 = (1.0f - cosTheta2) / cosTheta2;
    float alpha2 = m_alpha * m_alpha;
    float sqTerm = alpha2 + tanTheta2;

    return alpha2 * InvPI<> / (cosTheta2 * cosTheta2 * sqTerm * sqTerm);
}

float GGXDistribution::G(const glm::vec3& wi, const glm::vec3& wo, const glm::vec3& m) const {
    return smithBeckmannG1(wi, m) * smithBeckmannG1(wo, m);
}

float GGXDistribution::smithBeckmannG1(const glm::vec3& v, const glm::vec3& m) const {
    // Back-surface check
    if (glm::dot(v, m) * CoordinateFrame::cosTheta(v) <= 0.0f) {
        return 0.0f;
    }

    float tanTheta = std::abs(CoordinateFrame::tanTheta(v));
    if (tanTheta == 0.0f) {
        return 1.0f;
    }

    float a = m_alpha * tanTheta;
    return 2.0f / (1.0f + sqrtf(1.0f + a * a));
}
} // namespace crisp