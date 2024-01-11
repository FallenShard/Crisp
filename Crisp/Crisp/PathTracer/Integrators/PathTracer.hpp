#pragma once

#include <Crisp/PathTracer/Integrators/Integrator.hpp>

namespace crisp {
class PathTracerIntegrator : public Integrator {
public:
    PathTracerIntegrator(const VariantMap& attributes);
    virtual ~PathTracerIntegrator();

    virtual void preprocess(pt::Scene* scene) override;
    virtual Spectrum Li(
        const pt::Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags flags = Illumination::Full) const override;
};
} // namespace crisp