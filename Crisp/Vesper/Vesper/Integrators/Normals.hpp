#pragma once

#include "Integrator.hpp"

namespace vesper
{
    class NormalsIntegrator : public Integrator
    {
    public:
        NormalsIntegrator(const VariantMap& params = VariantMap());
        virtual ~NormalsIntegrator();

        virtual void preprocess(Scene* scene) override;
        virtual Spectrum Li(const Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags flags = Illumination::Full) const override;
    };
}