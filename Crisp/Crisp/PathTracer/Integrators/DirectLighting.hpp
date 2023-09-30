#pragma once

#include "Integrator.hpp"

namespace crisp {
struct Intersection;

class DirectLightingIntegrator : public Integrator {
public:
    DirectLightingIntegrator(const VariantMap& params = VariantMap());
    virtual ~DirectLightingIntegrator();

    virtual void preprocess(pt::Scene* scene) override;
    virtual Spectrum Li(
        const pt::Scene* scene,
        Sampler& sampler,
        Ray3& ray,
        IlluminationFlags flags = Illumination::Full) const override;
};
} // namespace crisp