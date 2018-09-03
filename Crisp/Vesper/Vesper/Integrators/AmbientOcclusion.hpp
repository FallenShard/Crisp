#pragma once

#include "Integrator.hpp"

namespace crisp
{
    class AmbientOcclusionIntegrator : public Integrator
    {
    public:
        AmbientOcclusionIntegrator(const VariantMap& params = VariantMap());
        virtual ~AmbientOcclusionIntegrator();

        virtual void preprocess(Scene* scene) override;
        virtual Spectrum Li(const Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags flags = Illumination::Full) const override;

    private:
        float m_rayLength;
    };
}