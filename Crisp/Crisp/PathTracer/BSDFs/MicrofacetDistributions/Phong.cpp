#include "Phong.hpp"

#include <Crisp/Math/CoordinateFrame.hpp>
#include <Crisp/Math/Operations.hpp>

namespace crisp {
PhongDistribution::PhongDistribution(const VariantMap& params) {
    m_alpha = params.get<float>("alpha", 0.1f);

    m_alpha = clamp(m_alpha, 0.0f, 1.0f);
}

PhongDistribution::~PhongDistribution() {}

glm::vec3 PhongDistribution::sampleNormal(const glm::vec2& sample) const {
    float cosTheta = std::sqrtf(std::powf(sample.y, 1.0f / (m_alpha + 2.0f)));
    float theta = acosf(cosTheta);
    float phi = 2.0f * PI<> * sample.x;
    float sinTheta = sinf(theta);

    return glm::vec3(sinTheta * cosf(phi), sinTheta * sinf(phi), cosTheta);
}

float PhongDistribution::pdf(const glm::vec3& m) const {
    return D(m) * std::abs(CoordinateFrame::cosTheta(m));
}

float PhongDistribution::D(const glm::vec3& m) const {
    float cosTheta = CoordinateFrame::cosTheta(m);
    if (cosTheta < 0.0f) {
        return 0.0f;
    }

    return std::pow(cosTheta, m_alpha) * (m_alpha + 2) * InvTwoPI<>;
}

float PhongDistribution::G(const glm::vec3& wi, const glm::vec3& wo, const glm::vec3& m) const {
    return smithBeckmannG1(wi, m) * smithBeckmannG1(wo, m);
}

float PhongDistribution::smithBeckmannG1(const glm::vec3& v, const glm::vec3& m) const {
    // Back-surface check
    if (glm::dot(v, m) * CoordinateFrame::cosTheta(v) <= 0) {
        return 0.0f;
    }

    float tanTheta = std::abs(CoordinateFrame::tanTheta(v));
    if (tanTheta == 0.0f) {
        return 1.0f;
    }

    float a = sqrt(0.5f * m_alpha + 1) / tanTheta;
    if (a >= 1.6f) {
        return 1.0f;
    }

    float a2 = a * a;

    return (3.535f * a + 2.181f * a2) / (1.0f + 2.276f * a + 2.577f * a2);
}
} // namespace crisp