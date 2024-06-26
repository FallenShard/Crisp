#pragma once

#include <Crisp/PathTracer/BSDFs/MicrofacetDistributions/MicrofacetDistribution.hpp>

namespace crisp {
class PhongDistribution : public MicrofacetDistribution {
public:
    PhongDistribution(const VariantMap& params);
    ~PhongDistribution();

    virtual glm::vec3 sampleNormal(const glm::vec2& sample) const override;
    virtual float pdf(const glm::vec3& m) const override;

    virtual float D(const glm::vec3& m) const override;
    virtual float G(const glm::vec3& wi, const glm::vec3& wo, const glm::vec3& m) const override;

private:
    float smithBeckmannG1(const glm::vec3& v, const glm::vec3& m) const;
};
} // namespace crisp