#pragma once

#include <Crisp/PathTracer/BSDFs/BSDF.hpp>
#include <Crisp/PathTracer/Optics/Fresnel.hpp>


namespace crisp {
class SmoothConductorBSDF : public BSDF {
public:
    SmoothConductorBSDF(const VariantMap& params = VariantMap());
    ~SmoothConductorBSDF();

    virtual Spectrum eval(const BSDF::Sample& bsdfSample) const override;
    virtual Spectrum sample(BSDF::Sample& bsdfSample, Sampler& sampler) const override;
    virtual float pdf(const BSDF::Sample& bsdfSample) const override;

private:
    ComplexIOR m_IOR;
};
} // namespace crisp