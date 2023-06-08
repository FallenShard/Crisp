#pragma once

#include "Integrator.hpp"

namespace crisp
{
struct Intersection;

class VolumePathTracerIntegrator : public Integrator
{
public:
    VolumePathTracerIntegrator(const VariantMap& attributes);
    virtual ~VolumePathTracerIntegrator();

    virtual void preprocess(pt::Scene* scene) override;
    virtual Spectrum Li(
        const pt::Scene* scene,
        Sampler& sampler,
        Ray3& ray,
        IlluminationFlags flags = Illumination::Full) const override;

private:
    unsigned int m_rrDepth;
    unsigned int m_maxDepth;
};
} // namespace crisp