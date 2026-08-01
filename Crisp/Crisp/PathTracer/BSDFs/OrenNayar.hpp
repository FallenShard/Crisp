#pragma once

#include <Crisp/PathTracer/BSDFs/BSDF.hpp>

namespace crisp {
class OrenNayarBSDF : public BSDF {
public:
    explicit OrenNayarBSDF(const VariantMap& params);

    void setTexture(std::shared_ptr<Texture<Spectrum>> texture) override;
    Spectrum eval(const BSDF::Sample& bsdfSample) const override;
    Spectrum sample(BSDF::Sample& bsdfSample, Sampler& sampler) const override;
    float pdf(const BSDF::Sample& bsdfSample) const override;

private:
    Spectrum evaluateReflectance(const glm::vec2& uv) const;
    float evaluateRoughnessFactor(const glm::vec3& wi, const glm::vec3& wo) const;

    Spectrum m_reflectance;
    std::shared_ptr<Texture<Spectrum>> m_reflectanceTexture;
    float m_a;
    float m_b;
};
} // namespace crisp
