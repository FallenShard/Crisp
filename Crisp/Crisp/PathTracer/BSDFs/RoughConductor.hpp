#pragma once

#include <Crisp/Optics/Fresnel.hpp>
#include <Crisp/PathTracer/BSDFs/BSDF.hpp>

#include <Crisp/PathTracer/BSDFs/MicrofacetDistributions/MicrofacetDistribution.hpp>

namespace crisp {
class RoughConductorBSDF : public BSDF {
public:
    RoughConductorBSDF(const VariantMap& params = VariantMap());
    ~RoughConductorBSDF();

    virtual Spectrum eval(const BSDF::Sample& bsdfSample) const override;
    virtual Spectrum sample(BSDF::Sample& bsdfSample, Sampler& sampler) const override;
    virtual float pdf(const BSDF::Sample& bsdfSample) const override;

private:
    ComplexIOR m_IOR;
    std::unique_ptr<MicrofacetDistribution> m_distrib;
};
} // namespace crisp