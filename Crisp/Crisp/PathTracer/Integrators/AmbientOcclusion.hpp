#pragma once

#include <Crisp/PathTracer/Integrators/Integrator.hpp>

namespace crisp {
class AmbientOcclusionIntegrator : public Integrator {
public:
    AmbientOcclusionIntegrator(const VariantMap& params = VariantMap());
    virtual ~AmbientOcclusionIntegrator();

    virtual void preprocess(pt::Scene* scene) override;
    virtual Spectrum Li(
        const pt::Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags flags = Illumination::Full) const override;

private:
    float m_rayLength;
};
} // namespace crisp