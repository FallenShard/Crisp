#pragma once

#include <Crisp/PathTracer/BSDFs/BSDF.hpp>

namespace crisp {
class LambertianBSDF : public BSDF {
public:
    LambertianBSDF(const VariantMap& params);

    virtual void setTexture(std::shared_ptr<Texture<Spectrum>> texture) override;
    virtual Spectrum eval(const BSDF::Sample& bsdfSample) const override;
    virtual Spectrum sample(BSDF::Sample& bsdfSample, Sampler& sampler) const override;
    virtual float pdf(const BSDF::Sample& bsdfSample) const override;

private:
    std::shared_ptr<Texture<Spectrum>> m_albedo;
};
} // namespace crisp
