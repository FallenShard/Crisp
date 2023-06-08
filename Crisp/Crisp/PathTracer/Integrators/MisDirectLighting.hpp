#pragma once

#include "Integrator.hpp"

namespace crisp
{
struct Intersection;

class MisDirectLightingIntegrator : public Integrator
{
public:
    MisDirectLightingIntegrator(const VariantMap& params = VariantMap());
    virtual ~MisDirectLightingIntegrator();

    virtual void preprocess(pt::Scene* scene) override;
    virtual Spectrum Li(
        const pt::Scene* scene,
        Sampler& sampler,
        Ray3& ray,
        IlluminationFlags flags = Illumination::Full) const override;

private:
    Spectrum lightImportanceSample(
        const pt::Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const;
    Spectrum bsdfImportanceSample(
        const pt::Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const;
};
} // namespace crisp