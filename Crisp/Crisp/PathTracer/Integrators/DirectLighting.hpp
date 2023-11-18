#pragma once

#include "Integrator.hpp"

namespace crisp {
struct Intersection;

class DirectLightingIntegrator final : public Integrator {
public:
    DirectLightingIntegrator(const VariantMap& params = VariantMap());
    ~DirectLightingIntegrator() override = default;

    virtual void preprocess(pt::Scene* scene) override;
    virtual Spectrum Li(
        const pt::Scene* scene,
        Sampler& sampler,
        Ray3& ray,
        IlluminationFlags flags = Illumination::Full) const override;
};
} // namespace crisp