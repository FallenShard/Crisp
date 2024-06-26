#pragma once

#include <Crisp/PathTracer/Integrators/Integrator.hpp>

namespace crisp {
struct Intersection;

class EmsDirectLightingIntegrator : public Integrator {
public:
    EmsDirectLightingIntegrator(const VariantMap& params = VariantMap());
    virtual ~EmsDirectLightingIntegrator() = default;

    virtual void preprocess(pt::Scene* scene) override;
    virtual Spectrum Li(
        const pt::Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags flags = Illumination::Full) const override;
};
} // namespace crisp