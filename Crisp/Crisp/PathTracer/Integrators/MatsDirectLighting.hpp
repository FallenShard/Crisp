#pragma once

#include "Integrator.hpp"

namespace crisp {
struct Intersection;

class MatsDirectLightingIntegrator : public Integrator {
public:
    MatsDirectLightingIntegrator(const VariantMap& params = VariantMap());
    virtual ~MatsDirectLightingIntegrator();

    virtual void preprocess(pt::Scene* scene) override;
    virtual Spectrum Li(
        const pt::Scene* scene,
        Sampler& sampler,
        Ray3& ray,
        IlluminationFlags flags = Illumination::Full) const override;
};
} // namespace crisp