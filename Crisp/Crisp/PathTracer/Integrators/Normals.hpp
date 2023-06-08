#pragma once

#include "Integrator.hpp"

namespace crisp
{
class NormalsIntegrator : public Integrator
{
public:
    NormalsIntegrator(const VariantMap& params = VariantMap());
    virtual ~NormalsIntegrator();

    virtual void preprocess(pt::Scene* scene) override;
    virtual Spectrum Li(
        const pt::Scene* scene,
        Sampler& sampler,
        Ray3& ray,
        IlluminationFlags flags = Illumination::Full) const override;
};
} // namespace crisp