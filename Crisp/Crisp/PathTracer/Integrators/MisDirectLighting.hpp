#pragma once

#include <Crisp/PathTracer/Integrators/Integrator.hpp>

namespace crisp {
struct Intersection;

class MisDirectLightingIntegrator : public Integrator {
public:
    MisDirectLightingIntegrator(const VariantMap& params = VariantMap());
    virtual ~MisDirectLightingIntegrator() = default;

    virtual void preprocess(pt::Scene* scene) override;
    virtual Spectrum Li(
        const pt::Scene* scene,
        Sampler& sampler,
        Ray3& ray,
        IlluminationFlags flags = Illumination::Full) const override;

private:
    static Spectrum lightImportanceSample(
        const pt::Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its);
    static Spectrum bsdfImportanceSample(
        const pt::Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its);
};
} // namespace crisp